#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace bs;

namespace
{
const char* kStructural[] = { "bars", "grid", "pick", "random", "reverse", "seed", "start", "end", "oneshot", "oslock" }; // v10: + oneshot, oslock
// v11: what every state (dots / One Shot / LOCK ONE SHOT) keeps for itself: the markers and the shape of the sound.
// The grid belongs to the dots states only. Volume, Master, LFO, effects, tempo and the rest are shared.
const char* kSlotKnobs[] = { "start", "end", "length", "attack", "decay", "sustain", "release", "pitch", "cutoff", "resonance",
                             "ftype", "noise", "noisetype", "grid" };
// v11: Attack, Decay, Sustain, Release, Length. v12: + Cutoff, Resonance, Noise, Pitch, Volume (same order as bs::kDot*)
const char* kDotEnvIds[bs::kDotKnobCount] = { "attack", "decay", "sustain", "release", "length", "cutoff", "resonance", "noise", "pitch", "volume" };
// v11: the dot of the new grid that sits exactly where a dot of the old grid was (-1 = a new place)
int sameSpot(int j, int oldSpb, int newSpb)
{
    if ((j * oldSpb) % newSpb != 0) return -1;
    const int i = j * oldSpb / newSpb;
    return i < oldSpb ? i : -1;
}
}

juce::AudioProcessorValueTreeState::ParameterLayout SlicerProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto F = [&](const char* id, const char* name, float lo, float hi, float def, float mid, const char* unit, int dec)
    {
        juce::NormalisableRange<float> r(lo, hi);
        if (mid > lo && mid < hi) r.setSkewForCentre(mid);
        auto a = juce::AudioParameterFloatAttributes().withLabel(unit).withStringFromValueFunction(
            [dec, unit](float v, int) { return juce::String(v, dec) + (juce::String(unit).isEmpty() ? "" : " " + juce::String(unit)); });
        p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { id, 1 }, name, r, def, a));
    };
    auto C = [&](const char* id, const char* name, juce::StringArray c, int def)
    { p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { id, 1 }, name, c, def)); };
    auto Pc = [&](const char* id, const char* name, float lo, float hi, float def)
    {
        auto a = juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v * 100.0f)) + " %"; });
        p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float>(lo, hi), def, a));
    };

    C("mode", "Notes Mode", { "Play", "Auto" }, 0);
    C("bars", "Bars", { "1", "2", "4", "8" }, 0);
    C("grid", "Grid", { "1/4", "1/8", "1/16", "1/32", "Trip", "1/2" }, 2); // v10: 1/2 added last, so old projects keep their grid
    C("pick", "Slice Pick", { "Auto", "Manual", "Random" }, 0);
    p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "keytrack", 1 }, "Key Pitch", true));
    p.push_back(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { "seed", 1 }, "Random Seed", 1, 9999, 1));
    Pc("length", "Slice Length", 0.05f, 1.0f, 0.8f);
    F("attack", "Env Attack", 0.1f, 500.0f, 1.0f, 15.0f, "ms", 1);
    F("decay", "Env Decay", 5.0f, 3000.0f, 250.0f, 300.0f, "ms", 0);
    Pc("sustain", "Env Sustain", 0.0f, 1.0f, 0.6f);
    F("release", "Env Release", 5.0f, 3000.0f, 80.0f, 200.0f, "ms", 0);
    F("cutoff", "Filter Cutoff", 40.0f, 20000.0f, 20000.0f, 1500.0f, "Hz", 0);
    Pc("resonance", "Filter Resonance", 0.0f, 1.0f, 0.0f);
    C("ftype", "Filter Type", { "LP", "HP", "BP" }, 0);
    p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "oneshot", 1 }, "One Shot", false));
    // v4: LFO
    C("lfomode", "LFO Mode", { "Trig", "Env", "Free" }, 0);
    C("lforate", "LFO Rate", { "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/4D", "1/8D" }, 4);
    p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "lfosync", 1 }, "LFO BPM Sync", true));
    F("lfohz", "LFO Rate Hz", 0.05f, 20.0f, 2.0f, 2.0f, "Hz", 2);
    // v4: unison + chorus
    p.push_back(std::make_unique<juce::AudioParameterInt>(juce::ParameterID { "univoices", 1 }, "Unison Voices", 1, 8, 1));
    F("unidetune", "Unison Detune", 0.0f, 100.0f, 20.0f, 0.0f, "ct", 0);
    Pc("unispread", "Unison Spread", 0.0f, 1.0f, 0.5f);
    F("chorate", "Chorus Rate", 0.05f, 5.0f, 0.6f, 0.8f, "Hz", 2);
    Pc("chodepth", "Chorus Depth", 0.0f, 1.0f, 0.5f);
    Pc("chomix", "Chorus Mix", 0.0f, 1.0f, 0.0f);
    Pc("noise", "Noise Amount", 0.0f, 1.0f, 0.0f);
    F("pitch", "Pitch", -12.0f, 12.0f, 0.0f, 0.0f, "st", 0);
    Pc("volume", "Volume", 0.0f, 1.0f, 0.8f);
    Pc("velocity", "Velocity Depth", 0.0f, 1.0f, 1.0f); // v5: how much the dots of the beat bar count (id kept for old projects)
    Pc("random", "Random Amount", 0.0f, 1.0f, 0.0f);
    Pc("swing", "Swing", 0.0f, 1.0f, 0.0f);
    Pc("reverse", "Reverse Amount", 0.0f, 1.0f, 0.0f);
    F("master", "Master", -24.0f, 12.0f, -3.0f, 0.0f, "dB", 1); // v5: up to +12 dB
    {
        // Tempo = the internal clock of the Standalone app (whole BPM steps). In a host (Cubase) the project tempo is used.
        juce::NormalisableRange<float> r(60.0f, 200.0f, 1.0f);
        r.setSkewForCentre(120.0f);
        auto a = juce::AudioParameterFloatAttributes().withLabel("BPM").withStringFromValueFunction(
            [](float v, int) { return juce::String(juce::roundToInt(v)) + " BPM"; });
        p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "tempo", 1 }, "Tempo (Standalone)", r, 120.0f, a));
    }
    // v6
    p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "latch", 1 }, "Loop", false));
    Pc("wah", "Wah Amount", 0.0f, 1.0f, 0.0f);
    Pc("wahspeed", "Wah Speed", 0.0f, 1.0f, 0.5f);
    // v7
    C("noisetype", "Noise Type", { "White", "Pink", "Brown", "Wind", "Metal", "Water", "Vinyl", "Tape", "Fire", "Digital" }, 0);
    Pc("layermix", "Layer Mix", 0.0f, 1.0f, 0.5f);
    // v8: BALAGAN
    {
        auto a = juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v)); });
        p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "balagan", 1 }, "Balagan Speed", juce::NormalisableRange<float>(1.0f, 100.0f), 50.0f, a));
    }
    p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "balsuper", 1 }, "Balagan Super", false));
    Pc("balfilter", "Balagan Filter", -1.0f, 1.0f, 0.0f);
    Pc("mainmix", "Main Mix", 0.0f, 1.0f, 1.0f); // v9: level of the main sample only
    Pc("start", "Left Locator (Start)", 0.0f, 1.0f, 0.0f);
    Pc("end", "Right Locator (End)", 0.0f, 1.0f, 1.0f);
    // v10
    p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "oslock", 1 }, "Lock One Shot", false));
    C("envmode", "Env Mode (Sample / Slice)", { "Sample", "Slice" }, 0);
    // v12: BALAGAN - FREE (smooth, as before) or BPM (the repeats land on note values)
    C("balmode", "Balagan Mode (Free / BPM)", { "Free", "BPM" }, 0);
    // v13: ADD LAYER - its own envelope (open with ENVELOPE) and UNLINK
    F("layatk", "Layer Attack", 0.1f, 500.0f, 0.1f, 15.0f, "ms", 1);
    F("laydec", "Layer Decay", 5.0f, 3000.0f, 250.0f, 300.0f, "ms", 0);
    Pc("laysus", "Layer Sustain", 0.0f, 1.0f, 1.0f);
    F("layrel", "Layer Release", 1.0f, 3000.0f, 1.0f, 200.0f, "ms", 0);
    p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "layunlink", 1 }, "Layer Unlink", false));

    // v12: every knob is a parameter, so Cubase can write (W) and read (R) its automation. The parameters sit in
    // folders with clear names, so the automation list in Cubase stays tidy. The ids never change, so old projects,
    // presets and automation keep working.
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto take = [&](const char* id) -> std::unique_ptr<juce::RangedAudioParameter>
    {
        for (auto& u : p)
            if (u != nullptr && u->getParameterID() == id) return std::move(u);
        jassertfalse;
        return nullptr;
    };
    auto group = [&](const char* gid, const char* gname, std::initializer_list<const char*> ids)
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup>(gid, gname, " | ");
        for (auto* id : ids)
            if (auto u = take(id)) g->addChild(std::move(u));
        layout.add(std::move(g));
    };
    group("sample", "Sample", { "oneshot", "oslock", "start", "end", "mainmix", "bars", "grid", "pick", "seed", "mode", "keytrack", "latch" });
    group("slice", "Slice Shape", { "length", "noise", "noisetype" });
    group("filter", "Filter", { "cutoff", "resonance", "ftype" });
    group("envelope", "Slice Envelope", { "attack", "decay", "sustain", "release", "envmode" });
    group("mix", "Per-Slice Mix", { "pitch", "volume", "velocity", "random" });
    group("lfo", "LFO", { "lfomode", "lforate", "lfosync", "lfohz" });
    group("unison", "Unison", { "univoices", "unidetune", "unispread" });
    group("chorus", "Chorus", { "chorate", "chodepth", "chomix" });
    group("wah", "Wah", { "wah", "wahspeed" });
    group("timing", "Timing", { "swing", "reverse", "tempo", "master" });
    group("layer", "Add Layer", { "layermix", "layunlink", "layatk", "laydec", "laysus", "layrel" });
    group("balagan", "Balagan", { "balagan", "balmode", "balsuper", "balfilter" });
    for (auto& u : p)
        if (u != nullptr) layout.add(std::move(u)); // nothing should be left, but never lose a parameter
    return layout;
}

