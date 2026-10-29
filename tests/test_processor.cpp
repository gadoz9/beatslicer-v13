// headless end-to-end test: real JUCE processor + real editor, synthetic sample, snapshot to PNG
#include <cstdio>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

static juce::File makeWav()
{
    const int sr = 44100, N = sr * 3;
    juce::AudioBuffer<float> b(2, N);
    b.clear();
    juce::Random rnd(5);
    const int onsets[] = { 0, 9000, 21000, 36000, 52000, 70000, 95000, 115000 };
    for (int k = 0; k < 8; ++k)
        for (int i = 0; i < 12000 && onsets[k] + i < N; ++i)
        {
            const float e = std::exp(-i / 3500.0f) * (0.4f + 0.6f * rnd.nextFloat());
            const float v = 0.6f * e * std::sin(2.0f * 3.14159f * (180.0f + 70.0f * k) * i / (float) sr) + 0.15f * e * (rnd.nextFloat() - 0.5f);
            for (int c = 0; c < 2; ++c) b.addSample(c, onsets[k] + i, v);
        }
    auto f = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("bs_test.wav");
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> os(f.createOutputStream());
    std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(os.get(), sr, 2, 16, {}, 0));
    os.release();
    w->writeFromAudioSampleBuffer(b, 0, N);
    return f;
}

