// Rhythm presets: 16-step patterns of velocities (0 = silent step)
#pragma once
#include <vector>
#include "dsp/SliceEngine.h"

namespace bs
{
struct Rhythm
{
    const char* name;
    int grid;        // index: 0=1/4 1=1/8 2=1/16 3=1/32 4=triplet
    float swing;
    float v[16];
};

inline const std::vector<Rhythm>& rhythms()
{
    static const std::vector<Rhythm> r = {
        { "4 on the floor", 2, 0.0f, { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 } },
        { "Straight 16", 2, 0.0f, { 1, .5f, .75f, .5f, 1, .5f, .75f, .5f, 1, .5f, .75f, .5f, 1, .5f, .75f, .5f } },
        { "8ths", 2, 0.0f, { 1, 0, .7f, 0, 1, 0, .7f, 0, 1, 0, .7f, 0, 1, 0, .7f, 0 } },
        { "Trap roll", 2, 0.0f, { 1, 0, .6f, .6f, 1, 0, .5f, 0, 1, 0, .6f, .5f, .9f, .5f, .7f, .5f } },
        { "Swing", 2, 0.6f, { 1, 0, .7f, .4f, 1, 0, .7f, .4f, 1, 0, .7f, .4f, 1, 0, .7f, .4f } },
        { "Syncopation", 2, 0.0f, { 1, 0, 0, .9f, 0, 0, .8f, 0, 1, 0, 0, .9f, 0, 0, .8f, 0 } },
        { "Hip-hop", 2, 0.25f, { 1, 0, 0, 0, 0, 0, .7f, 0, 0, .8f, 0, 0, 1, 0, 0, .5f } },
        { "House", 2, 0.0f, { 1, 0, 0, .5f, 1, 0, 0, .5f, 1, 0, 0, .5f, 1, 0, .6f, .5f } },
        { "Stutter", 2, 0.0f, { 1, .8f, .6f, .4f, 1, .8f, .6f, .4f, 1, .8f, .6f, .4f, 1, 1, 1, 1 } },
    };
    return r;
}

// ------------------------------------------------------------------ factory presets
// A factory preset is the whole picture: rhythm (velocity per step), pitch / reverse / stutter / long per step,
// and the sound knobs. It is built from exactly the same elements a user preset saves.
struct FactoryPreset
{
    const char* name;
    int grid;                 // 0=1/4 1=1/8 2=1/16 3=1/32 4=triplet
    int steps;                // how many steps of the arrays are used (16 or 32)
    float vel[32];
    float semi[32];
    unsigned char rev[32];
    unsigned char rep[32];
    unsigned char len[32];
    struct Knob { const char* id; float value; };
    std::vector<Knob> knobs;  // plain parameter values
    // v12: the Monkey Library uses much more of the plugin (balagan, wah, pan, copied chops, LFO and ENV routing)
    unsigned char bal[32] {};
    float pan[32] {};
    unsigned char wah[32] {};
    unsigned char src[32] {};  // 1-based step whose chop this step plays (0 = its own)
    struct Send { int dest; float amount; };
    std::vector<Send> lfo, env;
    int lfoShape = -1;        // index of lfoPreset() (-1 = the LFO shape is not touched)
    bool library = false;     // listed in the Monkey Library at the bottom of the preset list
};

inline std::vector<FactoryPreset> monkeyLibrary(); // v12, below

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> f = []
    {
    std::vector<FactoryPreset> f = {
        // House: the strong chops sit on the off-beats between the kicks, short and bouncy,
        // 16th pickups lead into them and the last off-beat jumps an octave up.
        { "House Chop", 2, 16,
          { .55f, 0, 1, 0,   0, .6f, 1, 0,   .55f, 0, 1, .5f,   0, .65f, 1, .7f },
          { 0, 0, 0, 0,      0, 0, 0, 0,     0, 0, 0, 0,        0, 0, 12, 0 },
          {}, {}, {},
          { { "swing", 0.08f }, { "length", 0.55f }, { "attack", 1.5f }, { "decay", 140.0f }, { "sustain", 0.35f },
            { "release", 45.0f }, { "cutoff", 20000.0f }, { "resonance", 0.0f }, { "ftype", 0.0f }, { "noise", 0.0f },
            { "pitch", 0.0f }, { "volume", 0.8f }, { "velocity", 0.85f }, { "random", 0.0f }, { "reverse", 0.0f } } },

        // Trap: 1/32 grid, half-time feel. A stutter right after the 3rd hit, one reversed slice,
        // and a 1/32 roll that climbs into the next bar. Long release and a lower pitch.
        { "Trap Stutter", 3, 32,
          { 1, 0, 0, 0,   .7f, 0, .6f, 0,   .9f, .6f, 0, 0,   .75f, 0, 0, 0,
            1, 0, 0, 0,   .7f, 0, .6f, 0,   .85f, 0, .45f, .5f,   .6f, .7f, .85f, 1 },
          { 0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,
            0, 0, 0, 0,   0, 0, 0, 0,   -5, 0, 0, 0,   0, 0, 0, 0 },
          { 0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   1, 0, 0, 0,
            0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0 },
          { 0, 0, 0, 0,   0, 0, 0, 0,   0, 1, 0, 0,   0, 0, 0, 0,
            0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 1,   1, 1, 1, 1 },
          {},
          { { "swing", 0.0f }, { "length", 0.9f }, { "attack", 0.5f }, { "decay", 400.0f }, { "sustain", 0.7f },
            { "release", 260.0f }, { "cutoff", 20000.0f }, { "resonance", 0.0f }, { "ftype", 0.0f }, { "noise", 0.0f },
            { "pitch", -2.0f }, { "volume", 0.8f }, { "velocity", 0.9f }, { "random", 0.0f }, { "reverse", 0.0f } } },

        // UK Garage 2-step: heavy swing, chops on the syncopations with a "talking" velocity curve,
        // soft envelope, a warm filter and one long slice that rings through the end of the bar.
        { "Garage Swing", 2, 16,
          { 1, 0, 0, .6f,   0, 0, .85f, .35f,   0, .7f, 0, .8f,   0, 0, .95f, 0 },
          {}, {}, {},
          { 0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 2, 0 },
          { { "swing", 0.55f }, { "length", 0.85f }, { "attack", 6.0f }, { "decay", 320.0f }, { "sustain", 0.55f },
            { "release", 140.0f }, { "cutoff", 9000.0f }, { "resonance", 0.15f }, { "ftype", 0.0f }, { "noise", 0.0f },
            { "pitch", 0.0f }, { "volume", 0.8f }, { "velocity", 1.0f }, { "random", 0.0f }, { "reverse", 0.0f } } },
    };
    for (auto& m : monkeyLibrary()) f.push_back(m);
    return f;
    }();
    return f;
}