SlicerProcessor::SlicerProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", createLayout())
{
    formats.registerBasicFormats();
    for (auto* id : kStructural) apvts.addParameterListener(id, this);
    lastSpb = stepsPerBarFor((int) raw("grid"));
    resampleRhythm(rhythms()[(size_t) currentRhythm], stepsPerBarFor(2), pattern);
    scratchR.assign(8192, 0.0f);
    setLfoPoints(lfoPreset(0));
    rebuildPlan();
}

SlicerProcessor::~SlicerProcessor()
{
    cancelPendingUpdate();
    for (auto* id : kStructural) apvts.removeParameterListener(id, this);
}

void SlicerProcessor::setParam(const char* id, float plain)
{
    if (auto* pr = apvts.getParameter(id)) pr->setValueNotifyingHost(pr->convertTo0to1(plain));
}

bs::Params SlicerProcessor::readParams() const
{
    Params p;
    p.mode = (int) raw("mode"); p.bars = (int) raw("bars"); p.grid = (int) raw("grid"); p.pick = (int) raw("pick");
    p.keytrack = raw("keytrack") > 0.5f ? 1 : 0; p.seed = (int) raw("seed");
    p.length = raw("length"); p.attackMs = raw("attack"); p.decayMs = raw("decay"); p.sustain = raw("sustain"); p.releaseMs = raw("release");
    p.cutoff = raw("cutoff"); p.noise = raw("noise"); p.pitch = raw("pitch"); p.volume = raw("volume"); p.velocity = raw("velocity");
    p.random = raw("random"); p.swing = raw("swing"); p.reverse = raw("reverse"); p.masterDb = raw("master"); p.tempo = raw("tempo");
    p.regionStart = raw("start"); p.regionEnd = raw("end");
    p.resonance = raw("resonance"); p.filterType = (int) raw("ftype"); p.oneShot = raw("oneshot") > 0.5f ? 1 : 0;
    p.lfoMode = (int) raw("lfomode"); p.lfoSync = raw("lfosync") > 0.5f ? 1 : 0;
    p.lfoBeats = lfoBeatsFor((int) raw("lforate")); p.lfoHz = raw("lfohz");
    p.uniVoices = (int) raw("univoices"); p.uniDetune = raw("unidetune"); p.uniSpread = raw("unispread");
    p.choRate = raw("chorate"); p.choDepth = raw("chodepth"); p.choMix = raw("chomix");
    p.latch = raw("latch") > 0.5f ? 1 : 0; p.wahAmt = raw("wah"); p.wahSpeed = raw("wahspeed");
    p.noiseType = (int) raw("noisetype"); p.layerMix = raw("layermix");
    p.balagan = raw("balagan"); p.balSuper = raw("balsuper") > 0.5f ? 1 : 0; p.balFilter = raw("balfilter"); // v8
    p.mainMix = raw("mainmix"); // v9
    p.envSlice = raw("envmode") > 0.5f ? 1 : 0; // v10
    p.osLock = raw("oslock") > 0.5f ? 1 : 0;    // v11
    p.balMode = raw("balmode") > 0.5f ? 1 : 0;  // v12
    // v13: ADD LAYER
    p.layerStartSec = layerStart.load();
    p.layAtkMs = raw("layatk"); p.layDecMs = raw("laydec"); p.laySus = raw("laysus"); p.layRelMs = raw("layrel");
    p.layUnlink = raw("layunlink") > 0.5f ? 1 : 0;
    p.layLinks = layerLinks.load();
    for (int d = 0; d < kModCount; ++d) p.modAmt[d] = modOn[d].load() ? modAmt[d].load() : 0.0f;
    for (int d = 0; d < kModCount; ++d) p.envAmt[d] = envOn[d].load() && envDestAllowed(d) ? envAmt[d].load() : 0.0f; // v12
    return p;
}

void SlicerProcessor::parameterChanged(const juce::String& id, float)
{
    // v11: switching ONE SHOT / LOCK ONE SHOT parks one state and brings back the other. From the screen this happens
    // right away (so quick clicks are never merged); from the host it happens a moment later on the message thread.
    if ((id == "oneshot" || id == "oslock") && !applyingSlot && juce::MessageManager::existsAndIsCurrentThread())
    {
        syncModes();
        triggerAsyncUpdate();
        return;
    }
    const bool keepBoxes = applyingSlot || isOneShotLocked(); // the boxes of the dots mode are not touched from the lock
    if ((id == "grid" || id == "bars" || id == "pick") && !keepBoxes) clearEnvPending.store(true); // v10: the boxes change, their own envelopes go
    if (id == "grid")
    {
        const int spb = stepsPerBarFor((int) raw("grid"));
        const int oldSpb = lastSpb;
        lastSpb = spb;
        if (applyingSlot || spb == oldSpb) { triggerAsyncUpdate(); return; } // a state coming back brings its own dots
        if (isOneShotLocked())
        {
            // v11: LOCK ONE SHOT - dots that keep their place keep their work, new dots get the clean one shot
            remapForGrid(oldSpb, spb, true);
            triggerAsyncUpdate();
            return;
        }
        DotEnv oldEnv[kPatternLen];
        for (int i = 0; i < kPatternLen; ++i) oldEnv[i] = extras.env[i];
        // keep the chosen rhythm when the grid changes, otherwise fall back to every step on
        if (currentFactory >= 0)
        {
            const auto& f = factoryPresets()[(size_t) currentFactory];
            resampleSteps(f.vel, f.steps, spb, pattern);
            resampleSteps(f.semi, f.steps, spb, extras.semi);
            resampleSteps(f.rev, f.steps, spb, extras.rev);
            resampleSteps(f.rep, f.steps, spb, extras.rep);
            resampleSteps(f.len, f.steps, spb, extras.len);
            applyPresetExtras(f, spb); // v12: balagan / wah / pan / copied chops of the Monkey presets
        }
        else if (currentRhythm >= 0) resampleRhythm(rhythms()[(size_t) currentRhythm], spb, pattern);
        else
        {
            for (int i = 0; i < kPatternLen; ++i) pattern[i] = i < spb ? 1.0f : 0.0f;
            extras = StepExtras();
        }
        // v11: the own envelope of a dot stays with the dot when it keeps its place in the bar
        for (int j = 0; j < kPatternLen; ++j)
        {
            const int i = j < spb ? sameSpot(j, oldSpb, spb) : -1;
            extras.env[j] = i >= 0 ? oldEnv[i] : DotEnv();
        }
    }
    triggerAsyncUpdate();
}

// v11: LOCK ONE SHOT + a new grid. Every dot of the new grid that sits where an old dot was keeps everything
// of it (level, extras, own envelope); every new place gets a dot that is on and plays the clean locked one shot.
void SlicerProcessor::remapForGrid(int oldSpb, int newSpb, bool)
{
    float np[kPatternLen] {};
    StepExtras ne;
    for (int j = 0; j < kPatternLen && j < newSpb; ++j)
    {
        const int i = sameSpot(j, oldSpb, newSpb);
        if (i < 0) { np[j] = 1.0f; continue; }
        np[j] = pattern[i];
        ne.semi[j] = extras.semi[i]; ne.rev[j] = extras.rev[i]; ne.rep[j] = extras.rep[i]; ne.len[j] = extras.len[i];
        ne.wah[j] = extras.wah[i]; ne.bal[j] = extras.bal[i]; ne.pan[j] = extras.pan[i]; ne.env[j] = extras.env[i];
        ne.src[j] = 0; // a copied chop is the same one shot anyway
    }
    std::copy(std::begin(np), std::end(np), pattern);
    extras = ne;
    currentRhythm = -1; currentFactory = -1;
}