// v13: a layer sound - `silence` seconds of nothing, then a tone of `freq` Hz for `len` seconds that dies in `decay` seconds
static juce::File makeLayerWav(const char* name, double silence, double freq, double len, double decay)
{
    const int sr = 44100, N = (int) ((silence + len) * sr) + 64, s0 = (int) (silence * sr);
    juce::AudioBuffer<float> b(2, N);
    b.clear();
    for (int i = s0; i < N; ++i)
    {
        const double t = (double) (i - s0) / sr;
        const float v = (float) (0.7 * std::exp(-t / decay) * std::sin(2.0 * 3.14159265 * freq * t));
        for (int c = 0; c < 2; ++c) b.setSample(c, i, v);
    }
    auto f = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(name);
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> os(f.createOutputStream());
    std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(os.get(), sr, 2, 16, {}, 0));
    os.release();
    w->writeFromAudioSampleBuffer(b, 0, N);
    return f;
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    SlicerProcessor p;
    p.setRateAndBufferSizeDetails(44100.0, 512);
    p.prepareToPlay(44100.0, 512);
    if (!p.loadSample(makeWav())) { std::printf("FAIL load\n"); return 1; }
    std::printf("loaded %s %.2fs suggested bars %d\n", p.getSampleName().toRawUTF8(), p.getSampleSeconds(), p.getSuggestedBars());
    auto plan = p.getPlan();
    std::printf("plan steps %d, per bar %d\n", plan->totalSteps(), plan->stepsPerBar);

    // PLAY mode: press a key, expect sound within ~1 s at 120 bpm
    p.keyboardState.noteOn(1, 48, 0.9f);
    float peak = 0.0f;
    juce::AudioBuffer<float> buf(2, 512);
    for (int blk = 0; blk < 150; ++blk)
    {
        juce::MidiBuffer m; buf.clear();
        p.processBlock(buf, m);
        peak = juce::jmax(peak, buf.getMagnitude(0, 512));
    }
    std::printf("PLAY peak %.3f\n", peak);
    if (peak < 0.02f) { std::printf("FAIL silent in PLAY mode\n"); return 1; }
    p.keyboardState.noteOff(1, 48, 0.0f);

    // every rhythm preset should be audible in AUTO mode
    if (auto* m = p.apvts.getParameter("mode")) m->setValueNotifyingHost(1.0f);
    for (int r = 0; r < (int) bs::rhythms().size(); ++r)
    {
        p.applyRhythm(r);
        float pk = 0.0f;
        for (int blk = 0; blk < 200; ++blk) { juce::MidiBuffer m; buf.clear(); p.processBlock(buf, m); pk = juce::jmax(pk, buf.getMagnitude(0, 512)); }
        std::printf("rhythm %-16s peak %.3f\n", bs::rhythms()[(size_t) r].name, pk);
        if (pk < 0.02f) { std::printf("FAIL silent rhythm\n"); return 1; }
    }

    // state round trip
    p.applyRhythm(3);
    p.setStepVel(2, 0.0f);
    juce::MemoryBlock mb; p.getStateInformation(mb);
    SlicerProcessor q; q.setRateAndBufferSizeDetails(44100.0, 512); q.prepareToPlay(44100.0, 512);
    q.setStateInformation(mb.getData(), (int) mb.getSize());
    std::printf("restored sample '%s', step2 vel %.1f, steps %d\n", q.getSampleName().toRawUTF8(), q.getStepVel(2), q.getPlan()->totalSteps());
    if (q.getSampleName().isEmpty() || q.getStepVel(2) != 0.0f) { std::printf("FAIL state\n"); return 1; }

    // ---- version 3: factory presets set the whole sound and are audible
    auto run = [&](SlicerProcessor& pr, int blocks, int note = -1)
    {
        float pk2 = 0.0f;
        if (note >= 0) pr.keyboardState.noteOn(1, note, 0.9f);
        for (int blk = 0; blk < blocks; ++blk) { juce::MidiBuffer m; buf.clear(); pr.processBlock(buf, m); pk2 = juce::jmax(pk2, buf.getMagnitude(0, 512)); }
        if (note >= 0) pr.keyboardState.noteOff(1, note, 0.0f);
        return pk2;
    };
    auto rawv = [&](SlicerProcessor& pr, const char* id) { return pr.apvts.getRawParameterValue(id)->load(); };
    for (int f = 0; f < (int) bs::factoryPresets().size(); ++f)
    {
        p.applyFactoryPreset(f);
        const auto& fp = bs::factoryPresets()[(size_t) f];
        bool knobsOk = (int) rawv(p, "grid") == fp.grid;
        for (auto& k : fp.knobs) knobsOk = knobsOk && std::fabs(rawv(p, k.id) - k.value) < 0.02f * juce::jmax(1.0f, std::fabs(k.value));
        const float pk2 = run(p, 200);
        auto pl = p.getPlan();
        int revs = 0, longs = 0; float maxSemi = 0;
        for (auto& sl : pl->slices) { revs += sl.reverse; longs += sl.len > 1.0f; maxSemi = juce::jmax(maxSemi, std::fabs(sl.semi)); }
        std::printf("factory %-13s name '%s' knobs %s steps %d peak %.3f reversed %d long %d max|semi| %.0f\n", fp.name,
                    p.getRhythmName().toRawUTF8(), knobsOk ? "ok" : "BAD", pl->totalSteps(), pk2, revs, longs, maxSemi);
        if (!knobsOk || pk2 < 0.02f || p.getRhythmName() != fp.name) { std::printf("FAIL factory preset\n"); return 1; }
    }
    {   // Trap Stutter: the roll at the end of the bar repeats one slice
        p.applyFactoryPreset(1);
        auto pl = p.getPlan();
        const int s0 = pl->slices[27].start;
        bool same = true;
        for (int k = 28; k < 32; ++k) same = same && pl->slices[(size_t) k].start == s0;
        std::printf("trap roll repeats slice: %s, step 12 reversed: %d\n", same ? "yes" : "no", (int) pl->slices[12].reverse);
        if (!same || !pl->slices[12].reverse) { std::printf("FAIL trap\n"); return 1; }
    }
    {   // the factory preset survives a grid change and a save / reload
        p.applyFactoryPreset(0);
        juce::MemoryBlock fm; p.getStateInformation(fm);
        SlicerProcessor q2; q2.setRateAndBufferSizeDetails(44100.0, 512); q2.prepareToPlay(44100.0, 512);
        q2.setStateInformation(fm.getData(), (int) fm.getSize());
        std::printf("restored preset '%s', step 14 semi %.0f\n", q2.getRhythmName().toRawUTF8(), q2.getStepExtras().semi[14]);
        if (q2.getRhythmName() != "House Chop" || std::fabs(q2.getStepExtras().semi[14] - 12.0f) > 0.01f) { std::printf("FAIL preset state\n"); return 1; }
    }

    // ---- slip: the slice keeps its place, its audio moves, Ctrl+double-click resets
    {
        p.applyFactoryPreset(0);
        auto pl = p.getPlan();
        const int seg = pl->slices[2].seg, before = pl->slices[2].start, steps = pl->totalSteps();
        p.setSlip(seg, 0.01f);
        auto pl2 = p.getPlan();
        const int after = pl2->slices[2].start;
        const bool segKept = pl2->segments[(size_t) seg] == pl->segments[(size_t) seg];
        juce::MemoryBlock sm2; p.getStateInformation(sm2);
        SlicerProcessor q3; q3.setRateAndBufferSizeDetails(44100.0, 512); q3.prepareToPlay(44100.0, 512);
        q3.setStateInformation(sm2.getData(), (int) sm2.getSize());
        const float restored = q3.getSlip(seg);
        std::printf("slip seg %d: start %d -> %d (expected +%d), steps %d -> %d, box kept %d, restored %.3f\n", seg, before, after,
                    (int) (0.01f * 132300), steps, pl2->totalSteps(), (int) segKept, restored);
        if (std::abs(after - before - 1323) > 2 || steps != pl2->totalSteps() || !segKept || std::fabs(restored - 0.01f) > 1e-4f) { std::printf("FAIL slip\n"); return 1; }
        p.resetSlip(seg);
        if (p.getPlan()->slices[2].start != before) { std::printf("FAIL slip reset\n"); return 1; }
        p.setSlip(seg, 0.004f); // leave one slipped slice for the snapshot
    }

    // ---- filter: HP removes lows, BP keeps the middle, resonance stays stable
    {
        auto toneEnergy = [&](int type, float cutoff, float reso)
        {
            bs::Engine e; e.prepare(44100.0);
            auto sd = std::make_shared<bs::SampleData>(); sd->sr = 44100.0;
            sd->L.resize(44100); sd->R.resize(44100);
            for (int i = 0; i < 44100; ++i) sd->L[(size_t) i] = sd->R[(size_t) i] = 0.5f * std::sin(2.0f * 3.14159265f * 100.0f * i / 44100.0f);
            bs::Params pp; pp.oneShot = 1; pp.filterType = type; pp.cutoff = cutoff; pp.resonance = reso; pp.length = 1.0f; pp.sustain = 1.0f; pp.keytrack = 0;
            float pat[bs::kPatternLen] = {};
            e.setParams(pp); e.setPlan(bs::buildPlan(sd, pp, pat, {}));
            e.setHost(false, false, 0.0, 0.0);
            e.noteOn(60, 1.0f);
            std::vector<float> L(22050), R(22050);
            e.render(L.data(), R.data(), 22050);
            double en = 0; float pk3 = 0;
            for (int i = 4410; i < 22050; ++i) { en += (double) L[(size_t) i] * L[(size_t) i]; pk3 = juce::jmax(pk3, std::fabs(L[(size_t) i])); }
            return std::make_pair(std::sqrt(en / 17640.0), pk3);
        };
        const auto lp = toneEnergy(0, 20000.0f, 0.0f), hp = toneEnergy(1, 2000.0f, 0.0f), bp = toneEnergy(2, 100.0f, 0.0f), rs = toneEnergy(0, 100.0f, 1.0f);
        std::printf("100 Hz tone rms: LP open %.3f, HP 2k %.4f, BP at 100 Hz %.3f, LP 100 Hz reso 100%% peak %.3f\n", lp.first, hp.first, bp.first, rs.second);
        if (hp.first > lp.first * 0.05 || bp.first < lp.first * 0.6 || rs.second > 1.01f || rs.first < lp.first) { std::printf("FAIL filter\n"); return 1; }
    }

    // ---- One Shot: loads as it is, the key plays it at once, Length / Pitch shape it
    {
        SlicerProcessor o; o.setRateAndBufferSizeDetails(44100.0, 512); o.prepareToPlay(44100.0, 512);
        if (auto* m = o.apvts.getParameter("oneshot")) m->setValueNotifyingHost(1.0f);
        if (auto* m = o.apvts.getParameter("bars")) m->setValueNotifyingHost(m->convertTo0to1(2.0f)); // 4 bars, must stay
        const auto wav = makeWav();
        o.loadSample(wav);
        std::printf("one shot: bars kept %s\n", (int) rawv(o, "bars") == 2 ? "yes" : "no");
        if ((int) rawv(o, "bars") != 2) { std::printf("FAIL one shot touched bars\n"); return 1; }
        // sound must start in the very first block after the key
        auto first = [&](float length, float pitch)
        {
            if (auto* m = o.apvts.getParameter("length")) m->setValueNotifyingHost(m->convertTo0to1(length));
            if (auto* m = o.apvts.getParameter("pitch")) m->setValueNotifyingHost(m->convertTo0to1(pitch));
            if (auto* m = o.apvts.getParameter("release")) m->setValueNotifyingHost(m->convertTo0to1(20.0f));
            o.reset();
            juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 110), 0);
            buf.clear(); o.processBlock(buf, m);
            const float pk1 = buf.getMagnitude(0, 512);
            int lastLoud = 0;
            for (int blk = 1; blk < 400; ++blk) { juce::MidiBuffer mm; buf.clear(); o.processBlock(buf, mm); if (buf.getMagnitude(0, 512) > 0.002f) lastLoud = blk; }
            return std::make_pair(pk1, (lastLoud + 1) * 512 / 44100.0);
        };
        const auto full = first(1.0f, 0.0f), half = first(0.5f, 0.0f), up = first(1.0f, 12.0f);
        std::printf("one shot: first block peak %.3f, plays %.2fs at 100%%, %.2fs at Length 50%%, %.2fs at +12 st (sample %.2fs)\n",
                    full.first, full.second, half.second, up.second, o.getSampleSeconds());
        // v13: Length does not cut a one shot any more - it always plays to the right locator (the locators set the length)
        if (full.first < 0.02f || std::fabs(full.second - 3.0) > 0.15 || std::fabs(half.second - 3.0) > 0.15 || std::fabs(up.second - 1.5) > 0.15)
        { std::printf("FAIL one shot\n"); return 1; }
        std::unique_ptr<juce::AudioProcessorEditor> ed1(o.createEditor());
        ed1->setSize(bsui::kBaseW, bsui::kBaseH);
        auto img1 = ed1->createComponentSnapshot(ed1->getLocalBounds(), true, 1.0f);
        juce::File out1(argc > 2 ? argv[2] : "editor_oneshot.png");
        out1.deleteFile();
        juce::FileOutputStream fos1(out1);
        juce::PNGImageFormat().writeImageToStream(img1, fos1);
    }

    // ---- version 4 ------------------------------------------------------------------------
    // the engine's knob ranges must match the real parameters (so the LFO moves knobs correctly)
    for (int d = 0; d < bs::kModCount; ++d)
    {
        auto* prm = p.apvts.getParameter(bs::modInfo(d).id);
        if (prm == nullptr) { std::printf("FAIL no parameter %s\n", bs::modInfo(d).id); return 1; }
        for (float n : { 0.1f, 0.37f, 0.5f, 0.83f })
        {
            const float a = prm->convertFrom0to1(n), b = bs::modFromNorm(d, n);
            const float tol = 1.0e-3f * (bs::modInfo(d).hi - bs::modInfo(d).lo);
            if (std::fabs(a - b) > tol) { std::printf("FAIL range %s %f vs %f\n", bs::modInfo(d).id, a, b); return 1; }
        }
    }
    std::printf("LFO ranges match all %d knobs\n", bs::kModCount);
    {
        // LFO on Volume: Trig restarts with every slice, Free runs along the loop
        auto setP = [&](SlicerProcessor& pr, const char* id, float v) { if (auto* m = pr.apvts.getParameter(id)) m->setValueNotifyingHost(m->convertTo0to1(v)); };
        auto levelsPerStep = [&](int lfoMode)
        {
            bs::Engine e; e.prepare(44100.0);
            auto sd = std::make_shared<bs::SampleData>(); sd->sr = 44100.0;
            sd->L.assign(44100 * 2, 0.0f); sd->R.assign(44100 * 2, 0.0f);
            for (int i = 0; i < 44100 * 2; ++i) sd->L[(size_t) i] = sd->R[(size_t) i] = 0.4f * std::sin(2.0f * 3.14159265f * 220.0f * i / 44100.0f);
            bs::Params pp; pp.mode = 1; pp.grid = 1; pp.bars = 0; pp.pick = 1; pp.length = 1.0f; pp.attackMs = 0.1f; pp.sustain = 1.0f; pp.decayMs = 5.0f; pp.velocity = 0.0f;
            pp.lfoMode = lfoMode; pp.lfoSync = 1; pp.lfoBeats = 0.5f; // one cycle = one 1/8 step
            pp.volume = 0.5f; pp.modAmt[bs::mdVolume] = -0.5f;    // volume dips with the LFO
            float pat[bs::kPatternLen] = {}; for (int i = 0; i < 8; ++i) pat[i] = 1.0f;
            auto table = bs::buildLfoTable(bs::lfoPreset(5));      // decay shape: 1 -> 0
            e.setParams(pp); e.setPlan(bs::buildPlan(sd, pp, pat, {})); e.setLfo(table); e.setHost(false, false, 0.0, 0.0);
            const int stepLen = 11025; std::vector<float> L((size_t) stepLen * 8), R(L.size());
            e.render(L.data(), R.data(), (int) L.size());
            std::vector<double> early, late;
            for (int k = 0; k < 8; ++k)
            {
                double a = 0, b = 0;
                for (int i = 200; i < 1200; ++i) a += std::fabs(L[(size_t) (k * stepLen + i)]);
                for (int i = stepLen - 1200; i < stepLen - 200; ++i) b += std::fabs(L[(size_t) (k * stepLen + i)]);
                early.push_back(a); late.push_back(b);
            }
            return std::make_pair(early, late);
        };
        const auto trig = levelsPerStep(0);
        // Trig: each slice starts quiet (LFO at 1 -> volume dipped) and gets louder as the LFO decays
        int risingSlices = 0;
        for (int k = 0; k < 8; ++k) risingSlices += trig.second[(size_t) k] > trig.first[(size_t) k] * 1.4;
        std::printf("LFO Trig on volume: %d / 8 slices rise inside the slice (early %.0f, late %.0f)\n", risingSlices, trig.first[3], trig.second[3]);
        if (risingSlices < 7) { std::printf("FAIL LFO trig\n"); return 1; }
        const auto env = levelsPerStep(1);
        std::printf("LFO Env: slice 3 early %.0f late %.0f\n", env.first[3], env.second[3]);
        if (env.second[3] < env.first[3] * 1.4) { std::printf("FAIL LFO env\n"); return 1; }
        (void) setP;
    }
    {
        // unison and chorus make a mono sample stereo; unison 1 / chorus 0 keep it mono
        auto width = [&](int voices, float detune, float spread, float choMix)
        {
            bs::Engine e; e.prepare(44100.0);
            auto sd = std::make_shared<bs::SampleData>(); sd->sr = 44100.0;
            sd->L.assign(44100, 0.0f); sd->R.assign(44100, 0.0f);
            for (int i = 0; i < 44100; ++i) sd->L[(size_t) i] = sd->R[(size_t) i] = 0.4f * std::sin(2.0f * 3.14159265f * 330.0f * i / 44100.0f);
            bs::Params pp; pp.oneShot = 1; pp.length = 1.0f; pp.sustain = 1.0f; pp.keytrack = 0;
            pp.uniVoices = voices; pp.uniDetune = detune; pp.uniSpread = spread; pp.choMix = choMix;
            float pat[bs::kPatternLen] = {};
            e.setParams(pp); e.setPlan(bs::buildPlan(sd, pp, pat, {})); e.setHost(false, false, 0.0, 0.0);
            e.noteOn(60, 1.0f);
            std::vector<float> L(22050), R(22050);
            e.render(L.data(), R.data(), 22050);
            double side = 0, mid = 0; float pk = 0;
            for (int i = 2000; i < 22050; ++i) { side += std::fabs(L[(size_t) i] - R[(size_t) i]); mid += std::fabs(L[(size_t) i] + R[(size_t) i]); pk = juce::jmax(pk, std::fabs(L[(size_t) i]), std::fabs(R[(size_t) i])); }
            return std::make_pair(side / juce::jmax(1.0, mid), pk);
        };
        const auto dry = width(1, 20, 0.5f, 0.0f), uni = width(5, 30, 1.0f, 0.0f), cho = width(1, 20, 0.5f, 0.6f);
        std::printf("stereo width (side/mid): dry %.3f, unison x5 %.3f (peak %.2f), chorus 60%% %.3f\n", dry.first, uni.first, uni.second, cho.first);
        if (dry.first > 0.001 || uni.first < 0.05 || cho.first < 0.05 || uni.second > 1.0f) { std::printf("FAIL unison/chorus\n"); return 1; }
    }
    {
        // routing + LFO shape are saved with the project
        p.addMod(bs::mdCutoff); p.setModAmount(bs::mdCutoff, -0.4f);
        p.setLfoPoints(bs::lfoPreset(4));
        juce::MemoryBlock vm; p.getStateInformation(vm);
        SlicerProcessor q4; q4.setRateAndBufferSizeDetails(44100.0, 512); q4.prepareToPlay(44100.0, 512);
        q4.setStateInformation(vm.getData(), (int) vm.getSize());
        std::printf("restored LFO: cutoff %s %.2f, %d points\n", q4.isModOn(bs::mdCutoff) ? "on" : "off", q4.getModAmount(bs::mdCutoff), (int) q4.getLfoPoints().size());
        if (!q4.isModOn(bs::mdCutoff) || std::fabs(q4.getModAmount(bs::mdCutoff) + 0.4f) > 1e-3f || q4.getLfoPoints().size() != 4) { std::printf("FAIL LFO state\n"); return 1; }
        p.clearMods();
    }
    {
        // drag out: the loop renders to a wav of exactly bars x 4 beats, stereo or mono
        p.applyFactoryPreset(0);
        if (auto* m = p.apvts.getParameter("mode")) m->setValueNotifyingHost(1.0f);
        for (bool mono : { false, true })
        {
            const auto f = p.renderToFile(mono);
            std::unique_ptr<juce::AudioFormatReader> rd(juce::WavAudioFormat().createReaderFor(new juce::FileInputStream(f), true));
            if (rd == nullptr) { std::printf("FAIL export unreadable\n"); return 1; }
            juce::AudioBuffer<float> eb((int) rd->numChannels, (int) rd->lengthInSamples);
            rd->read(&eb, 0, (int) rd->lengthInSamples, 0, true, true);
            const int expect = (int) std::round(p.getPlan()->bars * 4.0 * 60.0 / 120.0 * 44100.0);
            std::printf("export %s: %d ch, %d samples (expected %d), peak %.2f, %d bit\n", f.getFileName().toRawUTF8(), (int) rd->numChannels,
                        (int) rd->lengthInSamples, expect, eb.getMagnitude(0, eb.getNumSamples()), (int) rd->bitsPerSample);
            if ((int) rd->numChannels != (mono ? 1 : 2) || (int) rd->lengthInSamples != expect || eb.getMagnitude(0, eb.getNumSamples()) < 0.05f)
            { std::printf("FAIL export\n"); return 1; }
            f.deleteFile();
        }
        if (auto* m = p.apvts.getParameter("mode")) m->setValueNotifyingHost(0.0f);
    }

    // ---- version 5 ------------------------------------------------------------------------
    {
        // Standalone internal clock: a host transport that is stopped must not silence the loop
        auto playStopped = [&](bool internal)
        {
            bs::Engine e; e.prepare(44100.0);
            auto sd = std::make_shared<bs::SampleData>(); sd->sr = 44100.0;
            sd->L.assign(44100 * 2, 0.0f); sd->R.assign(44100 * 2, 0.0f);
            for (int i = 0; i < 44100 * 2; ++i) sd->L[(size_t) i] = sd->R[(size_t) i] = 0.4f * std::sin(2.0f * 3.14159265f * 220.0f * i / 44100.0f);
            bs::Params pp; pp.mode = 0; pp.grid = 1; pp.bars = 0; pp.pick = 1; pp.tempo = 90.0f;
            float pat[bs::kPatternLen] = {}; for (int i = 0; i < 8; ++i) pat[i] = 1.0f;
            e.setParams(pp); e.setPlan(bs::buildPlan(sd, pp, pat, {}));
            e.setInternalClock(internal);
            e.setHost(true, false, 0.0, 120.0);   // like a standalone wrapper with a play head that never plays
            e.noteOn(60, 1.0f);
            std::vector<float> L(44100), R(44100);
            e.render(L.data(), R.data(), 44100);
            float pk = 0; for (float v : L) pk = juce::jmax(pk, std::fabs(v));
            return std::make_pair(pk, e.bpm());
        };
        const auto inHost = playStopped(false), inStandalone = playStopped(true);
        std::printf("stopped transport: host clock peak %.3f, internal clock peak %.3f at %.0f bpm\n", inHost.first, inStandalone.first, inStandalone.second);
        if (inHost.first > 0.001f || inStandalone.first < 0.05f || std::fabs(inStandalone.second - 90.0) > 0.01) { std::printf("FAIL internal clock\n"); return 1; }
        // Tempo moves in whole BPM steps
        auto* tp = p.apvts.getParameter("tempo");
        tp->setValueNotifyingHost(tp->convertTo0to1(97.4f));
        std::printf("tempo snaps to %.1f\n", p.apvts.getRawParameterValue("tempo")->load());
        if (std::fabs(p.apvts.getRawParameterValue("tempo")->load() - 97.0f) > 0.01f) { std::printf("FAIL tempo steps\n"); return 1; }
        tp->setValueNotifyingHost(tp->convertTo0to1(120.0f));
    }
    {
        // dots in dB: top = 0 dB, Velocity Depth scales the dB
        const float d = bs::dbToDot(-20.0f);
        const float full = 20.0f * std::log10(bs::sliceLevel(d, 1.0f)), half = 20.0f * std::log10(bs::sliceLevel(d, 0.5f));
        std::printf("dot -20 dB: depth 100%% %.1f dB, depth 50%% %.1f dB, depth 0%% %.2f, top %.1f dB\n", full, half, bs::sliceLevel(d, 0.0f),
                    bs::dotToDb(bs::dbToDot(3.0f)));
        if (std::fabs(full + 20.0f) > 0.05f || std::fabs(half + 10.0f) > 0.05f || std::fabs(bs::sliceLevel(d, 0.0f) - 1.0f) > 1e-4f
            || bs::dotToDb(bs::dbToDot(3.0f)) > 0.0f) { std::printf("FAIL dot dB\n"); return 1; }
        // a quiet dot survives a save / reload
        p.applyRhythm(0);
        p.setStepVel(0, bs::dbToDot(-41.3f));
        juce::MemoryBlock dm; p.getStateInformation(dm);
        SlicerProcessor q5; q5.setRateAndBufferSizeDetails(44100.0, 512); q5.prepareToPlay(44100.0, 512);
        q5.setStateInformation(dm.getData(), (int) dm.getSize());
        std::printf("restored dot %.2f dB\n", bs::dotToDb(q5.getStepVel(0)));
        if (std::fabs(bs::dotToDb(q5.getStepVel(0)) + 41.3f) > 0.1f) { std::printf("FAIL dot state\n"); return 1; }
        // master reaches +12 dB
        auto* mp = p.apvts.getParameter("master");
        std::printf("master range %.0f .. %.0f dB\n", mp->convertFrom0to1(0.0f), mp->convertFrom0to1(1.0f));
        if (std::fabs(mp->convertFrom0to1(1.0f) - 12.0f) > 0.01f) { std::printf("FAIL master range\n"); return 1; }
    }

    // ---- version 6 ------------------------------------------------------------------------
    {
        auto toneSample = [&]()
        {
            auto sd = std::make_shared<bs::SampleData>(); sd->sr = 44100.0;
            sd->L.assign(44100 * 2, 0.0f); sd->R.assign(44100 * 2, 0.0f);
            juce::Random rr(3);
            for (int i = 0; i < 44100 * 2; ++i) sd->L[(size_t) i] = sd->R[(size_t) i] = 0.3f * std::sin(2.0f * 3.14159265f * 220.0f * i / 44100.0f) + 0.1f * (rr.nextFloat() - 0.5f);
            return sd;
        };
        auto peakOf = [](bs::Engine& e, int samples)
        {
            std::vector<float> L((size_t) samples), R((size_t) samples);
            for (int pos = 0; pos < samples; pos += 512) e.render(L.data() + pos, R.data() + pos, juce::jmin(512, samples - pos));
            float pk = 0; for (float v : L) pk = juce::jmax(pk, std::fabs(v));
            return pk;
        };
        // LOOP: one press starts, release keeps it going, the next press stops, the press after starts again
        bs::Engine e; e.prepare(44100.0);
        bs::Params pp; pp.mode = 0; pp.grid = 1; pp.bars = 0; pp.pick = 1; pp.latch = 1; pp.releaseMs = 10.0f;
        float pat[bs::kPatternLen] = {}; for (int i = 0; i < 8; ++i) pat[i] = 1.0f;
        auto sd = toneSample();
        e.setParams(pp); e.setPlan(bs::buildPlan(sd, pp, pat, {})); e.setInternalClock(true); e.setHost(false, false, 0.0, 0.0);
        const float idle = peakOf(e, 8192);
        const int idleStep = e.liveStep(); const float idleLfo = e.lfoPhase();
        e.noteOn(60, 1.0f); e.noteOff(60);                // press and let go
        const float looping = peakOf(e, 44100);
        const bool activeWhileLooping = e.isActive();
        const int liveWhileLooping = e.liveStep();
        e.noteOn(60, 1.0f); e.noteOff(60);                // press again = stop
        peakOf(e, 22050);                                 // let the last slice ring out
        const float stopped = peakOf(e, 22050);
        const int liveStopped = e.liveStep();
        e.noteOn(62, 1.0f); e.noteOff(62);                // and again = starts from the top
        const float again = peakOf(e, 22050);
        std::printf("LOOP: idle %.3f (step %d, lfo %.2f), latched after release %.3f (active %d, step %d), stopped %.4f (step %d), again %.3f\n",
                    idle, idleStep, idleLfo, looping, (int) activeWhileLooping, liveWhileLooping, stopped, liveStopped, again);
        if (idle > 0.001f || idleStep != -1 || idleLfo != 0.0f || looping < 0.05f || !activeWhileLooping || liveWhileLooping < 0
            || stopped > 0.001f || liveStopped != -1 || again < 0.05f) { std::printf("FAIL loop latch\n"); return 1; }
        // LOOP off: only while the key is held
        pp.latch = 0; e.setParams(pp);
        peakOf(e, 512);
        const bool dropped = !e.isLatched();
        e.noteOn(60, 1.0f);
        const float held = peakOf(e, 22050);
        e.noteOff(60);
        peakOf(e, 22050);
        const float released = peakOf(e, 22050);
        std::printf("LOOP off: latch dropped %d, held %.3f, released %.4f, step after release %d\n", (int) dropped, held, released, e.liveStep());
        if (!dropped || held < 0.05f || released > 0.001f || e.liveStep() != -1) { std::printf("FAIL loop off\n"); return 1; }

        // WAH: only steps marked W change, amount 0 = no change
        auto renderWah = [&](float amt, bool mark)
        {
            bs::Engine w; w.prepare(44100.0);
            bs::Params q; q.mode = 1; q.grid = 1; q.bars = 0; q.pick = 1; q.sustain = 1.0f; q.length = 1.0f; q.wahAmt = amt; q.wahSpeed = 0.5f;
            bs::StepExtras ex; if (mark) ex.wah[0] = 1;
            w.setParams(q); w.setPlan(bs::buildPlan(sd, q, pat, {}, &ex)); w.setHost(false, false, 0.0, 0.0);
            std::vector<float> L(22050), R(22050);
            w.render(L.data(), R.data(), 22050);
            return L;
        };
        const auto dry = renderWah(0.0f, true), wet = renderWah(0.8f, true), unmarked = renderWah(0.8f, false);
        double dStep0 = 0, dStep1 = 0, dUn = 0; float wpk = 0;
        for (int i = 0; i < 11025; ++i) dStep0 += std::fabs(wet[(size_t) i] - dry[(size_t) i]);
        for (int i = 11025 + 4410; i < 22050; ++i) dStep1 += std::fabs(wet[(size_t) i] - dry[(size_t) i]); // after the W slice has rung out
        for (int i = 0; i < 22050; ++i) { dUn += std::fabs(unmarked[(size_t) i] - dry[(size_t) i]); wpk = juce::jmax(wpk, std::fabs(wet[(size_t) i])); }
        std::printf("WAH: change on W step %.1f, on the next (unmarked) step %.4f, nothing marked %.4f, peak %.2f\n", dStep0, dStep1, dUn, wpk);
        if (dStep0 < 50.0 || dStep1 > 0.01 || dUn > 0.01 || wpk > 1.0f) { std::printf("FAIL wah\n"); return 1; }

        // W flag, Wah knob and LOOP are saved with the project
        p.applyRhythm(1);
        p.setStepWah(3, true);
        if (auto* m = p.apvts.getParameter("wah")) m->setValueNotifyingHost(0.4f);
        if (auto* m = p.apvts.getParameter("latch")) m->setValueNotifyingHost(1.0f);
        juce::MemoryBlock wm; p.getStateInformation(wm);
        SlicerProcessor q6; q6.setRateAndBufferSizeDetails(44100.0, 512); q6.prepareToPlay(44100.0, 512);
        q6.setStateInformation(wm.getData(), (int) wm.getSize());
        std::printf("restored: step 4 wah %d, slice wah %d, wah %.2f, loop %d\n", (int) q6.getStepExtras().wah[3], (int) q6.getPlan()->slices[3].wah,
                    rawv(q6, "wah"), (int) (rawv(q6, "latch") > 0.5f));
        if (!q6.getStepExtras().wah[3] || !q6.getPlan()->slices[3].wah || std::fabs(rawv(q6, "wah") - 0.4f) > 0.01f || rawv(q6, "latch") < 0.5f)
        { std::printf("FAIL v6 state\n"); return 1; }
        // factory presets start without wah
        p.applyFactoryPreset(0);
        bool anyW = false; for (auto w : p.getStepExtras().wah) anyW = anyW || w;
        if (anyW || rawv(p, "wah") > 0.001f) { std::printf("FAIL factory wah\n"); return 1; }
        if (auto* m = p.apvts.getParameter("latch")) m->setValueNotifyingHost(0.0f);

        // CLEAR: the waveform is empty and silent, a new sample loads normally afterwards
        SlicerProcessor c; c.setRateAndBufferSizeDetails(44100.0, 512); c.prepareToPlay(44100.0, 512);
        c.loadSample(makeWav());
        c.keyboardState.noteOn(1, 48, 0.9f);
        const float before = run(c, 60);
        c.clearSample();
        const float after = run(c, 60);
        const bool empty = c.getPlan()->sample == nullptr && c.getSampleName().isEmpty();
        c.keyboardState.noteOff(1, 48, 0.0f);
        c.loadSample(makeWav());
        const float reloaded = run(c, 100, 48);
        std::printf("CLEAR: before %.3f, after %.4f, empty %d, reloaded %.3f\n", before, after, (int) empty, reloaded);
        if (before < 0.02f || after > 0.0001f || !empty || reloaded < 0.02f) { std::printf("FAIL clear\n"); return 1; }

        // knob feel: 235 px for the whole range (v5 200 px = 85 %% speed), Shift 0.15x
        std::printf("knob: %.0f px full range (%.0f%% of v5 speed), Shift x%.2f\n", bsui::KnobSlider::kPixelsFullRange,
                    100.0f * 200.0f / bsui::KnobSlider::kPixelsFullRange, bsui::KnobSlider::kShiftFine);
    }

    // ---- version 7 ------------------------------------------------------------------------
    {
        // Copy to next / all dots: the targets switch on and play exactly the same chop
        // v9: Default resets everything (sample too), so load the sample again and go to 1/16: every dot is on
        p.applyCustom();
        p.loadSample(makeWav());
        if (auto* m = p.apvts.getParameter("grid")) m->setValueNotifyingHost(m->convertTo0to1(2.0f)); // 1/16
        std::printf("custom: name '%s', all 16 on %d\n", p.getRhythmName().toRawUTF8(), (int) (p.getStepVel(15) > 0.99f));
        if (p.getRhythmName() != "Custom" || p.getStepVel(15) < 0.99f) { std::printf("FAIL custom\n"); return 1; }
        p.setStepVel(1, 0.0f);
        p.setStepWah(0, true);
        p.setStepSemi(0, 5.0f);
        const auto before = p.getPlan();
        p.copyStepToNext(0);
        auto pl = p.getPlan();
        const bool nextOk = p.getStepVel(1) > 0.99f && pl->slices[1].start == pl->slices[0].start && pl->slices[1].end == pl->slices[0].end
                            && pl->slices[1].wah && std::fabs(pl->slices[1].semi - pl->slices[0].semi) < 0.01f && pl->slices[2].start == before->slices[2].start;
        p.copyStepToNext(1); // continues the chain: dot 3 also plays the chop of dot 1
        pl = p.getPlan();
        const bool chainOk = pl->slices[2].start == pl->slices[0].start;
        p.copyStepToAll(0);
        pl = p.getPlan();
        int same = 0;
        for (int i = 0; i < pl->totalSteps(); ++i) same += pl->slices[(size_t) i].seg == pl->slices[(size_t) (i / 16 * 16)].seg;
        std::printf("copy: next %d, chain %d, all -> %d / %d steps play the first chop of their bar\n", (int) nextOk, (int) chainOk, same, pl->totalSteps());
        if (!nextOk || !chainOk || same != pl->totalSteps()) { std::printf("FAIL copy\n"); return 1; }
        // the copy survives save / reload, and a dot can change its own pitch afterwards
        p.setStepSemi(4, -7.0f);
        juce::MemoryBlock cm; p.getStateInformation(cm);
        SlicerProcessor q7; q7.setRateAndBufferSizeDetails(44100.0, 512); q7.prepareToPlay(44100.0, 512);
        q7.setStateInformation(cm.getData(), (int) cm.getSize());
        auto ql = q7.getPlan();
        std::printf("copy restored: step 9 plays chop of step 1 %d, step 5 pitch %.0f\n", (int) (ql->slices[8].start == ql->slices[0].start), ql->slices[4].semi);
        if (ql->slices[8].start != ql->slices[0].start || std::fabs(ql->slices[4].semi + 7.0f) > 0.01f) { std::printf("FAIL copy state\n"); return 1; }

        // noise types: every one is audible, none clips, white stays the v6 noise
        for (int t = 0; t < bs::kNoiseTypes; ++t)
        {
            bs::Engine e; e.prepare(44100.0);
            auto sd = std::make_shared<bs::SampleData>(); sd->sr = 44100.0;
            sd->L.assign(44100 * 2, 0.0f); sd->R.assign(44100 * 2, 0.0f); // silent sample: only the noise is heard
            bs::Params q; q.oneShot = 1; q.length = 1.0f; q.sustain = 1.0f; q.noise = 1.0f; q.noiseType = t; q.keytrack = 0;
            float pt[bs::kPatternLen] = {};
            e.setParams(q); e.setPlan(bs::buildPlan(sd, q, pt, {})); e.setHost(false, false, 0.0, 0.0);
            e.noteOn(60, 1.0f);
            std::vector<float> L(44100), R(44100);
            e.render(L.data(), R.data(), 44100);
            double en = 0; float pk = 0;
            for (int i = 2000; i < 44100; ++i) { en += (double) L[(size_t) i] * L[(size_t) i]; pk = juce::jmax(pk, std::fabs(L[(size_t) i])); }
            const double rms = std::sqrt(en / 42100.0);
            std::printf("noise %-8s rms %.3f peak %.3f\n", bs::noiseTypeName(t), rms, pk);
            if (rms < 0.03 || pk > 0.95f || !std::isfinite(rms)) { std::printf("FAIL noise type\n"); return 1; }
        }

        // ADD LAYER: blended into every chop, restarts with each chop, Mix 0 = gone, saved with the project
        auto layerFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("bs_layer.wav");
        {
            juce::AudioBuffer<float> lb(2, 22050);
            for (int i = 0; i < 22050; ++i) for (int c = 0; c < 2; ++c) lb.setSample(c, i, i < 2000 ? 0.8f * std::sin(i * 0.3f) : 0.0f); // a click at the start
            layerFile.deleteFile();
            std::unique_ptr<juce::FileOutputStream> os(layerFile.createOutputStream());
            std::unique_ptr<juce::AudioFormatWriter> w(juce::WavAudioFormat().createWriterFor(os.get(), 44100, 2, 16, {}, 0));
            os.release(); w->writeFromAudioSampleBuffer(lb, 0, 22050);
        }
        auto layerRun = [&](float mix)
        {
            bs::Engine e; e.prepare(44100.0);
            auto sd = std::make_shared<bs::SampleData>(); sd->sr = 44100.0;
            sd->L.assign(44100 * 2, 0.0f); sd->R.assign(44100 * 2, 0.0f);
            auto ld = std::make_shared<bs::SampleData>(); ld->sr = 44100.0;
            ld->L.assign(22050, 0.0f); for (int i = 0; i < 2000; ++i) ld->L[(size_t) i] = 0.8f * std::sin(i * 0.3f);
            ld->R = ld->L;
            bs::Params q; q.mode = 1; q.grid = 1; q.bars = 0; q.pick = 1; q.sustain = 1.0f; q.length = 1.0f; q.layerMix = mix;
            float pt[bs::kPatternLen] = {}; for (int i = 0; i < 8; ++i) pt[i] = 1.0f;
            e.setParams(q); e.setPlan(bs::buildPlan(sd, q, pt, {})); e.setLayer(ld); e.setHost(false, false, 0.0, 0.0);
            std::vector<float> L(11025 * 4), R(L.size());
            e.render(L.data(), R.data(), (int) L.size());
            int clicks = 0;
            for (int k = 0; k < 4; ++k) { float pk = 0; for (int i = 0; i < 2000; ++i) pk = juce::jmax(pk, std::fabs(L[(size_t) (k * 11025 + i)])); clicks += pk > 0.1f; }
            float tail = 0; for (int i = 4000; i < 11000; ++i) tail = juce::jmax(tail, std::fabs(L[(size_t) i]));
            return std::make_pair(clicks, tail);
        };
        const auto on = layerRun(0.8f), off = layerRun(0.0f);
        std::printf("layer: click heard at the start of %d / 4 chops (tail %.4f), mix 0 -> %d\n", on.first, on.second, off.first);
        if (on.first != 4 || on.second > 0.01f || off.first != 0) { std::printf("FAIL layer\n"); return 1; }
        p.loadLayer(layerFile);
        if (auto* m = p.apvts.getParameter("layermix")) m->setValueNotifyingHost(0.35f);
        if (auto* m = p.apvts.getParameter("noisetype")) m->setValueNotifyingHost(m->convertTo0to1(3.0f));
        juce::MemoryBlock lm; p.getStateInformation(lm);
        SlicerProcessor q8; q8.setRateAndBufferSizeDetails(44100.0, 512); q8.prepareToPlay(44100.0, 512);
        q8.setStateInformation(lm.getData(), (int) lm.getSize());
        std::printf("layer restored %d, mix %.2f, noise type %s\n", (int) (q8.getLayer() != nullptr), rawv(q8, "layermix"), bs::noiseTypeName((int) rawv(q8, "noisetype")));
        if (q8.getLayer() == nullptr || std::fabs(rawv(q8, "layermix") - 0.35f) > 0.01f || (int) rawv(q8, "noisetype") != 3) { std::printf("FAIL layer state\n"); return 1; }
        q8.clearLayer();
        if (q8.getLayer() != nullptr) { std::printf("FAIL layer clear\n"); return 1; }
    }

    // ------------------------------------------------------------------ v8: BALAGAN + panning
    {
        // a sample of 16 even chops, each one starts with a short click and is silent after it
        auto sd = std::make_shared<bs::SampleData>(); sd->sr = 44100.0;
        const int N = 88200;
        sd->L.assign((size_t) N, 0.0f);
        for (int k = 0; k < 16; ++k) for (int i = 0; i < 300; ++i) sd->L[(size_t) (k * N / 16 + i)] = 0.8f * std::sin(i * 0.4f);
        sd->R = sd->L;
        auto run = [&](bool bal, float speed, bool super, float pan, float balFilter, std::shared_ptr<bs::SampleData> smp, int balMode = 0)
        {
            bs::Engine e; e.prepare(44100.0);
            bs::Params q; q.mode = 1; q.grid = 2; q.bars = 0; q.pick = 1; q.sustain = 1.0f; q.length = 1.0f; q.releaseMs = 5.0f;
            q.balagan = speed; q.balSuper = super ? 1 : 0; q.balFilter = balFilter; q.balMode = balMode;
            float pt[bs::kPatternLen] = {}; for (int i = 0; i < 16; ++i) pt[i] = 1.0f;
            bs::StepExtras ex; ex.bal[0] = bal ? 1 : 0; ex.pan[0] = pan;
            e.setParams(q); e.setPlan(bs::buildPlan(smp, q, pt, {}, &ex)); e.setHost(false, false, 0.0, 0.0);
            std::vector<float> L(5512 * 2), R(L.size());
            e.render(L.data(), R.data(), (int) L.size());
            return std::make_pair(L, R);
        };
        auto clicks = [](const std::vector<float>& x, int from, int to)
        {
            int c = 0, quiet = 1000;
            for (int i = from; i < to; ++i)
            {
                if (std::fabs(x[(size_t) i]) > 0.1f) { if (quiet > 150) ++c; quiet = 0; }
                else ++quiet;
            }
            return c;
        };
        const auto normal = run(false, 50.0f, false, 0.0f, 0.0f, sd);
        const auto b25 = run(true, 25.0f, false, 0.0f, 0.0f, sd), b50 = run(true, 50.0f, false, 0.0f, 0.0f, sd);
        const auto sup = run(true, 50.0f, true, 0.0f, 0.0f, sd);
        const int cn = clicks(normal.first, 0, 5500), c25 = clicks(b25.first, 0, 5500), c50 = clicks(b50.first, 0, 5500);
        const int cs = clicks(sup.first, 0, 5500), nextStep = clicks(b50.first, 5512, 11000);
        // v12: FREE runs on a curve (more room for the slow / middle speeds); BPM lands on note values
        const int p25 = clicks(run(true, 25.0f, false, 0.0f, 0.0f, sd, 1).first, 0, 5500), p50 = clicks(run(true, 50.0f, false, 0.0f, 0.0f, sd, 1).first, 0, 5500);
        const int p30 = clicks(run(true, 30.0f, false, 0.0f, 0.0f, sd, 1).first, 0, 5500), p19 = clicks(run(true, 19.0f, false, 0.0f, 0.0f, sd, 1).first, 0, 5500);
        std::printf("balagan FREE: normal step %d hit, knob 25 -> %d hits, knob 50 -> %d hits, next step %d, SUPER: %d hits from the chop start\n",
                    cn, c25, c50, nextStep, cs);
        std::printf("balagan BPM (1/16 step): 1/16T (19) -> %d, 1/32 (25) -> %d, 1/32T (30) -> %d, 1/64 (50) -> %d hits\n", p19, p25, p30, p50);
        if (cn != 1 || c25 != 2 || c50 != 3 || nextStep != 1 || cs >= c50) { std::printf("FAIL balagan\n"); return 1; }
        if (p19 != 2 || p25 != 2 || p30 != 3 || p50 != 4) { std::printf("FAIL balagan BPM\n"); return 1; }
        // knob 100 is close to a buzz: the hits are about 8 ms apart at 120 BPM
        {
            bs::Engine e; e.prepare(44100.0);
            auto tone = std::make_shared<bs::SampleData>(); tone->sr = 44100.0;
            tone->L.assign((size_t) N, 0.0f);
            for (int i = 0; i < N; ++i) tone->L[(size_t) i] = 0.5f * std::sin(i * 0.05f);
            tone->R = tone->L;
            const auto fast = run(true, 100.0f, false, 0.0f, 0.0f, tone);
            float pk = 0; bool finite = true;
            for (float v : fast.first) { pk = juce::jmax(pk, std::fabs(v)); finite = finite && std::isfinite(v); }
            std::printf("balagan 100: peak %.3f\n", pk);
            if (!finite || pk < 0.05f || pk > 0.95f) { std::printf("FAIL balagan 100\n"); return 1; }
            // Filter -> + : the roll starts dark and opens up (more high end at the end of the step than at the start)
            auto noise = std::make_shared<bs::SampleData>(); noise->sr = 44100.0;
            juce::Random nr(3);
            noise->L.resize((size_t) N); for (auto& v : noise->L) v = 0.5f * (nr.nextFloat() - 0.5f);
            noise->R = noise->L;
            auto hi = [](const std::vector<float>& x, int a, int b) { double e2 = 0; for (int i = a + 1; i < b; ++i) e2 += std::pow(x[(size_t) i] - x[(size_t) i - 1], 2.0); return e2; };
            const auto open = run(true, 25.0f, false, 0.0f, 1.0f, noise), close = run(true, 25.0f, false, 0.0f, -1.0f, noise);
            const double o1 = hi(open.first, 100, 2000), o2 = hi(open.first, 2856, 4800), c1 = hi(close.first, 100, 2000), c2 = hi(close.first, 2856, 4800);
            std::printf("balagan filter: + start %.1f -> end %.1f, - start %.1f -> end %.1f\n", o1, o2, c1, c2);
            if (!(o2 > o1 * 1.5) || !(c1 > c2 * 1.5)) { std::printf("FAIL balagan filter\n"); return 1; }
        }
        // panning: hard left = nothing on the right, half right = the left at half
        {
            const auto left = run(false, 50.0f, false, -1.0f, 0.0f, sd), half = run(false, 50.0f, false, 0.5f, 0.0f, sd);
            float l1 = 0, r1 = 0, l2 = 0, r2 = 0;
            for (int i = 0; i < 2000; ++i)
            {
                l1 = juce::jmax(l1, std::fabs(left.first[(size_t) i])); r1 = juce::jmax(r1, std::fabs(left.second[(size_t) i]));
                l2 = juce::jmax(l2, std::fabs(half.first[(size_t) i])); r2 = juce::jmax(r2, std::fabs(half.second[(size_t) i]));
            }
            std::printf("pan: hard left L %.3f R %.3f, half right L %.3f R %.3f\n", l1, r1, l2, r2);
            if (r1 > 0.001f || l1 < 0.1f || std::fabs(l2 / r2 - 0.5f) > 0.05f) { std::printf("FAIL pan\n"); return 1; }
        }
        // processor: red dots + pan are saved with the project, copied with Copy to next, cleared by factory presets
        p.applyCustom();
        p.loadSample(makeWav()); // v9: Default also empties the waveform
        if (auto* m = p.apvts.getParameter("grid")) m->setValueNotifyingHost(m->convertTo0to1(2.0f));
        p.setStepBal(3, true); p.setStepPan(3, -0.4f); p.setStepPan(5, 0.7f);
        p.copyStepToNext(3);
        if (auto* m = p.apvts.getParameter("balagan")) m->setValueNotifyingHost(m->convertTo0to1(70.0f));
        if (auto* m = p.apvts.getParameter("balsuper")) m->setValueNotifyingHost(1.0f);
        juce::MemoryBlock bm; p.getStateInformation(bm);
        SlicerProcessor q9; q9.setRateAndBufferSizeDetails(44100.0, 512); q9.prepareToPlay(44100.0, 512);
        q9.setStateInformation(bm.getData(), (int) bm.getSize());
        const auto& qe = q9.getStepExtras();
        auto qp = q9.getPlan();
        std::printf("balagan state: dot4 bal %d pan %.2f, copy dot5 bal %d pan %.2f, dot6 pan %.2f, knob %.0f super %d, plan bal %d\n", (int) qe.bal[3], qe.pan[3],
                    (int) qe.bal[4], qe.pan[4], qe.pan[5], rawv(q9, "balagan"), (int) rawv(q9, "balsuper"), (int) qp->slices[3].bal);
        if (!qe.bal[3] || std::fabs(qe.pan[3] + 0.4f) > 0.01f || !qe.bal[4] || std::fabs(qe.pan[4] + 0.4f) > 0.01f || std::fabs(qe.pan[5] - 0.7f) > 0.01f
            || std::fabs(rawv(q9, "balagan") - 70.0f) > 0.5f || rawv(q9, "balsuper") < 0.5f || !qp->slices[3].bal) { std::printf("FAIL balagan state\n"); return 1; }
        // the LFO can be dragged onto the BALAGAN knob and its Filter
        q9.addMod(bs::mdBalagan); q9.addMod(bs::mdBalFilter);
        if (!q9.isModOn(bs::mdBalagan) || !q9.isModOn(bs::mdBalFilter)) { std::printf("FAIL balagan lfo\n"); return 1; }
        q9.applyFactoryPreset(0);
        bool clean = true; for (int i = 0; i < bs::kPatternLen; ++i) clean = clean && !q9.getStepExtras().bal[i] && q9.getStepExtras().pan[i] == 0.0f;
        if (!clean) { std::printf("FAIL factory clears balagan\n"); return 1; }
        // a whole loop with red dots in the real processor: audible, no clipping
        q9.applyCustom(); q9.loadSample(makeWav()); // v9: Default also empties the waveform and resets every knob
        if (auto* m = q9.apvts.getParameter("mode")) m->setValueNotifyingHost(1.0f);
        if (auto* m = q9.apvts.getParameter("grid")) m->setValueNotifyingHost(m->convertTo0to1(2.0f));
        q9.setStepBal(2, true); q9.setStepBal(10, true);
        float pk9 = 0;
        for (int blk = 0; blk < 120; ++blk) { juce::MidiBuffer m; buf.clear(); q9.processBlock(buf, m); pk9 = juce::jmax(pk9, buf.getMagnitude(0, 512)); }
        std::printf("balagan loop peak %.3f\n", pk9);
        if (pk9 < 0.02f || pk9 > 1.0f) { std::printf("FAIL balagan loop\n"); return 1; }
    }

    // ------------------------------------------------------------------ v9
    {
        // 1) every key press starts the loop from the first dot (LOOP off and on), also in a host that is playing
        auto firstChop = [&](SlicerProcessor& pr, bool host)
        {
            struct Head : juce::AudioPlayHead
            {
                double ppq = 0.0;
                juce::Optional<PositionInfo> getPosition() const override
                {
                    PositionInfo i; i.setIsPlaying(true); i.setBpm(120.0); i.setPpqPosition(ppq); return i;
                }
            };
            static Head head;
            pr.setPlayHead(host ? &head : nullptr);
            std::vector<int> starts;
            for (int press = 0; press < 3; ++press)
            {
                // wait an odd amount of time, press, listen for the first chop
                for (int b = 0; b < 7 + press * 5; ++b) { juce::MidiBuffer m; buf.clear(); pr.processBlock(buf, m); head.ppq += 512.0 / 44100.0 * 2.0; }
                juce::MidiBuffer on; on.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
                buf.clear(); pr.processBlock(buf, on); head.ppq += 512.0 / 44100.0 * 2.0;
                int at = -1;
                for (int i = 0; i < 512 && at < 0; ++i) if (std::fabs(buf.getSample(0, i)) > 0.01f) at = i;
                starts.push_back(at);
                for (int b = 0; b < 3; ++b) { juce::MidiBuffer m; buf.clear(); pr.processBlock(buf, m); head.ppq += 512.0 / 44100.0 * 2.0; }
                juce::MidiBuffer off; off.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
                if (rawv(pr, "latch") > 0.5f) off.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 1); // LOOP: second press stops
                buf.clear(); pr.processBlock(buf, off); head.ppq += 512.0 / 44100.0 * 2.0;
                for (int b = 0; b < 30; ++b) { juce::MidiBuffer m; buf.clear(); pr.processBlock(buf, m); head.ppq += 512.0 / 44100.0 * 2.0; }
            }
            pr.setPlayHead(nullptr);
            return starts;
        };
        SlicerProcessor r9; r9.setRateAndBufferSizeDetails(44100.0, 512); r9.prepareToPlay(44100.0, 512);
        r9.loadSample(makeWav());
        r9.applyRhythm(0);
        for (bool host : { false, true })
            for (float latch : { 0.0f, 1.0f })
            {
                if (auto* m = r9.apvts.getParameter("latch")) m->setValueNotifyingHost(latch);
                const auto st = firstChop(r9, host);
                std::printf("restart (%s, LOOP %s): first sound at samples %d %d %d after the press\n", host ? "host playing" : "no host", latch > 0.5f ? "on" : "off", st[0], st[1], st[2]);
                for (int a : st) if (a < 0 || a > 200) { std::printf("FAIL loop restart\n"); return 1; }
            }

        // 2) MAIN MIX lowers only the main sample, the layer keeps its level
        auto mixRun = [&](float mainMix, bool withLayer)
        {
            bs::Engine e; e.prepare(44100.0);
            auto sd = std::make_shared<bs::SampleData>(); sd->sr = 44100.0;
            sd->L.assign(88200, 0.0f); for (int i = 0; i < 88200; ++i) sd->L[(size_t) i] = 0.5f * std::sin(i * 0.05f);
            sd->R = sd->L;
            auto ld = std::make_shared<bs::SampleData>(); ld->sr = 44100.0;
            ld->L.assign(22050, 0.0f); for (int i = 0; i < 22050; ++i) ld->L[(size_t) i] = 0.5f * std::sin(i * 0.31f);
            ld->R = ld->L;
            bs::Params q; q.mode = 1; q.grid = 1; q.bars = 0; q.pick = 1; q.sustain = 1.0f; q.length = 1.0f; q.layerMix = 1.0f; q.mainMix = mainMix;
            float pt[bs::kPatternLen] = {}; for (int i = 0; i < 8; ++i) pt[i] = 1.0f;
            e.setParams(q); e.setPlan(bs::buildPlan(sd, q, pt, {})); if (withLayer) e.setLayer(ld); e.setHost(false, false, 0.0, 0.0);
            std::vector<float> L(11025), R(L.size());
            e.render(L.data(), R.data(), (int) L.size());
            double en = 0; for (int i = 500; i < 5000; ++i) en += (double) L[(size_t) i] * L[(size_t) i];
            return std::sqrt(en / 4500.0);
        };
        const double full = mixRun(1.0f, false), half = mixRun(0.5f, false), zero = mixRun(0.0f, false);
        const double layerOnly = mixRun(0.0f, true), layerAlone = mixRun(0.0f, true);
        std::printf("main mix: 100%% %.3f, 50%% %.3f, 0%% %.4f, 0%% + layer %.3f\n", full, half, zero, layerOnly);
        if (std::fabs(half / full - 0.5) > 0.03 || zero > 1.0e-4 || layerOnly < 0.05 || std::fabs(layerOnly - layerAlone) > 1.0e-6) { std::printf("FAIL main mix\n"); return 1; }

        // 3) Default - start from scratch resets the whole plugin
        SlicerProcessor d9; d9.setRateAndBufferSizeDetails(44100.0, 512); d9.prepareToPlay(44100.0, 512);
        d9.loadSample(makeWav());
        d9.applyFactoryPreset(1);
        d9.addMod(bs::mdCutoff); d9.setStepBal(2, true); d9.setStepPan(3, 0.5f); d9.setStepWah(4, true);
        d9.loadLayer(juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("bs_layer.wav"));
        for (auto [id, v] : std::vector<std::pair<const char*, float>> { { "chomix", 0.5f }, { "wah", 0.4f }, { "mainmix", 0.3f }, { "latch", 1.0f }, { "balagan", 80.0f }, { "noise", 0.3f } })
            if (auto* m = d9.apvts.getParameter(id)) m->setValueNotifyingHost(m->convertTo0to1(v));
        d9.applyCustom();
        bool allDefault = true;
        for (auto* prm : d9.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(prm))
                if (rp->getParameterID() != "grid" && std::fabs(rp->getValue() - rp->getDefaultValue()) > 1.0e-4f) { allDefault = false; std::printf("  not default: %s\n", rp->getParameterID().toRawUTF8()); }
        bool anyMod = false; for (int d = 0; d < bs::kModCount; ++d) anyMod = anyMod || d9.isModOn(d);
        bool clean = true; const auto& de = d9.getStepExtras();
        for (int i = 0; i < bs::kPatternLen; ++i) clean = clean && !de.bal[i] && de.pan[i] == 0.0f && !de.wah[i] && !de.rev[i] && de.semi[i] == 0.0f && !de.src[i];
        int on = 0; for (int i = 0; i < bs::kPatternLen; ++i) on += d9.getStepVel(i) > 0.999f;
        std::printf("default: params %d, grid %d, dots on %d, mods %d, extras clean %d, sample %d, layer %d, name '%s'\n", (int) allDefault, (int) rawv(d9, "grid"), on,
                    (int) anyMod, (int) clean, (int) (d9.getPlan()->sample != nullptr), (int) (d9.getLayer() != nullptr), d9.getPresetName().toRawUTF8());
        if (!allDefault || (int) rawv(d9, "grid") != 0 || on != 4 || anyMod || !clean || d9.getPlan()->sample != nullptr || d9.getLayer() != nullptr) { std::printf("FAIL default\n"); return 1; }

        // 4) BALAGAN filter: the roll is always heard (never closed), and the sweep is clear in both directions
        auto sd = std::make_shared<bs::SampleData>(); sd->sr = 44100.0;
        juce::Random nr(3);
        sd->L.resize(88200); for (auto& v : sd->L) v = 0.5f * (nr.nextFloat() - 0.5f);
        sd->R = sd->L;
        auto roll = [&](float amt, float cutoff)
        {
            bs::Engine e; e.prepare(44100.0);
            bs::Params q; q.mode = 1; q.grid = 2; q.bars = 0; q.pick = 1; q.sustain = 1.0f; q.length = 1.0f; q.releaseMs = 5.0f;
            q.balagan = 25.0f; q.balFilter = amt; q.cutoff = cutoff;
            float pt[bs::kPatternLen] = {}; for (int i = 0; i < 16; ++i) pt[i] = 1.0f;
            bs::StepExtras ex; ex.bal[0] = 1;
            e.setParams(q); e.setPlan(bs::buildPlan(sd, q, pt, {}, &ex)); e.setHost(false, false, 0.0, 0.0);
            std::vector<float> L(5512), R(L.size());
            e.render(L.data(), R.data(), (int) L.size());
            auto rms = [&](int a, int b) { double x = 0; for (int i = a; i < b; ++i) x += (double) L[(size_t) i] * L[(size_t) i]; return std::sqrt(x / (b - a)); };
            auto hi = [&](int a, int b) { double x = 0; for (int i = a + 1; i < b; ++i) x += std::pow(L[(size_t) i] - L[(size_t) i - 1], 2.0); return std::sqrt(x / (b - a)); };
            return std::array<double, 4> { rms(200, 2000), rms(2956, 4800), hi(200, 2000), hi(2956, 4800) };
        };
        const auto off = roll(0.0f, 20000.0f);
        for (float cut : { 20000.0f, 3000.0f })
            for (float amt : { 1.0f, 0.4f, -0.4f, -1.0f })
            {
                const auto r = roll(amt, cut);
                std::printf("balagan filter %+.0f%% (cutoff %.0f): level start %.3f end %.3f (off %.3f), brightness start %.3f end %.3f\n", amt * 100.0f, cut, r[0], r[1], off[0], r[2], r[3]);
                const double quietest = std::min(r[0], r[1]);
                if (quietest < 0.25 * off[0] * (cut < 10000.0f ? 0.5 : 1.0)) { std::printf("FAIL balagan filter closes too far\n"); return 1; }
                if (amt > 0 ? !(r[3] > r[2] * 1.3) : !(r[2] > r[3] * 1.3)) { std::printf("FAIL balagan filter sweep\n"); return 1; }
            }
    }

    // ---- version 10 ------------------------------------------------------------------------
    auto pump = [](int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil(ms); };
    auto setp = [](SlicerProcessor& pr, const char* id, float v) { if (auto* m = pr.apvts.getParameter(id)) m->setValueNotifyingHost(m->convertTo0to1(v)); };
    {   // 1/2 grid: two dots per bar, saved as index 5, older indexes unchanged
        SlicerProcessor g2; g2.setRateAndBufferSizeDetails(44100.0, 512); g2.prepareToPlay(44100.0, 512);
        g2.loadSample(makeWav());
        setp(g2, "grid", 5.0f); pump(30);
        const int spb = g2.getPlan()->stepsPerBar;
        juce::MemoryBlock mb; g2.getStateInformation(mb);
        SlicerProcessor g3; g3.setStateInformation(mb.getData(), (int) mb.getSize());
        std::printf("v10 grid 1/2: steps per bar %d, restored grid %d, trip still %d steps\n", spb, (int) rawv(g3, "grid"), bs::stepsPerBarFor(4));
        if (spb != 2 || (int) rawv(g3, "grid") != 5 || bs::stepsPerBarFor(4) != 12 || bs::stepsPerBarFor(0) != 4) { std::printf("FAIL half grid\n"); return 1; }
    }
    {   // v11 LOCK ONE SHOT: three separate states (dots / One Shot / lock), every dot plays the whole locked one shot
        SlicerProcessor lk; lk.setRateAndBufferSizeDetails(44100.0, 512); lk.prepareToPlay(44100.0, 512);
        lk.loadSample(makeWav());
        setp(lk, "grid", 0.0f); pump(20);                       // 4 dots
        setp(lk, "start", 0.1f); setp(lk, "end", 0.9f); setp(lk, "release", 80.0f); setp(lk, "decay", 250.0f);
        lk.setStepVel(2, 0.0f);                                 // the dots preset: dot 3 off
        setp(lk, "oneshot", 1.0f);
        setp(lk, "start", 0.3f); setp(lk, "end", 0.5f); setp(lk, "release", 10.0f); setp(lk, "decay", 40.0f);
        // no lock: leaving One Shot = back to the dots exactly as they were
        setp(lk, "oneshot", 0.0f); pump(20);
        const bool backToDots = std::fabs(rawv(lk, "start") - 0.1f) < 0.01f && std::fabs(rawv(lk, "release") - 80.0f) < 0.5f
                                && lk.getStepVel(2) == 0.0f && !lk.getPlan()->locked;
        // One Shot again: its own edits come back
        setp(lk, "oneshot", 1.0f); pump(20);
        const bool osBack = std::fabs(rawv(lk, "start") - 0.3f) < 0.01f && std::fabs(rawv(lk, "release") - 10.0f) < 0.5f;
        // lock = picture; leaving One Shot -> all dots on, each plays the whole locked part with the locked knobs
        setp(lk, "oslock", 1.0f);
        setp(lk, "oneshot", 0.0f); pump(20);
        auto pl = lk.getPlan();
        bool allWhole = pl->locked;
        int onDots = 0;
        for (int i = 0; i < pl->stepsPerBar; ++i)
        {
            const auto& sl = pl->slices[(size_t) i];
            allWhole = allWhole && sl.whole && sl.start == pl->regionStart && sl.end == pl->regionEnd;
            onDots += sl.vel > 0.001f ? 1 : 0;
        }
        const bool lockKnobs = std::fabs(rawv(lk, "release") - 10.0f) < 0.5f && std::fabs(rawv(lk, "start") - 0.3f) < 0.01f;
        std::printf("v11 lock: back to dots %d, one shot edits back %d, locked dots whole %d, dots on %d/4, lock knobs %d\n",
                    (int) backToDots, (int) osBack, (int) allWhole, onDots, (int) lockKnobs);
        if (!backToDots || !osBack || !allWhole || onDots != 4 || !lockKnobs) { std::printf("FAIL v11 lock states\n"); return 1; }
        // still free to work inside the lock: dot 2 gets a shorter release, only dot 2 changes
        lk.moveDotsEnv(1u << 1, bs::kDotEnvRel, lk.getDotsShownValue(1u << 1, bs::kDotEnvRel), 5.0f);
        const bool dot2 = std::fabs(lk.getDotEnvValue(1, bs::kDotEnvRel) - 5.0f) < 0.1f && std::fabs(lk.getDotEnvValue(0, bs::kDotEnvRel) - 10.0f) < 0.1f
                          && lk.getPlan()->slices[1].dot.has(bs::kDotEnvRel);
        // grid 1/4 -> 1/8 inside the lock: old places keep their work, new dots are the clean one shot (all on)
        setp(lk, "grid", 1.0f); pump(20);
        pl = lk.getPlan();
        int on8 = 0; for (int i = 0; i < 8; ++i) on8 += pl->slices[(size_t) i].vel > 0.001f ? 1 : 0;
        const bool kept = lk.dotHasOwnEnv(2) && !lk.dotHasOwnEnv(1) && !lk.dotHasOwnEnv(3) && std::fabs(lk.getDotEnvValue(2, bs::kDotEnvRel) - 5.0f) < 0.1f;
        // lock off -> the normal dots and preset (grid 1/4, dot 3 off, release 80); lock on -> the lock exactly as left
        setp(lk, "oslock", 0.0f); pump(20);
        const bool normal = (int) rawv(lk, "grid") == 0 && lk.getStepVel(2) == 0.0f && std::fabs(rawv(lk, "release") - 80.0f) < 0.5f
                            && std::fabs(rawv(lk, "start") - 0.1f) < 0.01f && !lk.getPlan()->locked && !lk.dotHasOwnEnv(2);
        lk.setStepVel(0, 0.5f); // work in the normal dots while the lock is parked
        setp(lk, "oslock", 1.0f); pump(20);
        const bool lockBack = (int) rawv(lk, "grid") == 1 && lk.dotHasOwnEnv(2) && lk.getPlan()->locked && lk.getStepVel(0) > 0.99f
                              && std::fabs(rawv(lk, "release") - 10.0f) < 0.5f;
        setp(lk, "oslock", 0.0f); pump(20);
        const bool normalKept = std::fabs(lk.getStepVel(0) - 0.5f) < 0.001f;
        std::printf("v11 lock work: dot 2 own release %d, after 1/8 %d dots on, work kept on its place %d, lock off = normal %d, lock on = as left %d, normal work kept %d\n",
                    (int) dot2, on8, (int) kept, (int) normal, (int) lockBack, (int) normalKept);
        if (!dot2 || on8 != 8 || !kept || !normal || !lockBack || !normalKept) { std::printf("FAIL v11 lock work\n"); return 1; }
        // the lock and the parked dots survive a save / reload (project and preset)
        setp(lk, "oslock", 1.0f); pump(20);
        juce::MemoryBlock mb; lk.getStateInformation(mb);
        SlicerProcessor lk2; lk2.setRateAndBufferSizeDetails(44100.0, 512); lk2.prepareToPlay(44100.0, 512);
        lk2.setStateInformation(mb.getData(), (int) mb.getSize()); pump(30);
        const bool reLock = lk2.isOneShotLocked() && lk2.getPlan()->locked && lk2.dotHasOwnEnv(2) && (int) rawv(lk2, "grid") == 1;
        setp(lk2, "oslock", 0.0f); pump(20);
        const bool reNormal = (int) rawv(lk2, "grid") == 0 && std::fabs(lk2.getStepVel(0) - 0.5f) < 0.001f && std::fabs(rawv(lk2, "release") - 80.0f) < 0.5f;
        std::printf("v11 lock state: reloaded lock %d, its parked dots %d\n", (int) reLock, (int) reNormal);
        if (!reLock || !reNormal) { std::printf("FAIL v11 lock state\n"); return 1; }
        // LOCK pressed in One Shot after changing the sound = a new picture (clean dots)
        setp(lk2, "oneshot", 1.0f); setp(lk2, "decay", 900.0f); setp(lk2, "oslock", 1.0f); setp(lk2, "oneshot", 0.0f); pump(20);
        if (lk2.dotHasOwnEnv(2) || std::fabs(rawv(lk2, "decay") - 900.0f) > 1.0f) { std::printf("FAIL v11 new picture\n"); return 1; }
        // a locked dot plays like the one shot (Length = part of the sample, not squeezed into the step)
        lk2.keyboardState.noteOn(1, 60, 1.0f);
        float pkL = 0.0f; for (int blk = 0; blk < 60; ++blk) { juce::MidiBuffer m; buf.clear(); lk2.processBlock(buf, m); pkL = juce::jmax(pkL, buf.getMagnitude(0, 512)); }
        lk2.keyboardState.noteOff(1, 60, 0.0f);
        std::printf("v11 locked dots peak %.3f\n", pkL);
        if (pkL < 0.02f) { std::printf("FAIL v11 locked dots silent\n"); return 1; }
    }
    {   // v11 several dots + the envelope knobs: the knob shows the highest, every dot moves from its own value
        SlicerProcessor md; md.setRateAndBufferSizeDetails(44100.0, 512); md.prepareToPlay(44100.0, 512);
        md.loadSample(makeWav());
        setp(md, "grid", 0.0f); setp(md, "release", 200.0f); pump(20);
        const uint32_t right = (1u << 2) | (1u << 3), left = 1u << 0;
        md.moveDotsEnv(right, bs::kDotEnvRel, 200.0f, 60.0f);          // the two right dots down
        md.moveDotsEnv(left, bs::kDotEnvRel, 200.0f, 120.0f);          // the left one a bit less
        const bool untouched = std::fabs(md.getDotEnvValue(1, bs::kDotEnvRel) - 200.0f) < 0.1f; // the dot never touched follows the knob
        const float shown = md.getDotsShownValue(left | right, bs::kDotEnvRel);
        auto* rp = md.apvts.getParameter("release");
        const float n0 = rp->convertTo0to1(md.getDotEnvValue(0, bs::kDotEnvRel)), n2 = rp->convertTo0to1(md.getDotEnvValue(2, bs::kDotEnvRel));
        md.moveDotsEnv(left | right, bs::kDotEnvRel, shown, rp->convertFrom0to1(rp->convertTo0to1(shown) - 0.1f));
        const float m0 = rp->convertTo0to1(md.getDotEnvValue(0, bs::kDotEnvRel)), m2 = rp->convertTo0to1(md.getDotEnvValue(2, bs::kDotEnvRel));
        std::printf("v11 dots knobs: shown %.0f (highest of 120/60), moved %.3f / %.3f (same turn), untouched dot %d\n", shown, n0 - m0, n2 - m2, (int) untouched);
        if (std::fabs(shown - 120.0f) > 0.5f || std::fabs((n0 - m0) - 0.1f) > 0.002f || std::fabs((n2 - m2) - 0.1f) > 0.002f || !untouched)
        { std::printf("FAIL v11 dots knobs\n"); return 1; }
        // an own release really changes the sound of that dot only
        bs::Engine e; e.prepare(44100.0);
        auto sd = std::make_shared<bs::SampleData>(); sd->sr = 44100.0; sd->L.assign(44100, 0.0f);
        for (int i = 0; i < 44100; ++i) sd->L[(size_t) i] = 0.5f * std::sin(i * 0.05f);
        sd->R = sd->L;
        bs::Params q; q.mode = 1; q.grid = 0; q.bars = 0; q.pick = 1; q.sustain = 1.0f; q.length = 0.25f; q.releaseMs = 300.0f; q.tempo = 120.0f;
        float pt[bs::kPatternLen] = { 1.0f, 1.0f, 1.0f, 1.0f };
        bs::StepExtras ex; ex.env[0].v[bs::kDotEnvRel] = 5.0f;
        e.setParams(q); e.setPlan(bs::buildPlan(sd, q, pt, {}, &ex)); e.setHost(false, false, 0.0, 0.0);
        std::vector<float> L(44100), R(L.size());
        e.render(L.data(), R.data(), (int) L.size());
        auto rms = [&](int a2, int b3) { double x = 0; for (int i = a2; i < b3; ++i) x += (double) L[(size_t) i] * L[(size_t) i]; return std::sqrt(x / (b3 - a2)); };
        const double tail0 = rms(6500, 10500), tail1 = rms(22050 + 6500, 22050 + 10500); // after Length 25 % closes the step (quarter = 22050)
        std::printf("v11 dot release: tail of dot 1 (5 ms) %.4f, dot 2 (300 ms knob) %.4f\n", tail0, tail1);
        if (!(tail0 < tail1 * 0.3) || tail1 < 0.01) { std::printf("FAIL v11 dot release\n"); return 1; }
        // the dot envelope is saved
        juce::MemoryBlock mb; md.getStateInformation(mb);
        SlicerProcessor md2; md2.setStateInformation(mb.getData(), (int) mb.getSize()); pump(20);
        if (!md2.dotHasOwnEnv(0) || md2.dotHasOwnEnv(1) || std::fabs(md2.getDotEnvValue(2, bs::kDotEnvRel) - md.getDotEnvValue(2, bs::kDotEnvRel)) > 0.05f)
        { std::printf("FAIL v11 dot env state\n"); return 1; }
        // preset with the same name exists -> the Save window asks first
        const juce::String pn = "zz v11 overwrite test";
        md.saveUserPreset(pn);
        const bool exists = SlicerProcessor::userPresetExists(pn) && !SlicerProcessor::userPresetExists(pn + " other");
        SlicerProcessor::getUserPresetDir().getChildFile(juce::File::createLegalFileName(pn) + ".bspreset").deleteFile();
        std::printf("v11 preset exists check %d\n", (int) exists);
        if (!exists) { std::printf("FAIL v11 preset exists\n"); return 1; }
    }
    {   // SLICE envelope: on a short hit the release only matters in SLICE mode; own envelopes per slice
        auto sd = std::make_shared<bs::SampleData>(); sd->sr = 44100.0;
        const int N = 44100; sd->L.assign((size_t) N, 0.0f);
        juce::Random nr(11);
        for (int h = 0; h < 4; ++h) // four snare-like hits, one in every quarter of the sample
            for (int i = 0; i < 9000; ++i) sd->L[(size_t) (h * 11025 + i)] = 0.8f * std::exp(-i / 2500.0f) * (nr.nextFloat() - 0.5f);
        sd->R = sd->L;
        auto run = [&](int envSlice, float relMs, const std::vector<bs::SegEnv>* envs)
        {
            bs::Engine e; e.prepare(44100.0);
            bs::Params q; q.mode = 1; q.grid = 0; q.bars = 0; q.pick = 1; q.sustain = 1.0f; q.length = 1.0f; q.decayMs = 300.0f;
            q.releaseMs = relMs; q.envSlice = envSlice; q.tempo = 120.0f;
            float pt[bs::kPatternLen] = { 1.0f, 1.0f, 1.0f, 1.0f };
            e.setParams(q); e.setPlan(bs::buildPlan(sd, q, pt, {}, nullptr, nullptr, envs)); e.setHost(false, false, 0.0, 0.0);
            std::vector<float> L(44100), R(L.size());
            e.render(L.data(), R.data(), (int) L.size());
            // the tail of the hit (where Release works): 3000..11000 samples after each step starts
            double en = 0; for (int i = 3000; i < 11000; ++i) en += (double) L[(size_t) i] * L[(size_t) i];
            double en2 = 0; for (int i = 22050 + 3000; i < 22050 + 11000; ++i) en2 += (double) L[(size_t) i] * L[(size_t) i];
            return std::make_pair(std::sqrt(en / 8000.0), std::sqrt(en2 / 8000.0));
        };
        const auto sShort = run(0, 5.0f, nullptr), sLong = run(0, 3000.0f, nullptr);
        const auto cShort = run(1, 5.0f, nullptr), cLong = run(1, 3000.0f, nullptr);
        std::printf("v10 slice env: SAMPLE rel 5/3000 ms -> %.4f / %.4f, SLICE rel 5/3000 ms -> %.4f / %.4f\n", sShort.first, sLong.first, cShort.first, cLong.first);
        if (std::fabs(sShort.first - sLong.first) > 1.0e-4) { std::printf("FAIL sample mode must stay as v9\n"); return 1; }
        if (!(cLong.first < cShort.first * 0.8) || cLong.first < cShort.first * 0.2) { std::printf("FAIL slice mode release (no effect, or silent)\n"); return 1; }
        {
            // SLICE: Length 50 % closes the hit in the middle of its own audio (in SAMPLE mode the step is longer than the hit)
            bs::Engine e; e.prepare(44100.0);
            bs::Params q; q.mode = 1; q.grid = 0; q.bars = 0; q.pick = 1; q.sustain = 1.0f; q.length = 0.5f; q.releaseMs = 5.0f; q.envSlice = 1;
            float pt[bs::kPatternLen] = { 1.0f, 1.0f, 1.0f, 1.0f };
            e.setParams(q); e.setPlan(bs::buildPlan(sd, q, pt, {})); e.setHost(false, false, 0.0, 0.0);
            std::vector<float> L(22050), R(L.size());
            e.render(L.data(), R.data(), (int) L.size());
            float late = 0.0f; for (int i = 6500; i < 11000; ++i) late = juce::jmax(late, std::fabs(L[(size_t) i]));
            std::printf("v10 slice length 50%%: peak after the middle of the slice %.5f\n", late);
            if (late > 1.0e-4f) { std::printf("FAIL slice length\n"); return 1; }
        }
        std::vector<bs::SegEnv> envs(4);
        envs[0] = { true, 1.0f, 15.0f, 0.0f, 5.0f }; // the first slice: very short and tight
        const auto own = run(1, 5.0f, &envs);
        std::printf("v10 own envelope: slice 1 %.4f (was %.4f), slice 2 untouched %.4f (was %.4f)\n", own.first, cShort.first, own.second, cShort.second);
        if (!(own.first < cShort.first * 0.6) || own.second < 0.001 || std::fabs(own.second - cShort.second) > 1.0e-4) { std::printf("FAIL own slice envelope\n"); return 1; }
    }
    {   // processor side: own slice envelopes are saved, a grid change removes them, RESET SAMPLE resets the sample work only
        SlicerProcessor se; se.setRateAndBufferSizeDetails(44100.0, 512); se.prepareToPlay(44100.0, 512);
        se.loadSample(makeWav());
        setp(se, "envmode", 1.0f);
        se.setSegEnvValue(2, 1, 40.0f);
        se.setSegEnvValue(2, 3, 9.0f);
        juce::MemoryBlock mb; se.getStateInformation(mb);
        SlicerProcessor se2; se2.setStateInformation(mb.getData(), (int) mb.getSize()); pump(30);
        const auto e2 = se2.getSegEnv(2);
        std::printf("v10 seg env restored: own %d decay %.0f release %.0f, plan slice uses it %d\n", (int) se2.segHasOwnEnv(2), e2.decayMs, e2.releaseMs,
                    (int) se2.getPlan()->slices[2].env.on);
        if (!se2.segHasOwnEnv(2) || std::fabs(e2.decayMs - 40.0f) > 0.1f || std::fabs(e2.releaseMs - 9.0f) > 0.1f || !se2.getPlan()->slices[2].env.on)
        { std::printf("FAIL seg env state\n"); return 1; }
        setp(se2, "grid", 1.0f); pump(30);
        if (se2.segHasOwnEnv(2)) { std::printf("FAIL seg env kept after grid change\n"); return 1; }
        // RESET SAMPLE
        setp(se, "cutoff", 3000.0f); se.addMod(bs::mdPitch);
        setp(se, "start", 0.2f); setp(se, "end", 0.7f);
        se.setStepVel(1, 0.0f); se.setStepRev(2, true); se.setSlip(1, 0.01f); se.setStepBal(3, true);
        se.resetSample(); pump(30);
        const auto& ex = se.getStepExtras();
        bool on = true; for (int i = 0; i < se.getPlan()->stepsPerBar; ++i) on = on && se.getStepVel(i) >= 0.999f;
        std::printf("v10 reset sample: markers %.2f-%.2f, dots all on %d, extras clean %d, slip %.3f, own env %d, cutoff kept %.0f, lfo kept %d, sample kept %d\n",
                    rawv(se, "start"), rawv(se, "end"), (int) on, (int) (!ex.rev[2] && !ex.bal[3]), se.getSlip(1), (int) se.segHasOwnEnv(2),
                    rawv(se, "cutoff"), (int) se.isModOn(bs::mdPitch), (int) (se.getPlan()->sample != nullptr));
        if (rawv(se, "start") > 0.001f || rawv(se, "end") < 0.999f || !on || ex.rev[2] || ex.bal[3] || se.getSlip(1) != 0.0f || se.segHasOwnEnv(2)
            || std::fabs(rawv(se, "cutoff") - 3000.0f) > 1.0f || !se.isModOn(bs::mdPitch) || se.getPlan()->sample == nullptr) { std::printf("FAIL reset sample\n"); return 1; }
    }
    {   // the dots selection box in the real editor: drag on an empty spot, click one selected dot = all off, click again = all on
        SlicerProcessor sb; sb.setRateAndBufferSizeDetails(44100.0, 512); sb.prepareToPlay(44100.0, 512);
        sb.loadSample(makeWav());
        setp(sb, "grid", 2.0f); setp(sb, "swing", 0.0f);
        sb.resetSample(); pump(30);
        std::unique_ptr<juce::AudioProcessorEditor> ed3(sb.createEditor());
        bsui::BeatBar* bar = nullptr;
        std::function<void(juce::Component&)> findBar = [&](juce::Component& c) { for (auto* ch : c.getChildren()) { if (auto* b = dynamic_cast<bsui::BeatBar*>(ch)) bar = b; findBar(*ch); } };
        findBar(*ed3);
        if (bar == nullptr) { std::printf("FAIL no beat bar\n"); return 1; }
        auto src = juce::Desktop::getInstance().getMainMouseSource();
        auto ev = [&](juce::Point<float> pos, juce::Point<float> down, bool drag)
        {
            return juce::MouseEvent(src, pos, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, bar, bar,
                                    juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, drag);
        };
        const float w = (float) bar->getWidth() - 16.0f;
        auto sx = [&](int i) { return 8.0f + ((float) i / 16.0f + 0.5f / 16.0f) * w; };
        const juce::Point<float> a(sx(4) - 6.0f, 60.0f), b2(sx(15) + 6.0f, 66.0f);
        bar->mouseDown(ev(a, a, false)); bar->mouseDrag(ev(b2, a, true)); bar->mouseUp(ev(b2, a, true));
        const juce::Point<float> dot(sx(6), 22.5f);
        bar->mouseDown(ev(dot, dot, false)); bar->mouseUp(ev(dot, dot, false));
        int onCount = 0; for (int i = 0; i < 16; ++i) onCount += sb.getStepVel(i) > 0.001f ? 1 : 0;
        const bool firstFourOn = sb.getStepVel(0) > 0.5f && sb.getStepVel(3) > 0.5f;
        bar->mouseDown(ev(dot, dot, false)); bar->mouseUp(ev(dot, dot, false));
        int onCount2 = 0; for (int i = 0; i < 16; ++i) onCount2 += sb.getStepVel(i) > 0.001f ? 1 : 0;
        std::printf("v10 selection box: after click %d dots on (first four on %d), after second click %d on\n", onCount, (int) firstFourOn, onCount2);
        if (onCount != 4 || !firstFourOn || onCount2 != 16) { std::printf("FAIL selection box\n"); return 1; }
        // SLICE lamp + a chosen slice: the Decay knob edits only that slice; letting go gives the knob back to the parameter
        setp(sb, "envmode", 1.0f);
        sb.setSelectedSeg(5);
        pump(250);
        bsui::KnobCell* decay = nullptr;
        std::function<void(juce::Component&)> findK = [&](juce::Component& c) { for (auto* ch : c.getChildren()) { if (auto* k = dynamic_cast<bsui::KnobCell*>(ch); k != nullptr && k->paramId == "decay") decay = k; findK(*ch); } };
        findK(*ed3);
        const float globalDecay = rawv(sb, "decay");
        decay->slider.setValue(33.0, juce::sendNotificationSync);
        const bool own5 = sb.segHasOwnEnv(5) && std::fabs(sb.getSegEnv(5).decayMs - 33.0f) < 0.5f;
        const bool globalSame = std::fabs(rawv(sb, "decay") - globalDecay) < 0.01f;
        sb.setSelectedSeg(-1);
        pump(250);
        std::printf("v10 envelope knobs: slice 6 decay %.1f (own %d), global decay untouched %d, knob back to parameter shows %.0f\n",
                    sb.getSegEnv(5).decayMs, (int) own5, (int) globalSame, decay->slider.getValue());
        if (!own5 || !globalSame || decay->hasOverride() || std::fabs((float) decay->slider.getValue() - globalDecay) > 0.5f) { std::printf("FAIL envelope knobs\n"); return 1; }
    }

    // snapshot: show the v4 features in use
    p.applyFactoryPreset(1);
    p.addMod(bs::mdCutoff); p.setModAmount(bs::mdCutoff, -0.45f);
    p.addMod(bs::mdPitch); p.setModAmount(bs::mdPitch, 0.25f);
    p.addMod(bs::mdChoMix); p.setModAmount(bs::mdChoMix, 0.3f);
    p.setLfoPoints({ { 0, 1, 0.55f }, { 0.5f, 0.15f, -0.3f }, { 0.75f, 0.6f, 0.4f }, { 1, 1, 0 } });
    for (auto [id, v] : std::vector<std::pair<const char*, float>> { { "cutoff", 6000.0f }, { "resonance", 0.35f }, { "univoices", 3.0f },
                                                                     { "unidetune", 18.0f }, { "unispread", 0.7f }, { "chomix", 0.3f } })
        if (auto* m = p.apvts.getParameter(id)) m->setValueNotifyingHost(m->convertTo0to1(v));
    p.setSlip(p.getPlan()->slices[4].seg, 0.006f);
    // v6 in the picture: a few W steps, some wah, LOOP on
    for (int k : { 6, 14, 22, 30 }) p.setStepWah(k, true);
    // v7 in the picture: a copied chop, a dot with three letters, noise type, a layer sound
    p.copyStepToNext(16);
    p.setStepRev(17, true);
    p.setStepRev(14, true);
    if (auto* m = p.apvts.getParameter("noise")) m->setValueNotifyingHost(0.2f);
    {
        auto lf = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("bs_layer_pic.wav");
        juce::AudioBuffer<float> lb(2, 44100);
        juce::Random lr(9);
        for (int i = 0; i < 44100; ++i)
        {
            const float env = 0.25f + 0.6f * std::pow(std::sin(3.14159f * i / 44100.0f * 3.0f), 2.0f);
            for (int c = 0; c < 2; ++c) lb.setSample(c, i, env * (lr.nextFloat() - 0.5f));
        }
        lf.deleteFile();
        std::unique_ptr<juce::FileOutputStream> os(lf.createOutputStream());
        std::unique_ptr<juce::AudioFormatWriter> w(juce::WavAudioFormat().createWriterFor(os.get(), 44100, 2, 16, {}, 0));
        os.release(); w->writeFromAudioSampleBuffer(lb, 0, 44100);
        w.reset();
        p.loadLayer(lf);
    }
    // v8 in the picture: red BALAGAN dots, some panned dots, the BALAGAN panel in use
    for (int k : { 3, 11, 27 }) { p.setStepVel(k, 1.0f); p.setStepBal(k, true); }
    p.setStepVel(2, 0.7f);
    p.setStepPan(2, -0.8f); p.setStepPan(8, 0.5f); p.setStepPan(11, -0.35f); p.setStepPan(13, 1.0f);
    for (auto [id, v] : std::vector<std::pair<const char*, float>> { { "balagan", 68.0f }, { "balsuper", 1.0f }, { "balfilter", 0.4f } })
        if (auto* m = p.apvts.getParameter(id)) m->setValueNotifyingHost(m->convertTo0to1(v));
    p.addMod(bs::mdBalagan); p.setModAmount(bs::mdBalagan, 0.2f);
    for (auto [id, v] : std::vector<std::pair<const char*, float>> { { "wah", 0.45f }, { "wahspeed", 0.6f }, { "latch", 1.0f } })
        if (auto* m = p.apvts.getParameter(id)) m->setValueNotifyingHost(m->convertTo0to1(v));
    p.keyboardState.noteOn(1, 48, 0.9f);
    for (int blk = 0; blk < 60; ++blk) { juce::MidiBuffer m; buf.clear(); p.processBlock(buf, m); }
    std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
    ed->setSize(bsui::kBaseW, bsui::kBaseH);
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
    juce::File out(argc > 1 ? argv[1] : "editor.png");
    out.deleteFile();
    juce::FileOutputStream fos(out);
    juce::PNGImageFormat().writeImageToStream(img, fos);
    std::printf("snapshot %dx%d -> %s\n", img.getWidth(), img.getHeight(), out.getFullPathName().toRawUTF8());
    if (argc > 3)
    {
        // v6: zoom picture - markers around the start of the sample, zoom slider pushed up
        if (auto* m = p.apvts.getParameter("start")) m->setValueNotifyingHost(0.05f);
        if (auto* m = p.apvts.getParameter("end")) m->setValueNotifyingHost(0.35f);
        std::function<void(juce::Component&)> find = [&](juce::Component& c)
        {
            for (auto* ch : c.getChildren())
            {
                if (auto* w = dynamic_cast<bsui::WaveView*>(ch)) { w->anchorZoomToRegion(); w->setZoom(0.85f); }
                if (auto* sl = dynamic_cast<juce::Slider*>(ch); sl != nullptr && sl->getSliderStyle() == juce::Slider::LinearHorizontal && sl->getTextBoxPosition() == juce::Slider::NoTextBox && sl->getMaximum() == 1.0 && sl->getProperties().contains("accent") && sl->getParentComponent() != nullptr && sl->getParentComponent()->getHeight() > 300)
                    sl->setValue(0.85, juce::dontSendNotification);
                find(*ch);
            }
        };
        find(*ed);
        auto img2 = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
        juce::File out2(argv[3]);
        out2.deleteFile();
        juce::FileOutputStream fos2(out2);
        juce::PNGImageFormat().writeImageToStream(img2, fos2);
    }
    if (argc > 4)
    {
        // v10 picture: LOCK ONE SHOT holding a part in the dots mode, SLICE lamp with a chosen slice, own envelopes,
        // the dots selection box, the 1/2 grid button, RESET SAMPLE and a wide-open BALAGAN mouth
        SlicerProcessor v; v.setRateAndBufferSizeDetails(44100.0, 512); v.prepareToPlay(44100.0, 512);
        v.loadSample(makeWav());
        setp(v, "bars", 0.0f); setp(v, "grid", 2.0f); setp(v, "swing", 0.0f);
        setp(v, "oneshot", 1.0f); setp(v, "start", 0.04f); setp(v, "end", 0.66f); setp(v, "oslock", 1.0f); setp(v, "oneshot", 0.0f);
        pump(30);
        v.resetSample(); // dots on, nothing extra (the lock goes off with it) ...
        setp(v, "oneshot", 1.0f); setp(v, "start", 0.04f); setp(v, "end", 0.66f); setp(v, "oslock", 1.0f); setp(v, "oneshot", 0.0f); // ... lock again
        pump(30);
        for (int k : { 1, 5, 9, 10, 13 }) v.setStepVel(k, 0.0f);
        v.setStepVel(6, 0.5f);
        setp(v, "envmode", 1.0f);
        v.setSegEnvValue(2, 1, 60.0f); v.setSegEnvValue(2, 2, 0.15f); v.setSegEnvValue(2, 3, 30.0f);
        v.setSegEnvValue(7, 1, 900.0f); v.setSegEnvValue(7, 3, 400.0f);
        v.setSelectedSeg(2);
        for (auto [id, val] : std::vector<std::pair<const char*, float>> { { "balagan", 92.0f }, { "cutoff", 6000.0f }, { "chomix", 0.25f } })
            setp(v, id, val);
        std::unique_ptr<juce::AudioProcessorEditor> ed4(v.createEditor());
        ed4->setSize(bsui::kBaseW, bsui::kBaseH);
        bsui::BeatBar* bar = nullptr;
        std::function<void(juce::Component&)> findBar = [&](juce::Component& c) { for (auto* ch : c.getChildren()) { if (auto* b = dynamic_cast<bsui::BeatBar*>(ch)) bar = b; findBar(*ch); } };
        findBar(*ed4);
        auto src = juce::Desktop::getInstance().getMainMouseSource();
        auto ev = [&](juce::Point<float> pos, juce::Point<float> down, bool drag)
        {
            return juce::MouseEvent(src, pos, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, bar, bar,
                                    juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, drag);
        };
        const float w = (float) bar->getWidth() - 16.0f;
        auto sx = [&](int i) { return 8.0f + ((float) i / 16.0f + 0.5f / 16.0f) * w; };
        const juce::Point<float> a(sx(8) - 8.0f, 6.0f), b2(sx(11) + 8.0f, 50.0f);
        bar->mouseDown(ev(a, a, false)); bar->mouseDrag(ev(b2, a, true)); // the box is still being dragged in the picture
        pump(300);
        auto img4 = ed4->createComponentSnapshot(ed4->getLocalBounds(), true, 1.0f);
        juce::File out4(argv[4]);
        out4.deleteFile();
        juce::FileOutputStream fos4(out4);
        juce::PNGImageFormat().writeImageToStream(img4, fos4);
        std::printf("v10 snapshot -> %s\n", out4.getFullPathName().toRawUTF8());
        if (argc > 5)
        {
            // the BALAGAN mouth at 1, 25, 50, 75 and 100
            bsui::KnobCell* bk = nullptr;
            std::function<void(juce::Component&)> findK = [&](juce::Component& c) { for (auto* ch : c.getChildren()) { if (auto* k = dynamic_cast<bsui::KnobCell*>(ch); k != nullptr && k->name.getText() == "Balagan") bk = k; findK(*ch); } };
            findK(*ed4);
            juce::Image strip(juce::Image::RGB, bk->getWidth() * 5, bk->getHeight(), true);
            juce::Graphics sg(strip);
            int i = 0;
            for (float val : { 1.0f, 25.0f, 50.0f, 75.0f, 100.0f })
            {
                setp(v, "balagan", val);
                bk->slider.setValue(val, juce::dontSendNotification);
                auto one = bk->getParentComponent()->createComponentSnapshot(bk->getBounds(), true, 1.0f);
                sg.drawImageAt(one, bk->getWidth() * i++, 0);
            }
            juce::File out5(argv[5]);
            out5.deleteFile();
            juce::FileOutputStream fos5(out5);
            juce::PNGImageFormat().writeImageToStream(strip, fos5);
        }
    }
    if (argc > 6)
    {
        // v10: the first lines of the preset list as the menu draws them (normal, then with the mouse over Default)
        bsui::LookAndFeel lf;
        juce::Image im(juce::Image::RGB, 300, 4 * 26, true);
        juce::Graphics g(im);
        g.fillAll(lf.findColour(juce::PopupMenu::backgroundColourId));
        const juce::String lines[] = { "Default  -  Reset Plugin", "", "House Chop", "Default  -  Reset Plugin" };
        for (int r = 0; r < 4; ++r)
            lf.drawPopupMenuItem(g, { 0, r * 26, 300, 26 }, r == 1, true, r == 3, false, false, lines[r], {}, nullptr, nullptr);
        juce::File out6(argv[6]);
        out6.deleteFile();
        juce::FileOutputStream fos6(out6);
        juce::PNGImageFormat().writeImageToStream(im, fos6);
    }
    if (argc > 7)
    {
        // v10: the Standalone header (BPM box + LOCK ONE SHOT + RESET SAMPLE in one row)
        juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Standalone);
        SlicerProcessor st;
        juce::AudioProcessor::setTypeOfNextNewPlugin(juce::AudioProcessor::wrapperType_Undefined); st.setRateAndBufferSizeDetails(44100.0, 512); st.prepareToPlay(44100.0, 512);
        st.loadSample(makeWav());
        setp(st, "oneshot", 1.0f);
        std::unique_ptr<juce::AudioProcessorEditor> ed7(st.createEditor());
        ed7->setSize(bsui::kBaseW, bsui::kBaseH);
        pump(200);
        auto img7 = ed7->createComponentSnapshot(ed7->getLocalBounds(), true, 1.0f);
        juce::File out7(argv[7]);
        out7.deleteFile();
        juce::FileOutputStream fos7(out7);
        juce::PNGImageFormat().writeImageToStream(img7, fos7);
    }
    {   // v11: the real editor - chosen dots drive the envelope knobs (highest value, relative move), letting go gives them back
        SlicerProcessor kv; kv.setRateAndBufferSizeDetails(44100.0, 512); kv.prepareToPlay(44100.0, 512);
        kv.loadSample(makeWav());
        setp(kv, "grid", 0.0f); setp(kv, "release", 200.0f);
        setp(kv, "oneshot", 1.0f); setp(kv, "start", 0.0f); setp(kv, "end", 0.12f); setp(kv, "oslock", 1.0f); setp(kv, "oneshot", 0.0f);
        std::unique_ptr<juce::AudioProcessorEditor> ed8(kv.createEditor());
        ed8->setSize(bsui::kBaseW, bsui::kBaseH);
        bsui::KnobCell* rel = nullptr; bsui::KnobCell* len = nullptr;
        std::function<void(juce::Component&)> findK = [&](juce::Component& c) { for (auto* ch : c.getChildren()) { if (auto* k = dynamic_cast<bsui::KnobCell*>(ch)) { if (k->paramId == "release") rel = k; if (k->name.getText() == "Length") len = k; } findK(*ch); } };
        findK(*ed8);
        kv.setSelectedDots((1u << 2) | (1u << 3));
        pump(250);
        rel->slider.setValue(40.0, juce::sendNotificationSync);       // the two right dots: release down
        kv.setSelectedDots(1u << 0);
        pump(250);
        const float leftShows = (float) rel->slider.getValue();        // the left dot was never changed: shows the knob value
        rel->slider.setValue(120.0, juce::sendNotificationSync);
        kv.setSelectedDots((1u << 0) | (1u << 2) | (1u << 3));
        pump(250);
        const float allShow = (float) rel->slider.getValue();          // the highest of 120 / 40 / 40
        len->slider.setValue(0.5, juce::sendNotificationSync);           // Length follows the dots too
        const bool lenDots = kv.dotHasOwnEnv(0) && std::fabs(kv.getDotEnvValue(2, bs::kDotEnvLen) - 0.5f) < 0.02f && std::fabs(rawv(kv, "length") - 0.8f) < 0.01f;
        // picture of the locked dots with their own envelopes
        kv.setStepVel(1, 0.0f);
        auto img8 = ed8->createComponentSnapshot(ed8->getLocalBounds(), true, 1.0f);
        if (argc > 8) { juce::File o8(argv[8]); o8.deleteFile(); juce::FileOutputStream f8(o8); juce::PNGImageFormat().writeImageToStream(img8, f8); }
        kv.setSelectedDots(0);
        pump(250);
        std::printf("v11 editor dots: left dot shows %.0f, three dots show %.0f, length per dot %d, knob back to parameter %d (%.0f)\n",
                    leftShows, allShow, (int) lenDots, (int) !rel->hasOverride(), rel->slider.getValue());
        if (std::fabs(leftShows - 200.0f) > 1.0f || std::fabs(allShow - 120.0f) > 1.0f || !lenDots || rel->hasOverride()
            || std::fabs((float) rel->slider.getValue() - 200.0f) > 1.0f) { std::printf("FAIL v11 editor dots\n"); return 1; }
    }
    // ================================================================== version 12
    auto mev = [](juce::Component* c, juce::Point<float> pos, juce::Point<float> down, bool drag, int clicks = 1)
    {
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), pos, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),
                                1.0f, 0.0f, 0.0f, 0.0f, 0.0f, c, c, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), clicks, drag);
    };
    auto toneFile = [](const char* name, double seconds)
    {
        const int n = (int) (44100 * seconds);
        juce::AudioBuffer<float> tb(2, n);
        for (int i = 0; i < n; ++i) for (int c = 0; c < 2; ++c) tb.setSample(c, i, 0.5f * std::sin(i * 0.05f));
        auto f = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(name);
        f.deleteFile();
        std::unique_ptr<juce::FileOutputStream> os(f.createOutputStream());
        std::unique_ptr<juce::AudioFormatWriter> w(juce::WavAudioFormat().createWriterFor(os.get(), 44100, 2, 16, {}, 0));
        os.release(); w->writeFromAudioSampleBuffer(tb, 0, n);
        return f;
    };
    {   // change 1: One Shot - the envelope lives between the locators: Release ends at the right locator, moving it moves the end
        SlicerProcessor os; os.setRateAndBufferSizeDetails(44100.0, 512); os.prepareToPlay(44100.0, 512);
        setp(os, "oneshot", 1.0f);
        os.loadSample(toneFile("bs_tone2.wav", 2.0));
        setp(os, "length", 1.0f); setp(os, "sustain", 1.0f); setp(os, "attack", 0.5f); setp(os, "decay", 5.0f);
        auto play = [&](float relMs, float endNorm)
        {
            setp(os, "release", relMs); setp(os, "end", endNorm); pump(30);
            os.reset();
            std::vector<float> mags;
            for (int blk = 0; blk < 330; ++blk)
            {
                juce::MidiBuffer m; if (blk == 0) m.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 127), 0);
                buf.clear(); os.processBlock(buf, m);
                mags.push_back(buf.getMagnitude(0, 512));
            }
            int last = 0; for (int k = 0; k < (int) mags.size(); ++k) if (mags[(size_t) k] > 0.002f) last = k;
            const double endSec = (last + 1) * 512 / 44100.0;
            const int probe = juce::jmax(0, (int) ((endSec - 0.15) * 44100.0 / 512.0)); // 150 ms before the end
            return std::make_pair(endSec, mags[(size_t) probe] / juce::jmax(1.0e-6f, mags[40]));
        };
        const auto shortRel = play(5.0f, 1.0f), longRel = play(300.0f, 1.0f), moved = play(300.0f, 0.5f);
        std::printf("v12 one shot locators: release 5 ms ends %.2fs (level 150 ms before %.2f), 300 ms ends %.2fs (level %.2f), right locator at half ends %.2fs (level %.2f)\n",
                    shortRel.first, shortRel.second, longRel.first, longRel.second, moved.first, moved.second);
        if (std::fabs(shortRel.first - 2.0) > 0.04 || std::fabs(longRel.first - 2.0) > 0.04 || std::fabs(moved.first - 1.0) > 0.04
            || shortRel.second < 0.9f || longRel.second < 0.3f || longRel.second > 0.7f || moved.second < 0.3f || moved.second > 0.7f)
        { std::printf("FAIL v12 one shot locators\n"); return 1; }
    }
    {   // change 1 + 3: LOCK ONE SHOT - every copy has its own locators; dragging the one of a chosen dot moves every chosen dot
        SlicerProcessor lk; lk.setRateAndBufferSizeDetails(44100.0, 512); lk.prepareToPlay(44100.0, 512);
        lk.loadSample(makeWav());
        setp(lk, "grid", 0.0f);
        setp(lk, "oneshot", 1.0f); setp(lk, "start", 0.0f); setp(lk, "end", 0.3f); setp(lk, "oslock", 1.0f); setp(lk, "oneshot", 0.0f);
        pump(30);
        std::unique_ptr<juce::AudioProcessorEditor> ed(lk.createEditor());
        ed->setSize(bsui::kBaseW, bsui::kBaseH);
        bsui::WaveView* wv = nullptr;
        std::function<void(juce::Component&)> findW = [&](juce::Component& c) { for (auto* ch : c.getChildren()) { if (auto* w = dynamic_cast<bsui::WaveView*>(ch)) wv = w; findW(*ch); } };
        findW(*ed);
        lk.setSelectedDots((1u << 0) | (1u << 1));
        const auto bnd = wv->getLocalBounds().toFloat().reduced(0.5f);
        const float cellW = (bnd.getWidth() - 8.0f) / 4.0f, cx0 = bnd.getX() + 4.0f + 1.0f, cw = cellW - 2.0f;
        const float hx = cx0 + 1.0f + (cw - 2.0f) * 1.0f, y = bnd.getCentreY();
        const juce::Point<float> d0(hx - 2.0f, y), d1(hx - 2.0f - 0.4f * (cw - 2.0f), y);
        wv->mouseDown(mev(wv, d0, d0, false)); wv->mouseDrag(mev(wv, d1, d0, true)); wv->mouseUp(mev(wv, d1, d0, true));
        const float e0 = lk.getDotTrim(0, bs::kDotEnd), e1 = lk.getDotTrim(1, bs::kDotEnd), e2 = lk.getDotTrim(2, bs::kDotEnd);
        auto pl = lk.getPlan();
        const float played = (float) (pl->slices[0].end - pl->slices[0].start) / (float) (pl->regionEnd - pl->regionStart);
        std::printf("v12 lock locators: dot 1 end %.2f, dot 2 end %.2f (chosen), dot 3 end %.2f, dot 1 plays %.2f of the one shot, trim %d / %d\n",
                    e0, e1, e2, played, (int) pl->slices[0].trim, (int) pl->slices[2].trim);
        if (std::fabs(e0 - 0.6f) > 0.03f || std::fabs(e1 - e0) > 0.001f || e2 < 0.999f || std::fabs(played - e0) > 0.02f || !pl->slices[0].trim || pl->slices[2].trim)
        { std::printf("FAIL v12 lock locators\n"); return 1; }
        // the start locator of dot 3 alone (not chosen) - only it moves
        const float sx = cx0 + 2.0f * cellW + 1.0f, s1 = sx + 0.25f * (cw - 2.0f);
        wv->mouseDown(mev(wv, { sx + 1.0f, y }, { sx + 1.0f, y }, false)); wv->mouseDrag(mev(wv, { s1 + 1.0f, y }, { sx + 1.0f, y }, true));
        wv->mouseUp(mev(wv, { s1 + 1.0f, y }, { sx + 1.0f, y }, true));
        const float st2 = lk.getDotTrim(2, bs::kDotStart), st0 = lk.getDotTrim(0, bs::kDotStart);
        // saved with the project
        juce::MemoryBlock mb; lk.getStateInformation(mb);
        SlicerProcessor lk2; lk2.setRateAndBufferSizeDetails(44100.0, 512); lk2.prepareToPlay(44100.0, 512);
        lk2.setStateInformation(mb.getData(), (int) mb.getSize()); pump(30);
        const bool kept = std::fabs(lk2.getDotTrim(0, bs::kDotEnd) - e0) < 0.01f && std::fabs(lk2.getDotTrim(2, bs::kDotStart) - st2) < 0.01f;
        std::printf("v12 lock locators: dot 3 start %.2f (dot 1 start %.2f), restored %d\n", st2, st0, (int) kept);
        if (std::fabs(st2 - 0.25f) > 0.03f || st0 > 0.001f || !kept) { std::printf("FAIL v12 lock locators 2\n"); return 1; }
        // the locked dots still play
        float pk = 0.0f;
        lk.keyboardState.noteOn(1, 60, 1.0f);
        for (int blk = 0; blk < 120; ++blk) { juce::MidiBuffer m; buf.clear(); lk.processBlock(buf, m); pk = juce::jmax(pk, buf.getMagnitude(0, 512)); }
        lk.keyboardState.noteOff(1, 60, 0.0f);
        if (pk < 0.02f) { std::printf("FAIL v12 lock locators silent\n"); return 1; }
        if (argc > 10)
        {
            for (int k : { 3 }) lk.setStepVel(k, 0.5f);
            pump(250);
            auto im = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
            juce::File o(argv[10]); o.deleteFile(); juce::FileOutputStream fo(o); juce::PNGImageFormat().writeImageToStream(im, fo);
        }
    }
    {   // change 2: every parameter can be automated, in clear folders, with the old ids
        SlicerProcessor gp;
        const auto& tree = gp.getParameterTree();
        int inGroups = 0;
        juce::StringArray names;
        bool named = true;
        for (auto* g : tree.getSubgroups(false)) inGroups += (int) g->getParameters(false).size();
        for (auto* prm : gp.getParameters())
        {
            const auto nm = prm->getName(100);
            named = named && nm.isNotEmpty() && prm->isAutomatable();
            names.addIfNotAlreadyThere(nm);
        }
        bool idsOk = true;
        for (auto* id : { "balagan", "swing", "wah", "mainmix", "layermix", "cutoff", "attack", "grid", "start", "end", "balmode" })
            idsOk = idsOk && gp.apvts.getParameter(id) != nullptr;
        std::printf("v12 automation: %d parameters, %d in %d folders, all named + automatable %d, unique names %d, ids %d\n", gp.getParameters().size(), inGroups,
                    tree.getSubgroups(false).size(), (int) named, (int) (names.size() == gp.getParameters().size()), (int) idsOk);
        if (inGroups != gp.getParameters().size() || !named || names.size() != gp.getParameters().size() || !idsOk) { std::printf("FAIL v12 automation\n"); return 1; }
    }
    {   // change 3 + 4: chosen dots - every change goes to every chosen dot; PICK ALL chooses them all
        SlicerProcessor cd; cd.setRateAndBufferSizeDetails(44100.0, 512); cd.prepareToPlay(44100.0, 512);
        cd.loadSample(makeWav());
        setp(cd, "grid", 2.0f); pump(30);
        std::unique_ptr<juce::AudioProcessorEditor> ed(cd.createEditor());
        ed->setSize(bsui::kBaseW, bsui::kBaseH);
        bsui::KnobCell* cut = nullptr; bsui::KnobCell* pit = nullptr; bsui::BeatBar* bar = nullptr; juce::TextButton* pickAll = nullptr;
        std::function<void(juce::Component&)> findAll = [&](juce::Component& c)
        {
            for (auto* ch : c.getChildren())
            {
                if (auto* k = dynamic_cast<bsui::KnobCell*>(ch)) { if (k->name.getText() == "Cutoff") cut = k; if (k->name.getText() == "Pitch") pit = k; }
                if (auto* b = dynamic_cast<bsui::BeatBar*>(ch)) bar = b;
                if (auto* t = dynamic_cast<juce::TextButton*>(ch); t != nullptr && t->getButtonText() == "PICK ALL") pickAll = t;
                findAll(*ch);
            }
        };
        findAll(*ed);
        const uint32_t two = (1u << 0) | (1u << 2);
        cd.setSelectedDots(two);
        pump(250);
        cut->slider.setValue(800.0, juce::sendNotificationSync);
        pit->slider.setValue(-5.0, juce::sendNotificationSync);
        const auto& ex = cd.getStepExtras();
        const bool cutOk = ex.env[0].has(bs::kDotCutoff) && std::fabs(ex.env[0].v[bs::kDotCutoff] - 800.0f) < 2.0f && std::fabs(ex.env[2].v[bs::kDotCutoff] - 800.0f) < 2.0f
                           && !ex.env[1].has(bs::kDotCutoff) && rawv(cd, "cutoff") > 19999.0f;
        const bool pitchOk = std::fabs(ex.env[2].v[bs::kDotPitch] + 5.0f) < 0.05f && !ex.env[1].has(bs::kDotPitch) && std::fabs(rawv(cd, "pitch")) < 0.01f;
        // the right-click menu choices go to every chosen dot
        cd.editDots(two, [](int k, float&, bs::StepExtras& x) { x.rev[k] = 1; x.bal[k] = 1; });
        const bool menuOk = ex.rev[0] && ex.rev[2] && !ex.rev[1] && ex.bal[0] && ex.bal[2] && !ex.bal[1];
        // dragging a chosen dot moves every chosen dot by the same dB
        const float before0 = bs::dotToDb(cd.getStepVel(0)), before2 = bs::dotToDb(cd.getStepVel(2)), before1 = cd.getStepVel(1);
        const float w = (float) bar->getWidth() - 16.0f;
        const juce::Point<float> p0(8.0f + 0.5f * w / 16.0f, 22.5f), p1 = p0.translated(0.0f, 40.0f);
        bar->mouseDown(mev(bar, p0, p0, false)); bar->mouseDrag(mev(bar, p0.translated(0.0f, 5.0f), p0, true)); bar->mouseDrag(mev(bar, p1, p0, true));
        bar->mouseUp(mev(bar, p1, p0, true));
        const float d0 = bs::dotToDb(cd.getStepVel(0)) - before0, d2 = bs::dotToDb(cd.getStepVel(2)) - before2;
        // the engine really plays the dot's own cutoff (dot 1 is dark, dot 2 is open)
        auto pl = cd.getPlan();
        const bool planOk = pl->slices[0].dot.has(bs::kDotCutoff) && !pl->slices[1].dot.has(bs::kDotCutoff);
        // PICK ALL
        cd.setSelectedDots(0);
        pickAll->triggerClick();
        pump(100);
        const uint32_t all = cd.getSelectedDots();
        // the panel titles say which dots the knobs follow
        cd.setSelectedDots(two); pump(250);
        juce::String filterTitle; // (the SLICE ENVELOPE title - FILTER has no room next to its LP / HP / BP buttons)
        for (auto* ch : cut->getParentComponent()->getParentComponent()->getChildren()) if (auto* pn = dynamic_cast<bsui::Panel*>(ch); pn != nullptr && pn->title.startsWith("SLICE ENVELOPE")) filterTitle = pn->title;
        std::printf("v12 chosen dots: cutoff %d, pitch %d, menu %d, drag %.1f / %.1f dB (other dot kept %d), plan %d, PICK ALL 0x%x, title '%s'\n",
                    (int) cutOk, (int) pitchOk, (int) menuOk, d0, d2, (int) (cd.getStepVel(1) == before1), (int) planOk, (unsigned) all, filterTitle.toRawUTF8());
        if (!cutOk || !pitchOk || !menuOk || std::fabs(d0 + 5.25f) > 0.1f || std::fabs(d2 - d0) > 0.05f || cd.getStepVel(1) != before1 || !planOk
            || all != 0xffffu || !filterTitle.contains("2 DOTS (1,3)")) { std::printf("FAIL v12 chosen dots\n"); return 1; }
        // the own values come back with the project
        juce::MemoryBlock mb; cd.getStateInformation(mb);
        SlicerProcessor cd2; cd2.setStateInformation(mb.getData(), (int) mb.getSize()); pump(20);
        if (std::fabs(cd2.getStepExtras().env[2].v[bs::kDotPitch] + 5.0f) > 0.05f || cd2.getStepExtras().env[1].has(bs::kDotCutoff)) { std::printf("FAIL v12 dot values state\n"); return 1; }
    }
    {   // change 5: the value box (double-click, type 37), the BPM lamp, and the clean fades of a fast roll
        SlicerProcessor bv; bv.setRateAndBufferSizeDetails(44100.0, 512); bv.prepareToPlay(44100.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed(bv.createEditor());
        ed->setSize(bsui::kBaseW, bsui::kBaseH);
        bsui::BalaganValue* box = nullptr; bsui::BalModeLamps* lamps = nullptr;
        std::function<void(juce::Component&)> findB = [&](juce::Component& c)
        { for (auto* ch : c.getChildren()) { if (auto* x = dynamic_cast<bsui::BalaganValue*>(ch)) box = x; if (auto* l = dynamic_cast<bsui::BalModeLamps*>(ch)) lamps = l; findB(*ch); } };
        findB(*ed);
        box->mouseDoubleClick(mev(box, { 10.0f, 10.0f }, { 10.0f, 10.0f }, false, 2));
        juce::Label* lab = nullptr;
        for (auto* ch : box->getChildren()) if (auto* l = dynamic_cast<juce::Label*>(ch)) lab = l;
        const bool cleared = lab != nullptr && lab->getCurrentTextEditor() != nullptr && lab->getCurrentTextEditor()->getText().isEmpty();
        if (lab != nullptr && lab->getCurrentTextEditor() != nullptr) lab->getCurrentTextEditor()->setText("37", false);
        if (lab != nullptr) lab->hideEditor(false);
        const float typed = rawv(bv, "balagan");
        const juce::Point<float> bpmLamp(20.0f, (float) lamps->getHeight() * 0.75f);
        lamps->mouseUp(mev(lamps, bpmLamp, bpmLamp, false));
        const float mode = rawv(bv, "balmode");
        // a very fast roll on a steady tone: no jump bigger than the tone itself makes (every repeat fades in and out)
        bs::Engine e; e.prepare(44100.0);
        auto tone = std::make_shared<bs::SampleData>(); tone->sr = 44100.0;
        tone->L.resize(88200); for (int i = 0; i < 88200; ++i) tone->L[(size_t) i] = 0.5f * std::sin(i * 0.05f);
        tone->R = tone->L;
        bs::Params q; q.mode = 1; q.grid = 2; q.pick = 1; q.sustain = 1.0f; q.length = 1.0f; q.releaseMs = 5.0f; q.balagan = 100.0f; q.balSuper = 1;
        float pt[bs::kPatternLen] = {}; for (int i = 0; i < 16; ++i) pt[i] = 1.0f;
        bs::StepExtras ex; for (int i = 0; i < 16; ++i) ex.bal[i] = 1;
        e.setParams(q); e.setPlan(bs::buildPlan(tone, q, pt, {}, &ex)); e.setHost(false, false, 0.0, 0.0);
        std::vector<float> L(22050), R(L.size());
        e.render(L.data(), R.data(), (int) L.size());
        float jump = 0.0f;
        for (size_t i = 1; i < L.size(); ++i) jump = juce::jmax(jump, std::fabs(L[i] - L[i - 1]));
        std::printf("v12 balagan: double-click cleared %d, typed 37 -> %.0f, BPM lamp -> mode %.0f, fast SUPER roll biggest jump %.3f (the tone alone 0.025)\n",
                    (int) cleared, typed, mode, jump);
        if (!cleared || std::fabs(typed - 37.0f) > 0.01f || mode < 0.5f || jump > 0.06f) { std::printf("FAIL v12 balagan value\n"); return 1; }
    }
    {   // change 6 + 7: ENV on Cutoff - every chop starts bright and closes; only on the knobs where it helps; UNDRAG; saved
        auto noise = std::make_shared<bs::SampleData>(); noise->sr = 44100.0;
        juce::Random nr(11);
        noise->L.resize(88200); for (auto& v : noise->L) v = 0.5f * (nr.nextFloat() - 0.5f);
        noise->R = noise->L;
        auto bright = [&](float amt)
        {
            bs::Engine e; e.prepare(44100.0);
            bs::Params q; q.mode = 1; q.grid = 2; q.pick = 1; q.attackMs = 0.5f; q.decayMs = 40.0f; q.sustain = 0.25f; q.length = 1.0f; q.releaseMs = 5.0f;
            q.cutoff = 400.0f; q.envAmt[bs::mdCutoff] = amt;
            float pt[bs::kPatternLen] = {}; pt[0] = 1.0f;
            e.setParams(q); e.setPlan(bs::buildPlan(noise, q, pt, {})); e.setHost(false, false, 0.0, 0.0);
            std::vector<float> L(5000), R(L.size());
            e.render(L.data(), R.data(), (int) L.size());
            auto ratio = [&](int a, int b) { double hi = 0, all = 1e-12; for (int i = a + 1; i < b; ++i) { hi += std::pow(L[(size_t) i] - L[(size_t) i - 1], 2.0); all += L[(size_t) i] * L[(size_t) i]; } return hi / all; };
            return std::make_pair(ratio(100, 900), ratio(3000, 4800));
        };
        const auto off = bright(0.0f), on = bright(0.8f);
        SlicerProcessor em;
        em.addEnvMod(bs::mdResonance); em.addEnvMod(bs::mdCutoff); em.setEnvModAmount(bs::mdCutoff, 0.6f); em.addEnvMod(bs::mdBalagan);
        const bool allowed = !em.isEnvModOn(bs::mdResonance) && em.isEnvModOn(bs::mdCutoff) && em.isEnvModOn(bs::mdBalagan);
        juce::MemoryBlock mb; em.getStateInformation(mb);
        SlicerProcessor em2; em2.setStateInformation(mb.getData(), (int) mb.getSize());
        const bool saved = em2.isEnvModOn(bs::mdCutoff) && std::fabs(em2.getEnvModAmount(bs::mdCutoff) - 0.6f) < 0.001f && em2.isEnvModOn(bs::mdBalagan);
        em2.removeEnvMod(bs::mdBalagan);
        em2.addMod(bs::mdCutoff);
        em2.removeEnvMod(bs::mdCutoff); // UNDRAG ENV leaves the LFO on the same knob
        const bool undrag = !em2.isEnvModOn(bs::mdCutoff) && em2.isModOn(bs::mdCutoff) && !em2.isEnvModOn(bs::mdBalagan);
        // the knobs accept the ENV handle only where it is allowed
        std::unique_ptr<juce::AudioProcessorEditor> ed(em.createEditor());
        int accept = 0, refuse = 0;
        std::function<void(juce::Component&)> scan = [&](juce::Component& c)
        {
            for (auto* ch : c.getChildren())
            {
                if (auto* k = dynamic_cast<bsui::KnobCell*>(ch); k != nullptr && k->modIdx >= 0)
                {
                    const bool wants = k->isInterestedInDragSource(juce::DragAndDropTarget::SourceDetails(juce::var("env"), nullptr, {}));
                    if (wants != bs::envDestAllowed(k->modIdx)) refuse = -100;
                    wants ? ++accept : ++refuse;
                }
                scan(*ch);
            }
        };
        scan(*ed);
        std::printf("v12 ENV: cutoff start/end brightness off %.3f/%.3f, on %.3f/%.3f, allowed %d, saved %d, UNDRAG %d, knobs accept %d refuse %d\n",
                    off.first, off.second, on.first, on.second, (int) allowed, (int) saved, (int) undrag, accept, refuse);
        if (!(on.first > on.second * 2.0) || !(on.first > off.first * 2.0) || !allowed || !saved || !undrag || accept != 5 || refuse < 10)
        { std::printf("FAIL v12 ENV\n"); return 1; }
    }
    {   // change 8: Monkey Library + Export / Import (the sample travels inside the file)
        int monkeys = 0; bool names = true;
        for (auto& f : bs::factoryPresets()) if (f.library) { ++monkeys; names = names && juce::String(f.name).endsWith("Monkey"); }
        SlicerProcessor ex; ex.setRateAndBufferSizeDetails(44100.0, 512); ex.prepareToPlay(44100.0, 512);
        auto src = toneFile("bs_export_sound.wav", 1.0);
        ex.loadSample(src);
        int yellow = -1;
        for (int i = 0; i < (int) bs::factoryPresets().size(); ++i) if (juce::String(bs::factoryPresets()[(size_t) i].name) == "Yellow Monkey") yellow = i;
        ex.applyFactoryPreset(yellow);
        auto out = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Shared Yellow.bspreset");
        out.deleteFile();
        const bool exported = ex.exportPreset(out);
        src.deleteFile(); // "another computer": the original sound is not there
        SlicerProcessor im; im.setRateAndBufferSizeDetails(44100.0, 512); im.prepareToPlay(44100.0, 512);
        const auto added = im.importPreset(out);
        const bool loaded = added.existsAsFile() && im.loadUserPreset(added);
        bool inList = false;
        for (auto& f : im.getUserPresetFiles()) inList = inList || f == added;
        const bool same = im.getSampleName() == "bs_export_sound.wav" && im.getPresetName() == "Shared Yellow" && (int) rawv(im, "balmode") == 1
                          && im.getStepExtras().bal[0] && im.getStepExtras().src[2] == 1;
        float pk = 0.0f;
        im.keyboardState.noteOn(1, 60, 1.0f);
        for (int blk = 0; blk < 100; ++blk) { juce::MidiBuffer m; buf.clear(); im.processBlock(buf, m); pk = juce::jmax(pk, buf.getMagnitude(0, 512)); }
        im.keyboardState.noteOff(1, 60, 0.0f);
        std::printf("v12 presets: Monkey Library %d (names %d), export %d (%lld KB), import %d, in My presets %d, same sound %d, peak %.3f\n",
                    monkeys, (int) names, (int) exported, (long long) (out.getSize() / 1024), (int) loaded, (int) inList, (int) same, pk);
        juce::File(SlicerProcessor::getSampleStoreDir().getChildFile("bs_export_sound.wav")).deleteFile();
        added.deleteFile();
        out.deleteFile();
        if (monkeys != 10 || !names || !exported || !loaded || !inList || !same || pk < 0.02f) { std::printf("FAIL v12 presets\n"); return 1; }
    }
    if (argc > 9)
    {
        // v12 picture: a Monkey preset, ENV on the Cutoff (green ring), two chosen dots (titles), BALAGAN in BPM, a dot with its own locators
        SlicerProcessor sh; sh.setRateAndBufferSizeDetails(44100.0, 512); sh.prepareToPlay(44100.0, 512);
        sh.loadSample(makeWav());
        int banana = -1;
        for (int i = 0; i < (int) bs::factoryPresets().size(); ++i) if (juce::String(bs::factoryPresets()[(size_t) i].name) == "Banana Monkey") banana = i;
        sh.applyFactoryPreset(banana);
        sh.addMod(bs::mdPitch); sh.setModAmount(bs::mdPitch, 0.2f);
        sh.addEnvMod(bs::mdBalFilter); sh.setEnvModAmount(bs::mdBalFilter, -0.4f);
        for (int k : { 6, 14 }) { sh.setStepBal(k, true); }
        setp(sh, "balmode", 1.0f); setp(sh, "balagan", 9.0f);
        sh.setSelectedDots((1u << 2) | (1u << 6));
        float v[bs::kPatternLen]; for (auto& x : v) x = 0.15f;
        sh.setDotTrims((1u << 2) | (1u << 6), bs::kDotStart, v);
        for (auto& x : v) x = 0.7f;
        sh.setDotTrims((1u << 2) | (1u << 6), bs::kDotEnd, v);
        sh.keyboardState.noteOn(1, 48, 0.9f);
        for (int blk = 0; blk < 40; ++blk) { juce::MidiBuffer m; buf.clear(); sh.processBlock(buf, m); }
        std::unique_ptr<juce::AudioProcessorEditor> ed(sh.createEditor());
        ed->setSize(bsui::kBaseW, bsui::kBaseH);
        pump(300);
        auto im = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
        juce::File o(argv[9]); o.deleteFile(); juce::FileOutputStream fo(o); juce::PNGImageFormat().writeImageToStream(im, fo);
        std::printf("v12 snapshot -> %s\n", o.getFullPathName().toRawUTF8());
    }
    // ======================================================================== v13
    {
        // 1. One Shot: Length does not cut - with any Length the sound plays to the right locator, Release ends there
        SlicerProcessor o; o.setRateAndBufferSizeDetails(44100.0, 512); o.prepareToPlay(44100.0, 512);
        setp(o, "oneshot", 1.0f);
        o.loadSample(makeWav());
        pump(30);
        auto lastLoudSec = [&](float length)
        {
            setp(o, "length", length); setp(o, "release", 5.0f);
            o.reset();
            juce::MidiBuffer m; m.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 110), 0);
            int last = 0;
            for (int blk = 0; blk < 400; ++blk) { buf.clear(); juce::MidiBuffer mm = blk == 0 ? m : juce::MidiBuffer(); o.processBlock(buf, mm); if (buf.getMagnitude(0, 512) > 0.002f) last = blk; }
            return (last + 1) * 512 / 44100.0;
        };
        const double l100 = lastLoudSec(1.0f), l20 = lastLoudSec(0.2f);
        std::printf("v13 one shot: plays %.2fs at Length 100%%, %.2fs at Length 20%% (to the right locator both)\n", l100, l20);
        if (std::fabs(l100 - l20) > 0.05 || l20 < 2.7) { std::printf("FAIL v13 length\n"); return 1; }

        // 2. LOOP in One Shot: one press, it plays again and again; the next press stops it after the pass
        setp(o, "latch", 1.0f); setp(o, "length", 1.0f);
        o.reset();
        auto rmsAt = [&](std::vector<float>& lv, double a, double b2) { float mx = 0.0f; for (int k = (int) (a * 44100 / 512); k < (int) (b2 * 44100 / 512) && k < (int) lv.size(); ++k) mx = juce::jmax(mx, lv[(size_t) k]); return mx; };
        std::vector<float> lv;
        for (int blk = 0; blk < (int) (13.0 * 44100 / 512); ++blk)
        {
            juce::MidiBuffer m;
            if (blk == 0) m.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 110), 0);
            if (blk == 2) m.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
            if (blk == (int) (7.5 * 44100 / 512)) m.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 110), 0); // stop
            if (blk == (int) (7.5 * 44100 / 512) + 2) m.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
            buf.clear(); o.processBlock(buf, m); lv.push_back(buf.getMagnitude(0, 512));
        }
        const float pass2 = rmsAt(lv, 3.0, 3.4), pass3 = rmsAt(lv, 6.0, 6.4), after = rmsAt(lv, 9.6, 13.0);
        std::printf("v13 one shot LOOP: 2nd pass %.3f, 3rd pass %.3f, after the stop %.4f, looping now %d\n", pass2, pass3, after, (int) o.isLooping());
        if (pass2 < 0.05f || pass3 < 0.05f || after > 0.002f || o.isLooping()) { std::printf("FAIL v13 one shot loop\n"); return 1; }
        setp(o, "latch", 0.0f);
    }
    {
        // 3. ADD LAYER: the silence before the sound is skipped, the start point is kept with the project; drag = set
        SlicerProcessor q; q.setRateAndBufferSizeDetails(44100.0, 512); q.prepareToPlay(44100.0, 512);
        q.loadSample(makeWav());
        const bool ok = q.loadLayer(makeLayerWav("bs_layer_sil.wav", 0.25, 1000.0, 0.4, 0.05));
        const float st = q.getLayerStart();
        q.setLayerStart(0.1f);
        juce::MemoryBlock mb; q.getStateInformation(mb);
        SlicerProcessor r; r.setRateAndBufferSizeDetails(44100.0, 512); r.prepareToPlay(44100.0, 512);
        r.setStateInformation(mb.getData(), (int) mb.getSize());
        const float restored = r.getLayerStart();
        r.autoAlignLayer();
        std::printf("v13 layer start: found %.4fs (silence 0.25s), restored %.3f, align again %.4f\n", st, restored, r.getLayerStart());
        if (!ok || std::fabs(st - 0.25f) > 0.003f || std::fabs(restored - 0.1f) > 1.0e-4f || std::fabs(r.getLayerStart() - st) > 1.0e-4f)
        { std::printf("FAIL v13 layer start\n"); return 1; }
        // the hit of the layer lands with the chop: only the layer is heard (MAIN MIX 0), the first loud block is the first block
        setp(q, "mode", 1.0f); setp(q, "mainmix", 0.0f); setp(q, "layermix", 1.0f); q.autoAlignLayer();
        pump(30);
        int firstLoud = -1;
        for (int blk = 0; blk < 40; ++blk) { juce::MidiBuffer m; buf.clear(); q.processBlock(buf, m); if (firstLoud < 0 && buf.getMagnitude(0, 512) > 0.05f) firstLoud = blk; }
        std::printf("v13 layer on the beat: first loud block %d\n", firstLoud);
        if (firstLoud != 0) { std::printf("FAIL v13 layer timing\n"); return 1; }
    }
    {
        // 4. UNLINK: the layer leaves the effects; the arrow's list links parts back
        SlicerProcessor u; u.setRateAndBufferSizeDetails(44100.0, 512); u.prepareToPlay(44100.0, 512);
        u.loadSample(makeWav());
        u.loadLayer(makeLayerWav("bs_layer_hi.wav", 0.0, 5000.0, 2.0, 10.0));
        setp(u, "mode", 1.0f); setp(u, "mainmix", 0.0f); setp(u, "layermix", 1.0f);
        setp(u, "grid", 0.0f); setp(u, "ftype", 0.0f); setp(u, "cutoff", 150.0f); setp(u, "length", 1.0f); setp(u, "sustain", 1.0f);
        pump(30);
        auto level = [&](int blocks)
        {
            u.reset();
            double e = 0.0;
            for (int blk = 0; blk < blocks; ++blk) { juce::MidiBuffer m; buf.clear(); u.processBlock(buf, m); e += buf.getRMSLevel(0, 0, 512); }
            return (float) (e / blocks);
        };
        const float linked = level(40);
        setp(u, "layunlink", 1.0f); u.setLayerLinks(0);
        const float unlinked = level(40);
        u.setLayerLinks(bs::kLinkFilter);
        const float filterBack = level(40);
        std::printf("v13 UNLINK filter: layer through LP 150 Hz linked %.4f, unlinked %.4f, Filter linked back %.4f\n", linked, unlinked, filterBack);
        if (!(unlinked > linked * 5.0f && filterBack < unlinked * 0.2f)) { std::printf("FAIL v13 unlink filter\n"); return 1; }
        // BALAGAN: linked = the layer repeats with the roll, unlinked = it plays the step once
        setp(u, "cutoff", 20000.0f);
        u.loadLayer(makeLayerWav("bs_layer_click.wav", 0.0, 1500.0, 0.3, 0.004));
        u.setStepBal(0, true); setp(u, "balmode", 1.0f); setp(u, "balagan", 25.0f); // 1/32
        auto hits = [&]()
        {
            u.reset();
            juce::AudioBuffer<float> b(2, 512);
            std::vector<float> fr;
            for (int blk = 0; blk < 44; ++blk) // the first quarter (0.5 s), from the first dot (PLAY + a key)
            {
                juce::MidiBuffer m; b.clear();
                if (blk == 0) m.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 110), 0);
                u.processBlock(b, m);
                for (int k = 0; k < 512; k += 64) fr.push_back(b.getRMSLevel(0, k, 64));
            }
            int n = 0; bool up = false;
            for (size_t k = 0; k < fr.size() && k < (size_t) (0.45 * 44100 / 64); ++k) { if (!up && fr[k] > 0.05f) { ++n; up = true; } else if (up && fr[k] < 0.01f) up = false; }
            return n;
        };
        setp(u, "mode", 0.0f);
        setp(u, "layunlink", 0.0f);
        const int hLinked = hits();
        setp(u, "layunlink", 1.0f); u.setLayerLinks(0);
        const int hUnlinked = hits();
        std::printf("v13 UNLINK balagan: layer hits in the red step linked %d, unlinked %d\n", hLinked, hUnlinked);
        if (hLinked < 4 || hUnlinked != 1) { std::printf("FAIL v13 unlink balagan\n"); return 1; }
        // links are kept with the project
        u.setLayerLinks(bs::kLinkWah | bs::kLinkLfo);
        juce::MemoryBlock mb; u.getStateInformation(mb);
        SlicerProcessor w; w.setStateInformation(mb.getData(), (int) mb.getSize());
        std::printf("v13 UNLINK saved: unlink %d, links %d\n", (int) w.isLayerUnlinked(), w.getLayerLinks());
        if (!w.isLayerUnlinked() || w.getLayerLinks() != (bs::kLinkWah | bs::kLinkLfo)) { std::printf("FAIL v13 unlink state\n"); return 1; }
    }
    {
        // 5. the playhead: the heard step while the loop runs, -1 when nothing plays
        SlicerProcessor h; h.setRateAndBufferSizeDetails(44100.0, 512); h.prepareToPlay(44100.0, 512);
        h.loadSample(makeWav());
        int total = 0;
        const double idle = h.getHeardStep(total);
        setp(h, "mode", 1.0f);
        pump(20);
        for (int blk = 0; blk < 30; ++blk) { juce::MidiBuffer m; buf.clear(); h.processBlock(buf, m); }
        const double a = h.getHeardStep(total);
        std::printf("v13 playhead: idle %.1f, playing %.2f of %d steps\n", idle, a, total);
        if (idle != -1.0 || a < 0.0 || a >= (double) total || total < 2) { std::printf("FAIL v13 playhead\n"); return 1; }
    }
    {
        // 6. preset libraries: only with the folder the installer makes; every folder in it = a library with an arrow
        const auto root = SlicerProcessor::getLibraryRoot();
        const bool existed = root.isDirectory();
        const auto oldUser = SlicerProcessor::getUserPresetDir();
        const bool noFolderMade = existed || (!SlicerProcessor::hasLibraryRoot() && !root.isDirectory());
        const auto noRootLib = SlicerProcessor::createLibrary("Nope");
        { SlicerProcessor op; op.loadSample(makeWav()); op.saveUserPreset("v13 old place"); } // saved before the install: old folder
        root.getChildFile("User").createDirectory();
        root.getChildFile("Monkey Extra").createDirectory();
        SlicerProcessor lp; lp.setRateAndBufferSizeDetails(44100.0, 512); lp.prepareToPlay(44100.0, 512);
        lp.loadSample(makeWav());
        lp.saveUserPreset("v13 test preset");
        const bool savedInUser = root.getChildFile("User").getChildFile("v13 test preset.bspreset").existsAsFile();
        root.getChildFile("User").getChildFile("v13 test preset.bspreset").copyFileTo(root.getChildFile("Monkey Extra").getChildFile("Copy.bspreset"));
        const auto made = SlicerProcessor::createLibrary("Test Lib");
        juce::StringArray libs; for (auto& d : lp.getLibraries()) libs.add(d.getFileName());
        const int oldOnes = SlicerProcessor::getPresetFilesIn(oldUser).size();
        const int userCount = lp.getUserPresetFiles().size();
        const bool loads = lp.loadUserPreset(root.getChildFile("Monkey Extra").getChildFile("Copy.bspreset"));
        std::printf("v13 libraries: no folder made without install %d (lib %d), saved in User %d, libraries [%s], User shows %d (old folder %d), loads %d\n",
                    (int) noFolderMade, (int) (noRootLib != juce::File()), (int) savedInUser, libs.joinIntoString(", ").toRawUTF8(), userCount, oldOnes, (int) loads);
        if (!existed) root.deleteRecursively();
        oldUser.getChildFile("v13 old place.bspreset").deleteFile();
        if (!noFolderMade || noRootLib != juce::File() || !savedInUser || made == juce::File() || !libs.contains("Monkey Extra") || !libs.contains("Test Lib")
            || libs.contains("User") || oldOnes < 1 || userCount < oldOnes + 1 || !loads)
        { std::printf("FAIL v13 libraries\n"); return 1; }
    }
    if (argc > 11)
    {
        // v13 pictures: the ADD LAYER window with a layer (silence skipped), UNLINK on with its list open; then the layer envelope box
        SlicerProcessor sh; sh.setRateAndBufferSizeDetails(44100.0, 512); sh.prepareToPlay(44100.0, 512);
        sh.loadSample(makeWav());
        setp(sh, "oneshot", 1.0f);
        pump(30);
        setp(sh, "length", 0.6f); setp(sh, "release", 400.0f);
        sh.loadLayer(makeLayerWav("bs_layer_pic.wav", 0.3, 180.0, 0.9, 0.25));
        setp(sh, "layunlink", 1.0f); sh.setLayerLinks(bs::kLinkFilter | bs::kLinkWah);
        setp(sh, "layatk", 30.0f); setp(sh, "layrel", 250.0f);
        std::unique_ptr<juce::AudioProcessorEditor> ed(sh.createEditor());
        ed->setSize(bsui::kBaseW, bsui::kBaseH);
        bsui::UnlinkButton* ub = nullptr; juce::TextButton* envB = nullptr;
        std::function<void(juce::Component&)> find = [&](juce::Component& c)
        { for (auto* ch : c.getChildren()) { if (auto* x = dynamic_cast<bsui::UnlinkButton*>(ch)) ub = x; if (auto* t = dynamic_cast<juce::TextButton*>(ch); t != nullptr && t->getButtonText() == "ENVELOPE") envB = t; find(*ch); } };
        find(*ed);
        if (ub == nullptr || envB == nullptr) { std::printf("FAIL v13 no UNLINK / ENVELOPE button\n"); return 1; }
        ub->onArrow();
        pump(300);
        auto im = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
        { juce::File o(argv[11]); o.deleteFile(); juce::FileOutputStream fo(o); juce::PNGImageFormat().writeImageToStream(im, fo); }
        envB->onClick();
        pump(300);
        if (argc > 12)
        {
            auto im2 = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
            juce::File o(argv[12]); o.deleteFile(); juce::FileOutputStream fo(o); juce::PNGImageFormat().writeImageToStream(im2, fo);
        }
        std::printf("v13 snapshots -> %s\n", argv[11]);
    }
    std::printf("ALL OK\n");
    return 0;
}