// ------------------------------------------------------------------ v12: Monkey Library
// Ten test presets, all ending in "Monkey", listed at the bottom of the preset list. Each one sets the whole sound
// (every knob it depends on), so it always sounds the same whatever was there before.
inline std::vector<FactoryPreset> monkeyLibrary()
{
    std::vector<FactoryPreset> out;
    auto make = [&](const char* name, int grid, int steps, std::vector<float> vel, std::vector<FactoryPreset::Knob> own)
    {
        FactoryPreset m {};
        m.name = name; m.grid = grid; m.steps = steps; m.library = true;
        for (int i = 0; i < 32; ++i) m.vel[i] = i < (int) vel.size() ? vel[(size_t) i] : 0.0f;
        // a clean start: every knob a Monkey preset relies on, then its own values on top
        m.knobs = { { "swing", 0.0f }, { "length", 0.8f }, { "attack", 1.0f }, { "decay", 250.0f }, { "sustain", 0.6f }, { "release", 80.0f },
                    { "cutoff", 20000.0f }, { "resonance", 0.0f }, { "ftype", 0.0f }, { "noise", 0.0f }, { "noisetype", 0.0f },
                    { "pitch", 0.0f }, { "volume", 0.8f }, { "velocity", 1.0f }, { "random", 0.0f }, { "reverse", 0.0f },
                    { "balagan", 50.0f }, { "balmode", 0.0f }, { "balsuper", 0.0f }, { "balfilter", 0.0f },
                    { "univoices", 1.0f }, { "unidetune", 20.0f }, { "unispread", 0.5f },
                    { "chorate", 0.6f }, { "chodepth", 0.5f }, { "chomix", 0.0f }, { "wah", 0.0f }, { "wahspeed", 0.5f },
                    { "lfomode", 0.0f }, { "lforate", 4.0f }, { "lfosync", 1.0f }, { "envmode", 0.0f } };
        for (auto& k : own)
        {
            bool found = false;
            for (auto& b : m.knobs) if (std::string(b.id) == k.id) { b.value = k.value; found = true; }
            if (!found) m.knobs.push_back(k);
        }
        out.push_back(m);
        return &out.back();
    };
    auto db = [](float d) { return std::pow(10.0f, d / 20.0f); };

    {   // Yellow Monkey: every quarter is a BALAGAN roll on 1/8 triplets (three hits per quarter), all on the first chop
        // (the hit of a short sample - snare, hat). The roll's own filter closes, so the first hit of every quarter is the strongest.
        auto* m = make("Yellow Monkey", 0, 4, { 1, 1, 1, 1 },
                       { { "balmode", 1.0f }, { "balagan", 9.0f }, { "balfilter", -0.45f }, { "length", 1.0f }, { "attack", 0.5f },
                         { "decay", 160.0f }, { "sustain", 0.35f }, { "release", 40.0f } });
        for (int i = 0; i < 4; ++i) { m->bal[i] = 1; m->src[i] = 1; }
    }
    {   // Banana Monkey: bouncy house on 1/16 with a little swing, the off-beats pushed by the dots,
        // a short envelope opens the filter on every chop (ENV -> Cutoff)
        auto* m = make("Banana Monkey", 2, 16, { .5f, 0, 1, .35f, .5f, 0, 1, .35f, .5f, 0, 1, .35f, .5f, .3f, 1, .6f },
                       { { "swing", 0.12f }, { "length", 0.6f }, { "decay", 90.0f }, { "sustain", 0.15f }, { "release", 45.0f },
                         { "cutoff", 900.0f }, { "resonance", 0.3f } });
        m->env = { { mdCutoff, 0.5f } };
    }
    {   // Black Monkey: trap on 1/8, clean dots and a 1/32 BALAGAN roll (BPM) on the last two, the last one a fourth lower.
        // ENV -> Pitch gives every chop a small dive.
        auto* m = make("Black Monkey", 1, 8, { 1, 0, .75f, 0, .9f, .55f, .85f, .85f },
                       { { "balmode", 1.0f }, { "balagan", 23.0f }, { "length", 0.85f }, { "attack", 0.5f }, { "decay", 320.0f },
                         { "sustain", 0.6f }, { "release", 120.0f }, { "pitch", -2.0f }, { "velocity", 0.9f } });
        m->bal[6] = m->bal[7] = 1;
        m->semi[7] = -5.0f;
        m->env = { { mdPitch, 0.12f } };
    }
    {   // Jungle Monkey: a 1/16 break, two single dots go wild (BALAGAN FREE + SUPER), the LFO moves the Cutoff along the bar
        auto* m = make("Jungle Monkey", 2, 16, { 1, 0, .55f, 0, .9f, 0, .5f, .8f, 0, .7f, 1, 0, .9f, .5f, .75f, .85f },
                       { { "swing", 0.06f }, { "length", 0.75f }, { "decay", 220.0f }, { "sustain", 0.5f }, { "release", 60.0f },
                         { "cutoff", 5000.0f }, { "resonance", 0.2f }, { "balagan", 45.0f }, { "balsuper", 1.0f },
                         { "lfomode", 2.0f }, { "lforate", 2.0f } });
        m->bal[7] = m->bal[15] = 1;
        m->lfo = { { mdCutoff, 0.35f } };
        m->lfoShape = 0;
    }
    {   // Crazy Monkey: every dot is BALAGAN (FREE + SUPER), the LFO keeps changing the roll speed, the roll's filter sweeps
        auto* m = make("Crazy Monkey", 1, 8, { 1, .8f, 1, .7f, 1, .85f, 1, .9f },
                       { { "length", 1.0f }, { "decay", 200.0f }, { "sustain", 0.5f }, { "release", 50.0f }, { "cutoff", 14000.0f },
                         { "balagan", 40.0f }, { "balsuper", 1.0f }, { "balfilter", 0.7f }, { "lfomode", 2.0f }, { "lforate", 3.0f } });
        for (int i = 0; i < 8; ++i) m->bal[i] = 1;
        m->lfo = { { mdBalagan, 0.45f } };
        m->lfoShape = 1;
    }
    {   // Space Monkey: sparse and airy 1/8 - wah on every second chop that plays, the pan swaps sides, chorus for space
        auto* m = make("Space Monkey", 1, 8, { 1, 0, 0, .75f, 0, .85f, 0, .6f },
                       { { "length", 1.0f }, { "attack", 6.0f }, { "decay", 700.0f }, { "sustain", 0.65f }, { "release", 350.0f },
                         { "cutoff", 9000.0f }, { "resonance", 0.1f }, { "chomix", 0.45f }, { "chodepth", 0.7f }, { "chorate", 0.35f },
                         { "wah", 0.55f }, { "wahspeed", 0.35f } });
        m->wah[3] = m->wah[7] = 1;
        m->pan[0] = -0.6f; m->pan[3] = 0.6f; m->pan[5] = -0.6f; m->pan[7] = 0.6f;
    }
    {   // Ping Pong Monkey: 1/16, the pan jumps hard left - right on every dot and the level drops along the bar,
        // like a ball bouncing to a stop
        std::vector<float> v;
        for (int i = 0; i < 16; ++i) v.push_back(db(-1.5f * (float) i));
        auto* m = make("Ping Pong Monkey", 2, 16, v,
                       { { "length", 0.5f }, { "attack", 0.5f }, { "decay", 140.0f }, { "sustain", 0.3f }, { "release", 40.0f } });
        for (int i = 0; i < 16; ++i) m->pan[i] = (i & 1) ? 1.0f : -1.0f;
    }
    {   // Robot Monkey: straight 1/16 techno, ENV -> Pitch = a fast dive on every chop, ENV -> Noise = a snap on every hit
        auto* m = make("Robot Monkey", 2, 16, { 1, .45f, .7f, .45f, 1, .45f, .7f, .45f, 1, .45f, .7f, .45f, 1, .45f, .7f, .6f },
                       { { "length", 0.5f }, { "attack", 0.3f }, { "decay", 60.0f }, { "sustain", 0.15f }, { "release", 30.0f },
                         { "cutoff", 12000.0f }, { "resonance", 0.15f } });
        m->env = { { mdPitch, 0.25f }, { mdNoise, 0.5f } };
    }
    {   // Drunk Monkey: garage with heavy swing, a slow LFO on the Pitch makes the chops sway, some dots weak on purpose
        auto* m = make("Drunk Monkey", 2, 16, { 1, 0, 0, .35f, 0, 0, .8f, .25f, 0, .65f, 0, .3f, 0, 0, .9f, .2f },
                       { { "swing", 0.6f }, { "length", 0.85f }, { "attack", 5.0f }, { "decay", 300.0f }, { "sustain", 0.55f },
                         { "release", 140.0f }, { "cutoff", 8000.0f }, { "resonance", 0.1f }, { "lfomode", 2.0f }, { "lforate", 1.0f } });
        m->lfo = { { mdPitch, 0.06f } };
        m->lfoShape = 0;
    }
    {   // Fire Monkey: a build-up - the dots get denser toward the end of the bar, the last four are BALAGAN (BPM) and the
        // LFO (Rise, TRIG) speeds every roll up; ENV -> Bal. Filter opens the roll bright
        auto* m = make("Fire Monkey", 2, 16, { 1, 0, 0, 0, .7f, 0, 0, 0, .8f, 0, .6f, 0, .9f, .75f, .85f, 1 },
                       { { "length", 0.9f }, { "decay", 250.0f }, { "sustain", 0.6f }, { "release", 60.0f }, { "cutoff", 16000.0f },
                         { "balmode", 1.0f }, { "balagan", 15.0f }, { "lfomode", 0.0f }, { "lforate", 4.0f } });
        for (int i = 12; i < 16; ++i) m->bal[i] = 1;
        m->lfo = { { mdBalagan, 0.4f } };
        m->env = { { mdBalFilter, 0.6f } };
        m->lfoShape = 6;
    }
    return out;
}

// map an array of `srcSteps` steps onto `steps` steps (nearest earlier step, empty in between when stretching)
template <typename T>
inline void resampleSteps(const T* src, int srcSteps, int steps, T* out32)
{
    for (int i = 0; i < kPatternLen; ++i) out32[i] = T {};
    for (int i = 0; i < steps && i < kPatternLen; ++i)
    {
        const int a = i * srcSteps / steps, prevA = i == 0 ? -1 : (i - 1) * srcSteps / steps;
        if (srcSteps >= steps || a != prevA) out32[i] = src[std::min(srcSteps - 1, a)];
    }
}

// resample a 16-step rhythm to `steps` steps
inline void resampleRhythm(const Rhythm& r, int steps, float* out32)
{
    for (int i = 0; i < kPatternLen; ++i) out32[i] = 0.0f;
    for (int i = 0; i < steps && i < kPatternLen; ++i)
    {
        if (steps == 32) out32[i] = (i % 2 == 0) ? r.v[i / 2] : 0.0f;
        else out32[i] = r.v[std::min(15, i * 16 / steps)];
    }
}
} // namespace bs