// ------------------------------------------------------------------ v11: the three states
void SlicerProcessor::saveSlot(ModeSlot& sl, bool withDots)
{
    sl.valid = true;
    sl.hasDots = withDots;
    sl.knobs.clear();
    for (auto* id : kSlotKnobs)
        if (withDots || juce::String(id) != "grid") sl.knobs.push_back({ id, raw(id) });
    if (!withDots) return;
    std::copy(std::begin(pattern), std::end(pattern), sl.pattern);
    sl.extras = extras;
    sl.rhythm = currentRhythm;
    sl.factory = currentFactory;
}
void SlicerProcessor::loadSlot(const ModeSlot& sl)
{
    for (auto& [id, v] : sl.knobs) setParam(id.toRawUTF8(), v);
    if (!sl.hasDots) return;
    std::copy(std::begin(sl.pattern), std::end(sl.pattern), pattern);
    extras = sl.extras;
    currentRhythm = sl.rhythm;
    currentFactory = sl.factory;
}
void SlicerProcessor::startLockFromLive()
{
    lockSlot = ModeSlot();
    lockSlot.valid = true; // filled when the lock is left
    const int spb = stepsPerBarFor((int) raw("grid"));
    for (int i = 0; i < kPatternLen; ++i) pattern[i] = i < spb ? 1.0f : 0.0f;
    extras = StepExtras();
    currentRhythm = -1; currentFactory = -1;
}
void SlicerProcessor::syncModes()
{
    const int now = currentMode();
    if (now == lastMode) return;
    const int was = lastMode;
    lastMode = now;
    const juce::ScopedValueSetter<bool> busy(applyingSlot, true);
    // was the One Shot changed since the lock was left? (then LOCK ONE SHOT takes a new picture)
    bool sameAsLock = lockSlot.valid && !lockSlot.knobs.empty();
    if (sameAsLock)
        for (auto& [id, v] : lockSlot.knobs)
            if (id != "grid" && std::fabs(raw(id.toRawUTF8()) - v) > 1.0e-6f) sameAsLock = false;
    // park the state that is left
    if (was == modeDots) saveSlot(dotsSlot, true);
    else if (was == modeOneShot) saveSlot(oneShotSlot, false);
    else saveSlot(lockSlot, true);
    // bring back the state that is entered
    if (now == modeDots) { if (dotsSlot.valid) loadSlot(dotsSlot); }
    else if (now == modeOneShot) { if (was == modeDots && oneShotSlot.valid) loadSlot(oneShotSlot); } // from the lock: the same sound
    else if (was == modeOneShot && !sameAsLock) startLockFromLive();   // LOCK pressed in One Shot = a new picture
    else if (lockSlot.valid && !lockSlot.knobs.empty()) loadSlot(lockSlot); // back to the lock as it was left
    else startLockFromLive();
    lastSpb = stepsPerBarFor((int) raw("grid"));
    selDots = 0;
    selectedSeg = -1;
}

// v10: parameter changes can come from any thread; the follow-up work happens here, on the message thread
void SlicerProcessor::handleAsyncUpdate()
{
    syncModes(); // v11
    if (clearEnvPending.exchange(false)) clearSegEnvs();
    rebuildPlan();
}

void SlicerProcessor::rebuildPlan()
{
    const Params p = readParams();
    auto np = buildPlan(sample, p, pattern, manualCuts, &extras, &slips, &segEnvs);
    const juce::SpinLock::ScopedLockType l(planLock);
    plan = std::move(np);
}

void SlicerProcessor::reroll()
{
    setParam("seed", (float) (1 + (juce::Random::getSystemRandom().nextInt(9000))));
}

void SlicerProcessor::prepareToPlay(double sr, int blockSize)
{
    sampleRateHz = sr;
    scratchR.assign((size_t) juce::jmax(blockSize, 8192) + 64, 0.0f);
    engine.prepare(sr);
    engine.setInternalClock(isStandalone());
}

void SlicerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n == 0 || buffer.getNumChannels() < 1) return;

    engine.setParams(readParams());
    if (flushVoices.exchange(false)) engine.killVoices(); // v6: CLEAR
    {
        const juce::SpinLock::ScopedTryLockType l(planLock);
        if (l.isLocked() && plan != enginePlan) { enginePlan = plan; engine.setPlan(enginePlan); }
        if (l.isLocked() && lfoTable != engineLfo) { engineLfo = lfoTable; engine.setLfo(engineLfo); }
        if (l.isLocked() && layer != engineLayer) { engineLayer = layer; engine.setLayer(engineLayer); } // v7
    }
    bool has = false, playing = false; double ppq = 0.0, bpm = 0.0;
    // v5: Standalone runs on its own clock (the Tempo / BPM control). In Cubase this whole block is skipped.
    engine.setInternalClock(isStandalone());
    if (isStandalone()) lastBpm.store((double) raw("tempo"));
    else if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            has = true; playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) { bpm = *b; if (bpm > 1.0) lastBpm.store(bpm); }
            if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    engine.setHost(has, playing, ppq, bpm);

    keyboardState.processNextMidiBuffer(midi, 0, n, true);
    const double blockMs = juce::Time::getMillisecondCounterHiRes(); // v13: the playhead on screen goes on from this moment
    engine.beginBlock();
    if ((int) scratchR.size() < n) scratchR.assign((size_t) n, 0.0f);
    float* L = buffer.getWritePointer(0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : scratchR.data();

    int pos = 0;
    for (const auto meta : midi)
    {
        if (meta.samplePosition > pos)
        {
            engine.render(L + pos, R + pos, meta.samplePosition - pos);
            pos = meta.samplePosition;
        }
        const auto m = meta.getMessage();
        if (m.isNoteOn()) engine.noteOn(m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) engine.noteOff(m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    }
    if (pos < n) engine.render(L + pos, R + pos, n - pos);
    {
        // v13: where this block started in the loop. The sound of it comes out of the speakers about two blocks later,
        // so the screen waits that long (the playhead then moves with what is heard, not ahead of it).
        const auto a = engine.anchor();
        const double sr = getSampleRate() > 1000.0 ? getSampleRate() : 44100.0;
        const double lat = juce::jlimit(0.0, 0.12, 2.0 * (double) juce::jmax(n, getBlockSize()) / sr + (double) getLatencySamples() / sr);
        const juce::SpinLock::ScopedTryLockType l(anchorLock);
        if (l.isLocked()) playAnchor = { a.start, a.sps, blockMs, lat, sr, a.total, a.swing, a.running };
    }

    float pk = 0.0f;
    for (int i = 0; i < n; ++i) pk = juce::jmax(pk, std::fabs(L[i]), std::fabs(R[i]));
    outputPeak.store(pk, std::memory_order_relaxed);
}

// v13: the step that is heard now. The audio thread leaves where every block started; the screen goes on from there at
// the loop's speed, minus the time the sound needs to reach the speakers. Smooth, and never ahead of the sound.
double SlicerProcessor::getHeardStep(int& total) const
{
    total = 1;
    if (!engine.isActive()) return -1.0;
    PlayAnchor a;
    {
        const juce::SpinLock::ScopedLockType l(anchorLock);
        a = playAnchor;
    }
    if (!a.running || a.start < -1.0e8 || a.total < 1) return -1.0;
    total = a.total;
    const double elapsed = juce::jlimit(0.0, 0.25, (juce::Time::getMillisecondCounterHiRes() - a.ms) * 0.001); // never runs off on its own
    double pos = a.start + (elapsed - a.latency) * a.sps * a.sr;
    if (pos < 0.0) pos = 0.0;
    // swing: an odd step is heard a little later
    const double ns = std::floor(pos), frac = pos - ns;
    if (((long long) ns & 1) && frac < (double) a.swing * 0.33) pos = ns - 1.0 + 0.999;
    return std::fmod(std::fmod(pos, (double) a.total) + (double) a.total, (double) a.total);
}

juce::AudioProcessorEditor* SlicerProcessor::createEditor() { return new SlicerEditor(*this); }

// ------------------------------------------------------------------ sample
bool SlicerProcessor::loadSample(const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader || reader->lengthInSamples < 2048) return false;
    const int maxLen = (int) juce::jmin<juce::int64>(reader->lengthInSamples, (juce::int64) (reader->sampleRate * 120.0));
    juce::AudioBuffer<float> buf((int) juce::jmax<unsigned>(2u, reader->numChannels), maxLen);
    reader->read(&buf, 0, maxLen, 0, true, true);
    auto sd = std::make_shared<SampleData>();
    sd->sr = reader->sampleRate;
    sd->L.assign(buf.getReadPointer(0), buf.getReadPointer(0) + maxLen);
    sd->R.assign(buf.getReadPointer(reader->numChannels > 1 ? 1 : 0), buf.getReadPointer(reader->numChannels > 1 ? 1 : 0) + maxLen);
    sample = sd;
    samplePath = file.getFullPathName();
    sampleName = file.getFileName();
    sampleSeconds = (double) maxLen / sd->sr;
    slips.clear();
    clearSegEnvs(); // v10
    if (isOneShotLocked()) { const juce::ScopedValueSetter<bool> quiet(applyingSlot, true); setParam("oslock", 0.0f); } // v10: a new sample starts unlocked
    selDots = 0;
    if (isOneShot())
    {
        // One Shot: load the sample as it is - no bar guess, no chopping
        setParam("start", 0.0f);
        setParam("end", 1.0f);
        manualCuts.clear();
        resetSlots(); // v11: the saved states belonged to the old sample
        rebuildPlan();
        return true;
    }
    // suggest the bar count whose length is closest to the sample at the current tempo
    const double bpm = lastBpm.load() > 1.0 ? lastBpm.load() : (double) raw("tempo");
    const double barsExact = sampleSeconds * bpm / 240.0;
    int best = 0; double bestErr = 1.0e9;
    for (int i = 0; i < 4; ++i)
    {
        const double e = std::fabs(std::log2(std::max(0.01, barsExact) / (double) barsFor(i)));
        if (e < bestErr) { bestErr = e; best = i; }
    }
    suggestedBars = barsFor(best);
    setParam("bars", (float) best);
    setParam("start", 0.0f);
    setParam("end", 1.0f);
    manualCuts.clear();
    resetSlots(); // v11
    rebuildPlan();
    return true;
}

// ------------------------------------------------------------------ rhythm / pattern
juce::String SlicerProcessor::getRhythmName() const
{
    if (currentFactory >= 0) return juce::String(factoryPresets()[(size_t) currentFactory].name);
    return currentRhythm >= 0 ? juce::String(rhythms()[(size_t) currentRhythm].name) : juce::String("Custom");
}

void SlicerProcessor::applyRhythm(int index)
{
    const auto& all = rhythms();
    if (index < 0 || index >= (int) all.size()) return;
    currentRhythm = index;
    currentFactory = -1;
    extras = StepExtras();
    const auto& r = all[(size_t) index];
    setParam("grid", (float) r.grid);
    setParam("swing", r.swing);
    resampleRhythm(r, stepsPerBarFor(r.grid), pattern);
    rebuildPlan();
}

void SlicerProcessor::setStepVel(int i, float v)
{
    if (i < 0 || i >= kPatternLen) return;
    pattern[i] = juce::jlimit(0.0f, 1.0f, v);
    currentRhythm = -1;
    currentFactory = -1;
    rebuildPlan();
}

void SlicerProcessor::setStepSemi(int i, float semi)
{
    if (i < 0 || i >= kPatternLen) return;
    extras.semi[i] = juce::jlimit(-24.0f, 24.0f, semi);
    currentRhythm = -1; currentFactory = -1;
    rebuildPlan();
}
void SlicerProcessor::setStepRev(int i, bool on)
{
    if (i < 0 || i >= kPatternLen) return;
    extras.rev[i] = on ? 1 : 0;
    currentRhythm = -1; currentFactory = -1;
    rebuildPlan();
}
void SlicerProcessor::setStepRep(int i, bool on)
{
    if (i < 0 || i >= kPatternLen) return;
    extras.rep[i] = on ? 1 : 0;
    currentRhythm = -1; currentFactory = -1;
    rebuildPlan();
}
void SlicerProcessor::setStepLen(int i, int steps)
{
    if (i < 0 || i >= kPatternLen) return;
    extras.len[i] = (unsigned char) juce::jlimit(0, 8, steps);
    currentRhythm = -1; currentFactory = -1;
    rebuildPlan();
}

void SlicerProcessor::setStepWah(int i, bool on)
{
    if (i < 0 || i >= kPatternLen) return;
    extras.wah[i] = on ? 1 : 0;
    currentRhythm = -1; currentFactory = -1;
    rebuildPlan();
}

// v8: BALAGAN on / off for one dot (red dot), and the panning of one dot
void SlicerProcessor::setStepBal(int i, bool on)
{
    if (i < 0 || i >= kPatternLen) return;
    extras.bal[i] = on ? 1 : 0;
    currentRhythm = -1; currentFactory = -1;
    rebuildPlan();
}
void SlicerProcessor::setStepPan(int i, float pan)
{
    if (i < 0 || i >= kPatternLen) return;
    extras.pan[i] = juce::jlimit(-1.0f, 1.0f, std::fabs(pan) < 0.005f ? 0.0f : pan);
    currentRhythm = -1; currentFactory = -1;
    rebuildPlan();
}

// v7: Copy to next / all dots - the target dots switch on and play exactly the same chop
// (same audio, level, pitch, reverse, length and wah). Each copy can still be changed on its own afterwards.
void SlicerProcessor::copyStep(int from, int to)
{
    const int spb = stepsPerBarFor((int) raw("grid"));
    if (from < 0 || to < 0 || from >= spb || to >= spb || from == to) return;
    pattern[to] = pattern[from] > 0.001f ? pattern[from] : 1.0f;
    extras.semi[to] = extras.semi[from];
    extras.rev[to] = extras.rev[from];
    extras.len[to] = extras.len[from];
    extras.wah[to] = extras.wah[from];
    extras.bal[to] = extras.bal[from]; // v8
    extras.pan[to] = extras.pan[from];
    extras.env[to] = extras.env[from]; // v11
    extras.rep[to] = 0;
    // the chop of the source dot (if the source is itself a copy, follow it to the original)
    extras.src[to] = (unsigned char) (extras.src[from] > 0 ? extras.src[from] : from + 1);
    if (extras.src[to] == to + 1) extras.src[to] = 0;
}
// v12: the balagan / wah / pan / copied chops of a preset, fitted to the grid in use
void SlicerProcessor::applyPresetExtras(const FactoryPreset& f, int spb)
{
    resampleSteps(f.bal, f.steps, spb, extras.bal);
    resampleSteps(f.pan, f.steps, spb, extras.pan);
    resampleSteps(f.wah, f.steps, spb, extras.wah);
    resampleSteps(f.src, f.steps, spb, extras.src);
    // a copied chop points at a step of the preset's own grid: move it to the same place in this grid
    for (int i = 0; i < kPatternLen; ++i)
        if (extras.src[i] > 0)
        {
            const int s = (int) std::lround((double) (extras.src[i] - 1) * spb / juce::jmax(1, f.steps)) + 1;
            extras.src[i] = (unsigned char) (s == i + 1 ? 0 : juce::jlimit(0, juce::jmax(1, spb), s));
        }
}

// v12: chosen dots - every change goes to every chosen dot, in one go (the plan is built once)
void SlicerProcessor::editDots(uint32_t mask, const std::function<void(int, float&, bs::StepExtras&)>& fn)
{
    const int spb = stepsPerBarFor((int) raw("grid"));
    bool any = false;
    for (int i = 0; i < spb && i < kPatternLen; ++i)
        if (mask & (1u << i)) { fn(i, pattern[i], extras); any = true; }
    if (!any) return;
    for (auto& v : pattern) v = juce::jlimit(0.0f, 1.0f, v);
    currentRhythm = -1; currentFactory = -1;
    rebuildPlan();
}

void SlicerProcessor::copyStepToNext(int i)
{
    const int spb = stepsPerBarFor((int) raw("grid"));
    if (i < 0 || i + 1 >= spb) return;
    copyStep(i, i + 1);
    currentRhythm = -1; currentFactory = -1;
    rebuildPlan();
}
void SlicerProcessor::copyStepToAll(int i)
{
    const int spb = stepsPerBarFor((int) raw("grid"));
    if (i < 0 || i >= spb) return;
    for (int k = 0; k < spb; ++k) if (k != i) copyStep(i, k);
    currentRhythm = -1; currentFactory = -1;
    rebuildPlan();
}
void SlicerProcessor::setStepSrc(int i, int src)
{
    if (i < 0 || i >= kPatternLen) return;
    extras.src[i] = (unsigned char) juce::jlimit(0, kPatternLen, src);
    currentRhythm = -1; currentFactory = -1;
    rebuildPlan();
}

// v9: "Default - start from scratch" in the preset list: the whole plugin as if it was just opened.
// Every parameter back to its default, no LFO routing, no sample, no layer, no dot extras, grid 1/4 with four dots on at 0 dB.
void SlicerProcessor::applyCustom()
{
    currentRhythm = -1;   // first: the grid listener then keeps every dot on and clears the extras
    currentFactory = -1;
    clearSample();
    clearLayer();
    layerLinks.store(0); // v13
    clearMods();
    setLfoPoints(lfoPreset(0));
    const juce::ScopedValueSetter<bool> quiet(applyingSlot, true); // v11: no state switching while everything is reset
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(prm))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost(rp->getDefaultValue());
            rp->endChangeGesture();
        }
    setParam("grid", 0.0f); // 1/4
    extras = StepExtras();
    slips.clear();
    manualCuts.clear();
    clearSegEnvs();               // v10
    exportMono = false;
    for (int i = 0; i < kPatternLen; ++i) pattern[i] = i < stepsPerBarFor(0) ? 1.0f : 0.0f;
    presetName = "Default";
    resetSlots(); // v11
    selDots = 0;
    flushVoices.store(true);
    rebuildPlan();
}

// v7: ADD LAYER - a sound blended into every chop
bool SlicerProcessor::loadLayer(const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader || reader->lengthInSamples < 64) return false;
    const int len = (int) juce::jmin<juce::int64>(reader->lengthInSamples, (juce::int64) (reader->sampleRate * 60.0));
    juce::AudioBuffer<float> buf((int) juce::jmax<unsigned>(2u, reader->numChannels), len);
    reader->read(&buf, 0, len, 0, true, true);
    auto sd = std::make_shared<SampleData>();
    sd->sr = reader->sampleRate;
    const int rc = reader->numChannels > 1 ? 1 : 0;
    sd->L.assign(buf.getReadPointer(0), buf.getReadPointer(0) + len);
    sd->R.assign(buf.getReadPointer(rc), buf.getReadPointer(rc) + len);
    layerPath = file.getFullPathName();
    // v13: the silence before the sound is skipped, so the hit of the layer lands exactly with the chop
    const float st = (float) ((double) bs::soundStart(*sd) / sd->sr);
    layerAutoStart.store(st);
    layerStart.store(st);
    const juce::SpinLock::ScopedLockType l(planLock);
    layer = sd;
    return true;
}
// v13: drag the layer left / right (seconds into the sound; below 0 = it waits, at most half a second)
void SlicerProcessor::setLayerStart(float sec)
{
    float len = 0.0f;
    {
        const juce::SpinLock::ScopedLockType l(planLock);
        if (layer) len = (float) ((double) layer->size() / layer->sr);
    }
    layerStart.store(juce::jlimit(-0.5f, juce::jmax(0.0f, len - 0.01f), sec));
}
void SlicerProcessor::autoAlignLayer() { layerStart.store(layerAutoStart.load()); }
void SlicerProcessor::clearLayer()
{
    layerPath = {};
    layerStart.store(0.0f); layerAutoStart.store(0.0f); // v13
    const juce::SpinLock::ScopedLockType l(planLock);
    layer = nullptr;
}

// v6: CLEAR - the waveform is emptied and waits for a new sample
void SlicerProcessor::clearSample()
{
    sample = nullptr;
    samplePath = {};
    sampleName = {};
    sampleSeconds = 0.0;
    slips.clear();
    manualCuts.clear();
    clearSegEnvs(); // v10
    if (isOneShotLocked()) { const juce::ScopedValueSetter<bool> quiet(applyingSlot, true); setParam("oslock", 0.0f); }
    setParam("start", 0.0f);
    setParam("end", 1.0f);
    resetSlots(); // v11
    selDots = 0;
    flushVoices.store(true);
    rebuildPlan();
}

// v10: RESET SAMPLE - everything done to the sample goes, the sample itself stays loaded.
// Markers back to the whole sample, LOCK ONE SHOT off, every dot on at 0 dB, no dot extras, no slip,
// no manual cuts, no own slice envelopes. Knobs, LFO, effects, grid and bars are not touched.
void SlicerProcessor::resetSample()
{
    if (isOneShotLocked()) { const juce::ScopedValueSetter<bool> quiet(applyingSlot, true); setParam("oslock", 0.0f); }
    setParam("start", 0.0f);
    setParam("end", 1.0f);
    const int spb = stepsPerBarFor((int) raw("grid"));
    for (int i = 0; i < kPatternLen; ++i) pattern[i] = i < spb ? 1.0f : 0.0f;
    extras = StepExtras();
    slips.clear();
    manualCuts.clear();
    clearSegEnvs();
    currentRhythm = -1;
    currentFactory = -1;
    resetSlots(); // v11
    selDots = 0;
    rebuildPlan();
}

// v10: the selection box on the dots - every selected dot on (0 dB) or off at once
void SlicerProcessor::setStepsOn(uint32_t mask, bool on)
{
    const int spb = stepsPerBarFor((int) raw("grid"));
    bool any = false;
    for (int i = 0; i < spb && i < kPatternLen; ++i)
        if (mask & (1u << i))
        {
            if (on && pattern[i] > 0.001f) continue; // a dot that is already on keeps its level
            pattern[i] = on ? 1.0f : 0.0f;
            any = true;
        }
    if (!any) return;
    currentRhythm = -1; currentFactory = -1;
    rebuildPlan();
}

bs::SegEnv SlicerProcessor::getSegEnv(int seg) const
{
    if (segHasOwnEnv(seg)) return segEnvs[(size_t) seg];
    SegEnv e;
    e.attackMs = raw("attack"); e.decayMs = raw("decay"); e.sustain = raw("sustain"); e.releaseMs = raw("release");
    return e;
}

void SlicerProcessor::setSegEnvValue(int seg, int which, float value)
{
    if (seg < 0 || seg >= kMaxSteps) return;
    if ((int) segEnvs.size() <= seg) segEnvs.resize((size_t) seg + 1);
    auto& e = segEnvs[(size_t) seg];
    if (!e.on) { e = getSegEnv(seg); e.on = true; } // the first touch starts from what the slice played until now
    switch (which)
    {
        case 0: e.attackMs = juce::jlimit(0.1f, 500.0f, value); break;
        case 1: e.decayMs = juce::jlimit(5.0f, 3000.0f, value); break;
        case 2: e.sustain = juce::jlimit(0.0f, 1.0f, value); break;
        default: e.releaseMs = juce::jlimit(5.0f, 3000.0f, value); break;
    }
    rebuildPlan();
}

// ------------------------------------------------------------------ v11: own envelope per dot
float SlicerProcessor::getDotEnvValue(int dot, int which) const
{
    which = juce::jlimit(0, kDotKnobCount - 1, which);
    if (dot < 0 || dot >= kPatternLen) return raw(kDotEnvIds[which]);
    const auto& e = extras.env[dot];
    if (e.has(which)) return e.v[which];
    if (which <= kDotEnvRel && !isOneShotLocked())
    {
        // the box this dot plays may have its own envelope (SLICE lamp)
        auto pl = getPlan();
        if (pl && dot < pl->totalSteps() && pl->slices[(size_t) dot].env.on)
        {
            const auto& se = pl->slices[(size_t) dot].env;
            return which == kDotEnvAtk ? se.attackMs : which == kDotEnvDec ? se.decayMs : which == kDotEnvSus ? se.sustain : se.releaseMs;
        }
    }
    return raw(kDotEnvIds[which]);
}
float SlicerProcessor::getDotsShownValue(uint32_t mask, int which) const
{
    const int spb = stepsPerBarFor((int) raw("grid"));
    bool any = false;
    float best = 0.0f;
    for (int i = 0; i < spb && i < kPatternLen; ++i)
        if (mask & (1u << i))
        {
            const float v = getDotEnvValue(i, which);
            best = any ? juce::jmax(best, v) : v;
            any = true;
        }
    return any ? best : raw(kDotEnvIds[juce::jlimit(0, kDotKnobCount - 1, which)]);
}
// every chosen dot moves by the same amount of knob turn, each from where it is now (so the differences stay)
void SlicerProcessor::moveDotsEnv(uint32_t mask, int which, float fromValue, float toValue)
{
    which = juce::jlimit(0, kDotKnobCount - 1, which);
    auto* prm = apvts.getParameter(kDotEnvIds[which]);
    if (prm == nullptr) return;
    const float delta = prm->convertTo0to1(toValue) - prm->convertTo0to1(fromValue);
    if (std::fabs(delta) < 1.0e-7f) return;
    const int spb = stepsPerBarFor((int) raw("grid"));
    for (int i = 0; i < spb && i < kPatternLen; ++i)
        if (mask & (1u << i))
        {
            const float n = juce::jlimit(0.0f, 1.0f, prm->convertTo0to1(getDotEnvValue(i, which)) + delta);
            extras.env[i].v[which] = prm->convertFrom0to1(n);
        }
    rebuildPlan();
}
void SlicerProcessor::clearDotEnv(int dot)
{
    if (dot < 0 || dot >= kPatternLen) return;
    extras.env[dot] = DotEnv();
    rebuildPlan();
}

// v12: the dot's own locators - where its chop starts and ends (0..1 of the chop; not set = 0 / 1)
float SlicerProcessor::getDotTrim(int dot, int which) const
{
    if (dot < 0 || dot >= kPatternLen) return which == kDotEnd ? 1.0f : 0.0f;
    const auto& e = extras.env[dot];
    return which == kDotEnd ? e.trimB() : e.trimA();
}
void SlicerProcessor::setDotTrims(uint32_t mask, int which, const float* values)
{
    if (which != kDotStart && which != kDotEnd) return;
    const int spb = stepsPerBarFor((int) raw("grid"));
    for (int i = 0; i < spb && i < kPatternLen; ++i)
    {
        if (!(mask & (1u << i))) continue;
        auto& e = extras.env[i];
        float v = juce::jlimit(0.0f, 1.0f, values[i]);
        if (which == kDotStart) v = juce::jmin(v, e.trimB() - 0.02f);
        else v = juce::jmax(v, e.trimA() + 0.02f);
        const bool atEdge = which == kDotStart ? v <= 1.0e-4f : v >= 1.0f - 1.0e-4f;
        e.v[which] = atEdge ? kDotUnset : v; // back at the edge = the locator is not set any more
    }
    currentRhythm = -1; currentFactory = -1;
    rebuildPlan();
}

void SlicerProcessor::applyFactoryPreset(int index)
{
    const auto& all = factoryPresets();
    if (index < 0 || index >= (int) all.size()) return;
    const auto& f = all[(size_t) index];
    currentFactory = index;   // set first: the grid listener re-reads the preset for the new grid
    currentRhythm = -1;
    {
        const juce::ScopedValueSetter<bool> quiet(applyingSlot, true);
        setParam("oneshot", 0.0f);
        setParam("oslock", 0.0f); // v11: a factory sound starts in the dots, without a lock (the saved states are dropped below)
    }
    // the factory sound is defined without LFO routing, unison or chorus
    clearMods();
    setParam("univoices", 1.0f);
    setParam("chomix", 0.0f);
    setParam("wah", 0.0f);                                  // v6: factory presets have no wah
    for (auto& w : extras.wah) w = 0;
    for (auto& c : extras.src) c = 0;                       // v7: no copied chops in factory presets
    for (auto& c : extras.bal) c = 0;                       // v8: no balagan / pan in factory presets
    for (auto& c : extras.pan) c = 0.0f;
    setParam("balsuper", 0.0f);
    setParam("balfilter", 0.0f);
    setParam("balmode", 0.0f);                              // v12: BALAGAN back to FREE (a Monkey preset sets it below)
    setParam("grid", (float) f.grid);
    for (const auto& k : f.knobs) setParam(k.id, k.value);
    const int spb = stepsPerBarFor(f.grid);
    resampleSteps(f.vel, f.steps, spb, pattern);
    resampleSteps(f.semi, f.steps, spb, extras.semi);
    resampleSteps(f.rev, f.steps, spb, extras.rev);
    resampleSteps(f.rep, f.steps, spb, extras.rep);
    resampleSteps(f.len, f.steps, spb, extras.len);
    applyPresetExtras(f, spb);                              // v12
    if (f.lfoShape >= 0) setLfoPoints(lfoPreset(f.lfoShape));
    for (const auto& s : f.lfo) { addMod(s.dest); setModAmount(s.dest, s.amount); }
    for (const auto& s : f.env) { addEnvMod(s.dest); setEnvModAmount(s.dest, s.amount); }
    presetName = f.name;
    clearSegEnvs(); // v10: factory sounds have no own slice envelopes
    for (auto& e : extras.env) e = DotEnv(); // v11: and no own dot envelopes
    resetSlots();
    selDots = 0;
    rebuildPlan();
}

// ------------------------------------------------------------------ LFO
void SlicerProcessor::setLfoPoints(const std::vector<LfoPoint>& pts)
{
    lfoPoints = pts;
    std::stable_sort(lfoPoints.begin(), lfoPoints.end(), [](const LfoPoint& a, const LfoPoint& b) { return a.x < b.x; });
    std::shared_ptr<const LfoTable> t = buildLfoTable(lfoPoints);
    const juce::SpinLock::ScopedLockType l(planLock);
    lfoTable = std::move(t);
}
void SlicerProcessor::addMod(int d)
{
    if (d < 0 || d >= kModCount) return;
    if (!modOn[d].load()) { modAmt[d].store(0.5f); modOn[d].store(true); }
    ++modVersion;
}
void SlicerProcessor::setModAmount(int d, float a)
{
    if (d < 0 || d >= kModCount) return;
    modAmt[d].store(juce::jlimit(-1.0f, 1.0f, a));
    ++modVersion;
}
void SlicerProcessor::removeMod(int d)
{
    if (d < 0 || d >= kModCount) return;
    modOn[d].store(false);
    ++modVersion;
}
void SlicerProcessor::clearMods()
{
    for (int d = 0; d < kModCount; ++d) { modOn[d].store(false); envOn[d].store(false); } // v12: the ENV routing goes too
    ++modVersion;
}
// v12: ENV routing (only the knobs where it is useful: Cutoff, Pitch, Noise, Balagan, Bal. Filter)
void SlicerProcessor::addEnvMod(int d)
{
    if (!envDestAllowed(d)) return;
    if (!envOn[d].load()) { envAmt[d].store(0.5f); envOn[d].store(true); }
    ++modVersion;
}
void SlicerProcessor::setEnvModAmount(int d, float a)
{
    if (d < 0 || d >= kModCount) return;
    envAmt[d].store(juce::jlimit(-1.0f, 1.0f, a));
    ++modVersion;
}
void SlicerProcessor::removeEnvMod(int d)
{
    if (d < 0 || d >= kModCount) return;
    envOn[d].store(false);
    ++modVersion;
}

// ------------------------------------------------------------------ render to file (drag out)
juce::File SlicerProcessor::getExportDir()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("BeatSlicer").getChildFile("Exports");
}

juce::File SlicerProcessor::renderToFile(bool mono)
{
    auto pl = getPlan();
    if (!pl || !pl->sample) return {};
    std::shared_ptr<const LfoTable> lt;
    { const juce::SpinLock::ScopedLockType l(planLock); lt = lfoTable; }
    const double sr = sampleRateHz > 1000.0 ? sampleRateHz : 44100.0;
    const double bpm = lastBpm.load() > 1.0 ? lastBpm.load() : (double) raw("tempo");
    Params p = readParams();
    if (p.oneShot) p.latch = 0; // v13: LOOP in One Shot would play it forever - DRAG TO DAW makes one pass
    Engine e;
    e.prepare(sr);
    e.setParams(p);
    e.setPlan(pl);
    e.setLfo(lt);
    { const juce::SpinLock::ScopedLockType l(planLock); e.setLayer(layer); } // v7
    constexpr int blk = 256;
    juce::AudioBuffer<float> all;
    if (p.oneShot)
    {
        // one shot: play the key once and keep everything until it is silent
        const int maxLen = (int) (sr * 30.0);
        all.setSize(2, maxLen);
        all.clear();
        e.setHost(false, false, 0.0, 0.0);
        e.noteOn(60, 1.0f);
        int pos = 0;
        while (pos < maxLen)
        {
            const int n = juce::jmin(blk, maxLen - pos);
            e.render(all.getWritePointer(0, pos), all.getWritePointer(1, pos), n);
            pos += n;
            if (!e.anyVoiceActive()) break;
        }
        int end = pos;
        while (end > 1 && std::fabs(all.getSample(0, end - 1)) < 1.0e-5f && std::fabs(all.getSample(1, end - 1)) < 1.0e-5f) --end;
        all.setSize(2, juce::jmax(1, end), true);
    }
    else
    {
        // loop: render it twice and keep the second pass, so the tails of the end wrap into the start
        // and the file loops perfectly at exactly bars x 4 beats
        const int loopLen = (int) std::round((double) pl->bars * 4.0 * 60.0 / bpm * sr);
        juce::AudioBuffer<float> two(2, loopLen * 2);
        two.clear();
        if (p.mode == 0) e.noteOn(60, 1.0f);
        for (int pos = 0; pos < loopLen * 2; pos += blk)
        {
            const int n = juce::jmin(blk, loopLen * 2 - pos);
            e.setHost(true, true, (double) pos / sr * bpm / 60.0, bpm);
            e.render(two.getWritePointer(0, pos), two.getWritePointer(1, pos), n);
        }
        all.setSize(2, loopLen);
        for (int c = 0; c < 2; ++c) all.copyFrom(c, 0, two, c, loopLen, loopLen);
    }
    if (mono)
    {
        juce::AudioBuffer<float> m(1, all.getNumSamples());
        m.copyFrom(0, 0, all, 0, 0, all.getNumSamples());
        m.addFrom(0, 0, all, 1, 0, all.getNumSamples());
        m.applyGain(0.5f);
        all.makeCopyOf(m);
    }
    auto dir = getExportDir();
    if (!dir.createDirectory()) return {};
    juce::String base = presetName != "Init" && presetName != "Default" && presetName.isNotEmpty() ? presetName : juce::File(sampleName).getFileNameWithoutExtension();
    if (base.isEmpty()) base = "Psycho Beat Lab"; // v9: new name
    base = juce::File::createLegalFileName(base + (p.oneShot ? " one shot" : " " + juce::String(juce::roundToInt(bpm)) + "bpm") + (mono ? " mono" : ""));
    auto file = dir.getNonexistentChildFile(base, ".wav", false);
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> os(file.createOutputStream());
    if (!os) return {};
    std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(os.get(), sr, (unsigned) all.getNumChannels(), 24, {}, 0));
    if (!w) return {};
    os.release();
    w->writeFromAudioSampleBuffer(all, 0, all.getNumSamples());
    w.reset();
    return file;
}

void SlicerProcessor::setSlip(int seg, float normOffset)
{
    if (seg < 0 || seg >= kMaxSteps) return;
    if ((int) slips.size() <= seg) slips.resize((size_t) seg + 1, 0.0f);
    slips[(size_t) seg] = juce::jlimit(-1.0f, 1.0f, normOffset);
    rebuildPlan();
}

void SlicerProcessor::addManualCut(float norm)
{
    manualCuts.push_back(juce::jlimit(0.0f, 1.0f, norm));
    rebuildPlan();
}

void SlicerProcessor::removeManualCut(float norm, float tol)
{
    manualCuts.erase(std::remove_if(manualCuts.begin(), manualCuts.end(), [&](float c) { return std::fabs(c - norm) < tol; }), manualCuts.end());
    rebuildPlan();
}

// ------------------------------------------------------------------ state
// v11: the dots (levels + every extra of every dot) as properties of a tree - used for the state and for the saved states
void SlicerProcessor::writeDots(juce::ValueTree& st, const float* pat, const StepExtras& ex)
{
    juce::StringArray pa, se, rv, rp, ln, wh, sr2, bl, pn, en;
    for (int i = 0; i < kPatternLen; ++i)
    {
        pa.add(juce::String(pat[i], 6)); // v5: finer (dots go down to -48 dB)
        se.add(juce::String(ex.semi[i], 2)); rv.add(juce::String((int) ex.rev[i]));
        rp.add(juce::String((int) ex.rep[i])); ln.add(juce::String((int) ex.len[i]));
        wh.add(juce::String((int) ex.wah[i])); sr2.add(juce::String((int) ex.src[i]));
        bl.add(juce::String((int) ex.bal[i])); pn.add(juce::String(ex.pan[i], 3));
        juce::StringArray e;
        for (float v : ex.env[i].v) e.add(!DotEnv::isSet(v) ? juce::String("-") : juce::String(v, 4)); // v12: 12 values (Pitch can be < 0)
        en.add(e.joinIntoString(":"));
    }
    st.setProperty("pattern", pa.joinIntoString(","), nullptr);
    st.setProperty("stepSemi", se.joinIntoString(","), nullptr);
    st.setProperty("stepRev", rv.joinIntoString(","), nullptr);
    st.setProperty("stepRep", rp.joinIntoString(","), nullptr);
    st.setProperty("stepLen", ln.joinIntoString(","), nullptr);
    st.setProperty("stepWah", wh.joinIntoString(","), nullptr);
    st.setProperty("stepSrc", sr2.joinIntoString(","), nullptr); // v7
    st.setProperty("stepBal", bl.joinIntoString(","), nullptr);  // v8
    st.setProperty("stepPan", pn.joinIntoString(","), nullptr);
    st.setProperty("stepEnv", en.joinIntoString(","), nullptr);  // v11
}
// returns false when the tree has no dots (older projects keep their fallback)
void SlicerProcessor::readDots(const juce::ValueTree& st, float* pat, StepExtras& ex)
{
    ex = StepExtras();
    auto readArr = [&](const char* key, auto setter)
    {
        juce::StringArray a;
        a.addTokens(st.getProperty(key, "").toString(), ",", "");
        if (a.size() == kPatternLen) for (int i = 0; i < kPatternLen; ++i) setter(i, a[i]);
    };
    readArr("pattern", [&](int i, const juce::String& v) { pat[i] = v.getFloatValue(); });
    readArr("stepSemi", [&](int i, const juce::String& v) { ex.semi[i] = v.getFloatValue(); });
    readArr("stepRev", [&](int i, const juce::String& v) { ex.rev[i] = (unsigned char) v.getIntValue(); });
    readArr("stepRep", [&](int i, const juce::String& v) { ex.rep[i] = (unsigned char) v.getIntValue(); });
    readArr("stepLen", [&](int i, const juce::String& v) { ex.len[i] = (unsigned char) v.getIntValue(); });
    readArr("stepWah", [&](int i, const juce::String& v) { ex.wah[i] = (unsigned char) v.getIntValue(); });
    readArr("stepSrc", [&](int i, const juce::String& v) { ex.src[i] = (unsigned char) v.getIntValue(); });
    readArr("stepBal", [&](int i, const juce::String& v) { ex.bal[i] = (unsigned char) v.getIntValue(); }); // v8
    readArr("stepPan", [&](int i, const juce::String& v) { ex.pan[i] = juce::jlimit(-1.0f, 1.0f, v.getFloatValue()); });
    readArr("stepEnv", [&](int i, const juce::String& v) // v11
    {
        juce::StringArray f;
        f.addTokens(v, ":", "");
        if (f.size() != 5 && f.size() != kDotEnvCount) return; // v11 projects have 5 values, v12 has 12
        for (int k = 0; k < f.size(); ++k)
            ex.env[i].v[k] = f[k] == "-" ? kDotUnset : (k == kDotPitch ? juce::jlimit(-12.0f, 12.0f, f[k].getFloatValue())
                                                                        : juce::jmax(0.0f, f[k].getFloatValue()));
    });
}
void SlicerProcessor::writeSlot(juce::ValueTree& t, const ModeSlot& sl)
{
    t.setProperty("valid", sl.valid, nullptr);
    t.setProperty("hasDots", sl.hasDots, nullptr);
    t.setProperty("rhythm", sl.rhythm, nullptr);
    t.setProperty("factory", sl.factory, nullptr);
    juce::StringArray k;
    for (auto& [id, v] : sl.knobs) k.add(id + "=" + juce::String(v, 6));
    t.setProperty("knobs", k.joinIntoString(";"), nullptr);
    if (sl.hasDots) writeDots(t, sl.pattern, sl.extras);
}
void SlicerProcessor::readSlot(const juce::ValueTree& t, ModeSlot& sl)
{
    sl = ModeSlot();
    if (!t.isValid()) return;
    sl.valid = (bool) t.getProperty("valid", false);
    sl.hasDots = (bool) t.getProperty("hasDots", false);
    sl.rhythm = (int) t.getProperty("rhythm", -1);
    sl.factory = (int) t.getProperty("factory", -1);
    if (sl.factory >= (int) factoryPresets().size()) sl.factory = -1;
    if (sl.rhythm >= (int) rhythms().size()) sl.rhythm = -1;
    juce::StringArray k;
    k.addTokens(t.getProperty("knobs", "").toString(), ";", "");
    for (auto& kv : k)
        if (kv.contains("=")) sl.knobs.push_back({ kv.upToFirstOccurrenceOf("=", false, false), kv.fromFirstOccurrenceOf("=", false, false).getFloatValue() });
    if (sl.hasDots) readDots(t, sl.pattern, sl.extras);
}

void SlicerProcessor::storeStateExtras()
{
    auto& st = apvts.state;
    st.setProperty("samplePath", samplePath, nullptr);
    st.setProperty("rhythm", currentRhythm, nullptr);
    st.setProperty("presetName", presetName, nullptr);
    writeDots(st, pattern, extras);
    juce::StringArray cuts;
    for (float c : manualCuts) cuts.add(juce::String(c, 5));
    st.setProperty("cuts", cuts.joinIntoString(","), nullptr);
    st.setProperty("factory", currentFactory, nullptr);
    st.setProperty("layerPath", layerPath, nullptr);
    st.setProperty("layerStart", layerStart.load(), nullptr);         // v13
    st.setProperty("layerAutoStart", layerAutoStart.load(), nullptr);
    st.setProperty("layerLinks", layerLinks.load(), nullptr);
    juce::StringArray sl;
    for (float v : slips) sl.add(juce::String(v, 6));
    st.setProperty("slips", sl.joinIntoString(","), nullptr);
    // v4
    juce::StringArray lp, md;
    for (auto& pt : lfoPoints) lp.add(juce::String(pt.x, 5) + ":" + juce::String(pt.y, 5) + ":" + juce::String(pt.curve, 4));
    st.setProperty("lfoPts", lp.joinIntoString(";"), nullptr);
    for (int d = 0; d < kModCount; ++d)
        if (modOn[d].load()) md.add(juce::String(modInfo(d).id) + ":" + juce::String(modAmt[d].load(), 4));
    st.setProperty("mods", md.joinIntoString(";"), nullptr);
    juce::StringArray em; // v12: ENV routing
    for (int d = 0; d < kModCount; ++d)
        if (envOn[d].load()) em.add(juce::String(modInfo(d).id) + ":" + juce::String(envAmt[d].load(), 4));
    st.setProperty("envMods", em.joinIntoString(";"), nullptr);
    st.setProperty("exportMono", exportMono, nullptr);
    // v10: own slice envelopes ("seg:attack:decay:sustain:release;...")
    juce::StringArray se2;
    for (size_t k = 0; k < segEnvs.size(); ++k)
        if (segEnvs[k].on)
            se2.add(juce::String((int) k) + ":" + juce::String(segEnvs[k].attackMs, 3) + ":" + juce::String(segEnvs[k].decayMs, 2) + ":"
                    + juce::String(segEnvs[k].sustain, 4) + ":" + juce::String(segEnvs[k].releaseMs, 2));
    st.setProperty("segEnv", se2.joinIntoString(";"), nullptr);
    // v11: the parked states (dots / One Shot / LOCK ONE SHOT), so the lock and its work come back with the project and the preset
    for (auto old = st.getChildWithName("V11STATES"); old.isValid(); old = st.getChildWithName("V11STATES")) st.removeChild(old, nullptr);
    juce::ValueTree states("V11STATES");
    const std::pair<const char*, const ModeSlot*> all[] = { { "DOTS", &dotsSlot }, { "ONESHOT", &oneShotSlot }, { "LOCK", &lockSlot } };
    for (auto& [name, sl2] : all)
    {
        juce::ValueTree t(name);
        writeSlot(t, *sl2);
        states.appendChild(t, nullptr);
    }
    st.appendChild(states, nullptr);
}

void SlicerProcessor::restoreFromState()
{
    auto& st = apvts.state;
    currentRhythm = (int) st.getProperty("rhythm", 1);
    presetName = st.getProperty("presetName", "Init").toString();
    {
        juce::StringArray pat;
        pat.addTokens(st.getProperty("pattern", "").toString(), ",", "");
        readDots(st, pattern, extras);
        if (pat.size() != kPatternLen) resampleRhythm(rhythms()[1], stepsPerBarFor((int) raw("grid")), pattern);
    }
    manualCuts.clear();
    juce::StringArray cuts;
    cuts.addTokens(st.getProperty("cuts", "").toString(), ",", "");
    for (auto& c : cuts) if (c.isNotEmpty()) manualCuts.push_back(c.getFloatValue());
    currentFactory = (int) st.getProperty("factory", -1);
    if (currentFactory >= (int) factoryPresets().size()) currentFactory = -1;
    {
        // v7: the layer sound comes back with the project (if the file is still there)
        const juce::String lp = st.getProperty("layerPath", "").toString();
        if (lp.isNotEmpty() && juce::File(lp).existsAsFile()) loadLayer(juce::File(lp));
        else if (lp.isEmpty() && st.hasProperty("layerPath")) clearLayer();
        // v13: the layer's start point and links. Older projects: the layer starts at its top, as it did then.
        if (layer != nullptr)
        {
            if (st.hasProperty("layerStart")) layerStart.store((float) st.getProperty("layerStart"));
            else layerStart.store(0.0f);
            if (st.hasProperty("layerAutoStart")) layerAutoStart.store((float) st.getProperty("layerAutoStart"));
        }
        layerLinks.store((int) st.getProperty("layerLinks", 0));
    }
    std::vector<float> slipsKeep;
    {
        juce::StringArray a;
        a.addTokens(st.getProperty("slips", "").toString(), ",", "");
        for (auto& v : a) if (v.isNotEmpty()) slipsKeep.push_back(v.getFloatValue());
    }
    slips = slipsKeep;
    {
        std::vector<LfoPoint> pts;
        juce::StringArray a;
        a.addTokens(st.getProperty("lfoPts", "").toString(), ";", "");
        for (auto& t : a)
        {
            juce::StringArray f;
            f.addTokens(t, ":", "");
            if (f.size() == 3) pts.push_back({ f[0].getFloatValue(), f[1].getFloatValue(), f[2].getFloatValue() });
        }
        setLfoPoints(pts.size() >= 2 ? pts : lfoPreset(0));
        for (int d = 0; d < kModCount; ++d) modOn[d].store(false);
        juce::StringArray m;
        m.addTokens(st.getProperty("mods", "").toString(), ";", "");
        for (auto& t : m)
        {
            const int d = modIndexOf(t.upToFirstOccurrenceOf(":", false, false).toStdString());
            if (d >= 0) { modAmt[d].store(t.fromFirstOccurrenceOf(":", false, false).getFloatValue()); modOn[d].store(true); }
        }
        // v12: ENV routing (older projects have none)
        for (int d = 0; d < kModCount; ++d) envOn[d].store(false);
        juce::StringArray em;
        em.addTokens(st.getProperty("envMods", "").toString(), ";", "");
        for (auto& t : em)
        {
            const int d = modIndexOf(t.upToFirstOccurrenceOf(":", false, false).toStdString());
            if (d >= 0 && envDestAllowed(d)) { envAmt[d].store(t.fromFirstOccurrenceOf(":", false, false).getFloatValue()); envOn[d].store(true); }
        }
        ++modVersion;
        exportMono = (bool) st.getProperty("exportMono", false);
    }
    // v10
    std::vector<SegEnv> envKeep;
    {
        juce::StringArray a;
        a.addTokens(st.getProperty("segEnv", "").toString(), ";", "");
        for (auto& t : a)
        {
            juce::StringArray f;
            f.addTokens(t, ":", "");
            if (f.size() != 5) continue;
            const int k = f[0].getIntValue();
            if (k < 0 || k >= kMaxSteps) continue;
            if ((int) envKeep.size() <= k) envKeep.resize((size_t) k + 1);
            envKeep[(size_t) k] = { true, f[1].getFloatValue(), f[2].getFloatValue(), f[3].getFloatValue(), f[4].getFloatValue() };
        }
    }
    // v11: the parked states (read before the sample loads, kept aside, put back after it)
    ModeSlot dKeep, oKeep, lKeep;
    {
        const auto states = st.getChildWithName("V11STATES");
        readSlot(states.getChildWithName("DOTS"), dKeep);
        readSlot(states.getChildWithName("ONESHOT"), oKeep);
        readSlot(states.getChildWithName("LOCK"), lKeep);
    }
    const juce::String path = st.getProperty("samplePath", "").toString();
    if (path.isNotEmpty() && juce::File(path).existsAsFile())
    {
        const auto keepBars = raw("bars"), keepS = raw("start"), keepE = raw("end"), keepLock = raw("oslock");
        const auto cutsKeep = manualCuts;
        float patKeep[kPatternLen];
        std::copy(std::begin(pattern), std::end(pattern), patKeep);
        const StepExtras exKeep = extras;
        loadSample(juce::File(path));
        setParam("bars", keepBars); setParam("start", keepS); setParam("end", keepE);
        if (keepLock > 0.5f) setParam("oslock", 1.0f);
        manualCuts = cutsKeep;
        slips = slipsKeep;
        std::copy(std::begin(patKeep), std::end(patKeep), pattern);
        extras = exKeep;
    }
    segEnvs = envKeep;
    selectedSeg = -1;
    selDots = 0;
    dotsSlot = dKeep; oneShotSlot = oKeep; lockSlot = lKeep;
    lastMode = currentMode();     // v11: a loaded state is already in its mode - nothing to switch
    lastSpb = stepsPerBarFor((int) raw("grid"));
    if (lastMode == modeLock) lockSlot.valid = true;
    clearEnvPending.store(false); // the restore just set grid / bars: the envelopes read above belong to them
    rebuildPlan();
}

void SlicerProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    storeStateExtras();
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void SlicerProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
        {
            const juce::ScopedValueSetter<bool> quiet(applyingSlot, true); // v11: the loaded state is already in its mode
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            restoreFromState();
        }
}

// v13: preset libraries
juce::File SlicerProcessor::getLibraryRoot()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Psycho Beat Lab Presets");
}
juce::File SlicerProcessor::getOldPresetDir()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("BeatSlicer").getChildFile("Presets");
}
// where Save puts a preset: "User" in the libraries folder (once it is installed), otherwise the old place
juce::File SlicerProcessor::getUserPresetDir()
{
    return hasLibraryRoot() ? getLibraryRoot().getChildFile("User") : getOldPresetDir();
}
// the presets of "User": the User folder, plus presets of the old folder that were not copied over (nothing gets lost)
juce::Array<juce::File> SlicerProcessor::getUserPresetFiles() const
{
    auto files = getPresetFilesIn(getUserPresetDir());
    if (hasLibraryRoot())
        for (auto& f : getPresetFilesIn(getOldPresetDir()))
        {
            bool same = false;
            for (auto& g : files) same = same || g.getFileName().equalsIgnoreCase(f.getFileName());
            if (!same) files.add(f);
        }
    files.sort();
    return files;
}
// every folder of the libraries folder is a library (read again every time the list opens - new folders show up by themselves)
juce::Array<juce::File> SlicerProcessor::getLibraries() const
{
    juce::Array<juce::File> out;
    if (!hasLibraryRoot()) return out;
    auto dirs = getLibraryRoot().findChildFiles(juce::File::findDirectories, false);
    dirs.sort();
    for (auto& d : dirs)
        if (!d.getFileName().equalsIgnoreCase("User") && !d.isHidden()) out.add(d);
    return out;
}
juce::File SlicerProcessor::createLibrary(const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName(name.trim());
    if (clean.isEmpty() || !hasLibraryRoot()) return {};
    auto d = getLibraryRoot().getChildFile(clean);
    if (d.isDirectory()) return d;
    return d.createDirectory().wasOk() ? d : juce::File();
}

// v11: a preset with this name is already saved (the Save window then asks before overwriting it)
bool SlicerProcessor::userPresetExists(const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName(name.trim());
    return clean.isNotEmpty() && getUserPresetDir().getChildFile(clean + ".bspreset").existsAsFile();
}

bool SlicerProcessor::saveUserPreset(const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName(name.trim());
    if (clean.isEmpty()) return false;
    auto dir = getUserPresetDir();
    if (!dir.createDirectory()) return false;
    presetName = clean;
    storeStateExtras();
    if (auto xml = apvts.copyState().createXml()) return xml->writeTo(dir.getChildFile(clean + ".bspreset"));
    return false;
}

// ------------------------------------------------------------------ v12: Export / Import preset
// An exported preset carries its sounds (the sample and the ADD LAYER sound) inside the file, so whoever opens it
// hears exactly the same thing. Importing unpacks the sounds into Documents\BeatSlicer\Samples and adds the preset
// to "My presets".
juce::File SlicerProcessor::getSampleStoreDir()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("BeatSlicer").getChildFile("Samples");
}

namespace
{
const char* kEmbedTag = "PBL_EMBED";
void embedSound(juce::XmlElement& parent, const char* tag, const juce::String& path)
{
    const juce::File f(path);
    if (path.isEmpty() || !f.existsAsFile() || f.getSize() > (juce::int64) 64 * 1024 * 1024) return;
    juce::MemoryBlock mb;
    if (!f.loadFileAsData(mb)) return;
    auto* e = parent.createNewChildElement(tag);
    e->setAttribute("file", f.getFileName());
    e->addTextElement(mb.toBase64Encoding());
}
// the sounds inside an exported preset are written to the Samples folder and the preset points at them
void unpackSounds(juce::XmlElement& xml, const juce::File& store)
{
    auto* emb = xml.getChildByName(kEmbedTag);
    if (emb == nullptr) return;
    store.createDirectory();
    auto unpack = [&](const char* tag, const char* prop)
    {
        auto* e = emb->getChildByName(tag);
        if (e == nullptr) return;
        juce::MemoryBlock mb;
        if (!mb.fromBase64Encoding(e->getAllSubText().trim()) || mb.getSize() == 0) return;
        auto name = juce::File::createLegalFileName(e->getStringAttribute("file", "sound.wav"));
        auto f = store.getChildFile(name);
        if (f.existsAsFile())
        {
            juce::MemoryBlock old;
            if (!(f.loadFileAsData(old) && old == mb)) f = store.getNonexistentChildFile(f.getFileNameWithoutExtension(), f.getFileExtension(), false);
        }
        if (!f.existsAsFile() && !f.replaceWithData(mb.getData(), mb.getSize())) return;
        xml.setAttribute(prop, f.getFullPathName());
    };
    unpack("SAMPLE", "samplePath");
    unpack("LAYER", "layerPath");
    xml.removeChildElement(emb, true);
}
}

bool SlicerProcessor::exportPreset(const juce::File& dest)
{
    if (dest == juce::File()) return false;
    const auto out = dest.hasFileExtension(".bspreset") ? dest : dest.withFileExtension(".bspreset");
    storeStateExtras();
    auto xml = apvts.copyState().createXml();
    if (!xml) return false;
    xml->setAttribute("presetName", out.getFileNameWithoutExtension());
    auto* emb = xml->createNewChildElement(kEmbedTag);
    embedSound(*emb, "SAMPLE", samplePath);
    embedSound(*emb, "LAYER", layerPath);
    out.getParentDirectory().createDirectory();
    return xml->writeTo(out);
}

juce::File SlicerProcessor::importPreset(const juce::File& src)
{
    auto xml = juce::XmlDocument::parse(src);
    if (!xml || !xml->hasTagName(apvts.state.getType().toString())) return {};
    unpackSounds(*xml, getSampleStoreDir());
    auto dir = getUserPresetDir();
    if (!dir.createDirectory()) return {};
    auto name = juce::File::createLegalFileName(src.getFileNameWithoutExtension().trim());
    if (name.isEmpty()) name = "Imported preset";
    auto dest = dir.getChildFile(name + ".bspreset");
    if (dest.existsAsFile()) dest = dir.getNonexistentChildFile(name, ".bspreset", false); // never replaces one of yours
    xml->setAttribute("presetName", dest.getFileNameWithoutExtension());
    if (!xml->writeTo(dest)) return {};
    return dest;
}

bool SlicerProcessor::loadUserPreset(const juce::File& f)
{
    if (auto xml = juce::XmlDocument::parse(f))
        if (xml->hasTagName(apvts.state.getType()))
        {
            unpackSounds(*xml, getSampleStoreDir()); // v12: an exported file put straight into the folder still works
            const juce::ScopedValueSetter<bool> quiet(applyingSlot, true); // v11
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            restoreFromState();
            presetName = f.getFileNameWithoutExtension();
            return true;
        }
    return false;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SlicerProcessor(); }
