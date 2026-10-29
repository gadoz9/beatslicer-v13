// Beat Slicer engine: no JUCE dependency, so it can be tested offline.
// A "plan" maps every step of the loop to a piece of the sample; the Engine plays the plan locked to the tempo.
#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace bs
{
constexpr int kMaxSteps = 256;   // 8 bars x 32 steps
constexpr int kPatternLen = 32;  // one bar of the pattern (at most 32 steps)

inline int stepsPerBarFor(int gridIdx)
{
    // v10: index 5 = 1/2 (two dots per bar). It sits last so old projects keep their grid; on screen it comes first.
    static const int tbl[] = { 4, 8, 16, 32, 12, 2 }; // 1/4, 1/8, 1/16, 1/32, 1/8 triplets, 1/2
    return tbl[std::clamp(gridIdx, 0, 5)];
}
inline int barsFor(int barsIdx)
{
    static const int tbl[] = { 1, 2, 4, 8 };
    return tbl[std::clamp(barsIdx, 0, 3)];
}

struct SampleData
{
    std::vector<float> L, R;
    double sr = 44100.0;
    int size() const { return (int) L.size(); }
};

struct Params
{
    int mode = 0;       // 0 PLAY (keys), 1 AUTO (runs by itself)
    int bars = 1;       // index into 1,2,4,8
    int grid = 2;       // index into 1/4,1/8,1/16,1/32,triplet,1/2 (v10)
    int pick = 0;       // 0 auto (onsets), 1 manual (markers / even), 2 random
    int keytrack = 1;
    int seed = 1;
    float length = 0.8f, attackMs = 1.0f, decayMs = 200.0f, sustain = 0.6f, releaseMs = 60.0f;
    float cutoff = 20000.0f, noise = 0.0f;
    float pitch = 0.0f, volume = 0.8f, velocity = 1.0f, random = 0.0f; // v5: velocity = Velocity Depth (how much the dots count)
    float swing = 0.0f, reverse = 0.0f, masterDb = -3.0f, tempo = 120.0f;
    float regionStart = 0.0f, regionEnd = 1.0f;
    float resonance = 0.0f;
    int filterType = 0; // 0 low-pass, 1 high-pass, 2 band-pass
    int oneShot = 0;    // 1 = the whole sample (between the region markers) plays once per key, no slicing
    // v4: LFO
    int lfoMode = 0;        // 0 Trig (restarts with every slice), 1 Env (runs once per slice, then holds), 2 Free (runs along the loop)
    int lfoSync = 1;        // 1 = rate in note values, 0 = rate in Hz
    float lfoBeats = 1.0f;  // cycle length in quarter notes (sync)
    float lfoHz = 2.0f;     // cycle rate (no sync)
    float modAmt[21] {};    // LFO amount per destination (-1..1 of the knob's range), see ModDest
    // v4: unison + chorus
    int uniVoices = 1;
    float uniDetune = 20.0f, uniSpread = 0.5f;  // cents, 0..1
    float choRate = 0.6f, choDepth = 0.5f, choMix = 0.0f;
    // v6
    int latch = 0;          // LOOP button: one key press starts the loop, the next press stops it
    float wahAmt = 0.0f;    // WAH amount (0 = off), only on the steps marked with W
    float wahSpeed = 0.5f;  // how fast the wah opens and closes (0 slow .. 1 fast)
    // v7
    int noiseType = 0;      // see noiseTypeName()
    float layerMix = 0.5f;  // ADD LAYER: how much of the dropped sound is blended into every chop
    // v8: BALAGAN - on the red dots the chop leaves the grid and repeats fast inside its own step
    float balagan = 50.0f;  // 1..100: how fast the repeats are (about 25 = 1/32, 50 = 1/64, 100 = almost a buzz)
    int balSuper = 0;       // SUPER: every repeat starts from a random place inside the chop (off = from its start)
    float balFilter = 0.0f; // -1..1: the filter moves along the roll (+ opens from dark to bright, - closes)
    // v9
    float mainMix = 1.0f;   // MAIN MIX: level of the main sample only (the ADD LAYER sound keeps its own Mix)
    // v10
    int envSlice = 0;       // SLICE ENVELOPE lamp: 0 = SAMPLE (as v9), 1 = SLICE (the ADSR fits inside every slice)
    // v11
    int osLock = 0;         // LOCK ONE SHOT: every dot plays the whole locked one shot (no slicing)
    // v12
    int balMode = 0;        // BALAGAN: 0 = FREE (smooth, as before), 1 = BPM (the repeats land on note values)
    float envAmt[21] {};    // ENV amount per destination (-1..1 of the knob's range): the envelope of every chop moves the knob
    // v13: ADD LAYER
    float layerStartSec = 0.0f; // where in the layer sound it starts (seconds). Below 0 = the layer waits that long
    float layAtkMs = 0.1f, layDecMs = 250.0f, laySus = 1.0f, layRelMs = 1.0f; // the layer's own envelope (Release ends at its end)
    int layUnlink = 0;          // UNLINK: the layer leaves the effects (the dots, timing and the main envelope stay)
    int layLinks = 0;           // with UNLINK on: what is linked back (bits, see kLink*)
};

// v13: UNLINK of the ADD LAYER sound - what it can be linked back to (the circles in the list under the arrow)
constexpr int kLinkBalagan = 1, kLinkPitch = 2, kLinkFilter = 4, kLinkLfo = 8, kLinkChorus = 16, kLinkUnison = 32, kLinkWah = 64;
constexpr int kLinkCount = 7;
inline int layerLinkBit(int i) { return 1 << std::clamp(i, 0, kLinkCount - 1); }
inline const char* layerLinkName(int i)
{
    static const char* n[kLinkCount] = { "Balagan", "Pitch", "Filter", "LFO", "Chorus", "Unison", "Wah" };
    return n[std::clamp(i, 0, kLinkCount - 1)];
}
// is the layer linked to this part? UNLINK off = everything as in v12 (the layer never followed Pitch there),
// UNLINK on = only what was linked back in the list.
inline bool layerLinked(const Params& p, int bit)
{
    if (bit == kLinkPitch) return p.layUnlink != 0 && (p.layLinks & bit) != 0;
    return p.layUnlink == 0 || (p.layLinks & bit) != 0;
}
// v13: the first place in a sound where it really starts (the silence before it is skipped), in samples
inline int soundStart(const SampleData& s)
{
    float pk = 0.0f;
    for (int i = 0; i < s.size(); ++i) pk = std::max(pk, std::max(std::fabs(s.L[(size_t) i]), std::fabs(s.R[(size_t) i])));
    if (pk < 1.0e-5f) return 0;
    const float thr = std::max(pk * 0.03f, 1.0e-4f);
    for (int i = 0; i < s.size(); ++i)
        if (std::fabs(s.L[(size_t) i]) > thr || std::fabs(s.R[(size_t) i]) > thr)
            return std::max(0, i - (int) (s.sr * 0.0004)); // a tiny bit before, so the attack is never cut
    return 0;
}

// v10: the own envelope of one slice (one box on the waveform), set while the SLICE lamp is on
struct SegEnv
{
    bool on = false;
    float attackMs = 1.0f, decayMs = 250.0f, sustain = 0.6f, releaseMs = 80.0f;
};

// v11: the own values of one dot. Each value that is not set follows the knob.
// v11 had Attack, Decay, Sustain, Release and Length. v12 adds Cutoff, Resonance, Noise, Pitch and Volume (chosen dots:
// every knob change goes to every chosen dot) and the dot's own locators: where its chop starts and ends (0..1 of the chop).
constexpr int kDotEnvAtk = 0, kDotEnvDec = 1, kDotEnvSus = 2, kDotEnvRel = 3, kDotEnvLen = 4;
constexpr int kDotCutoff = 5, kDotRes = 6, kDotNoise = 7, kDotPitch = 8, kDotVolume = 9; // v12
constexpr int kDotKnobCount = 10;                                                        // the values that belong to a knob
constexpr int kDotStart = 10, kDotEnd = 11, kDotEnvCount = 12;                           // v12: the dot's own locators
constexpr float kDotUnset = -1.0e6f; // "not set" (Pitch can be below 0, so the mark is far outside every range)
struct DotEnv
{
    float v[kDotEnvCount] { kDotUnset, kDotUnset, kDotUnset, kDotUnset, kDotUnset, kDotUnset,
                            kDotUnset, kDotUnset, kDotUnset, kDotUnset, kDotUnset, kDotUnset };
    static bool isSet(float x) { return x > -1.0e5f; }
    bool any() const { for (float x : v) if (isSet(x)) return true; return false; }
    bool has(int k) const { return isSet(v[k]); }
    float get(int k, float knob) const { return has(k) ? v[k] : knob; }
    bool trimmed() const { return has(kDotStart) || has(kDotEnd); }
    float trimA() const { return has(kDotStart) ? std::clamp(v[kDotStart], 0.0f, 0.98f) : 0.0f; }
    float trimB() const { return has(kDotEnd) ? std::clamp(v[kDotEnd], trimA() + 0.02f, 1.0f) : 1.0f; }
};

// v12: ENV (the envelope of every chop) can be dragged onto these knobs only - where it gives a clear, useful result
inline bool envDestAllowed(int d);

// v12: BALAGAN in BPM mode - the knob picks a note value, so every repeat lands on the beat grid
struct BalDiv { int upTo; float perQuarter; const char* name; };
inline const BalDiv& balaganDiv(float knob)
{
    static const BalDiv t[] = { { 7, 2.0f, "1/8" },     { 10, 3.0f, "1/8T" },    { 17, 4.0f, "1/16" },   { 20, 6.0f, "1/16T" },
                                { 25, 8.0f, "1/32" },   { 30, 12.0f, "1/32T" },  { 50, 16.0f, "1/64" },  { 60, 24.0f, "1/64T" },
                                { 75, 32.0f, "1/128" }, { 85, 48.0f, "1/128T" }, { 100, 64.0f, "1/256" } };
    const int k = (int) std::lround(std::clamp(knob, 1.0f, 100.0f));
    for (const auto& d : t)
        if (k <= d.upTo) return d;
    return t[10];
}
// v12: BALAGAN in FREE mode - a curve instead of a straight line: more of the knob for the slow and middle speeds,
// less for the very fast end. 1 and 100 stay where they were (about 1/16 and 1/256).
inline float balaganFreeCurve(float knob)
{
    const float x = (std::clamp(knob, 1.0f, 100.0f) - 1.0f) / 99.0f;
    return 1.0f + 99.0f * std::pow(x, 1.5f);
}
inline double balaganPerQuarter(float knob, int mode)
{
    if (mode == 1) return (double) balaganDiv(knob).perQuarter;
    return std::pow(2.0, 2.0 + (double) balaganFreeCurve(knob) / 25.0);
}

// v7: built-in noise colours / textures of the Noise knob
constexpr int kNoiseTypes = 10;
inline const char* noiseTypeName(int t)
{
    static const char* n[kNoiseTypes] = { "White", "Pink", "Brown", "Wind", "Metal", "Water", "Vinyl", "Tape", "Fire", "Digital" };
    return n[std::clamp(t, 0, kNoiseTypes - 1)];
}

// ------------------------------------------------------------------ LFO destinations
// every knob the LFO can be dragged onto. Ranges mirror the parameters (lo, hi, centre for the skew; -1 = linear)
enum ModDest
{
    mdLength, mdAttack, mdDecay, mdSustain, mdRelease, mdCutoff, mdResonance, mdNoise, mdPitch, mdVolume, mdVelocity,
    mdSwing, mdMaster, mdUniDetune, mdUniSpread, mdChoRate, mdChoDepth, mdChoMix, mdLayerMix,
    mdBalagan, mdBalFilter, // v8
    kModCount
};
static_assert(kModCount == 21, "Params::modAmt size must match ModDest");
inline bool envDestAllowed(int d) { return d == mdCutoff || d == mdPitch || d == mdNoise || d == mdBalagan || d == mdBalFilter; }
struct ModInfo { const char* id; const char* label; float lo, hi, centre; };
inline const ModInfo& modInfo(int d)
{
    static const ModInfo t[kModCount] = {
        { "length", "Length", 0.05f, 1.0f, -1.0f },     { "attack", "Attack", 0.1f, 500.0f, 15.0f },
        { "decay", "Decay", 5.0f, 3000.0f, 300.0f },     { "sustain", "Sustain", 0.0f, 1.0f, -1.0f },
        { "release", "Release", 5.0f, 3000.0f, 200.0f }, { "cutoff", "Cutoff", 40.0f, 20000.0f, 1500.0f },
        { "resonance", "Resonance", 0.0f, 1.0f, -1.0f }, { "noise", "Noise", 0.0f, 1.0f, -1.0f },
        { "pitch", "Pitch", -12.0f, 12.0f, 0.0f },       { "volume", "Volume", 0.0f, 1.0f, -1.0f },
        { "velocity", "Vel Depth", 0.0f, 1.0f, -1.0f },   { "swing", "Swing", 0.0f, 1.0f, -1.0f },
        { "master", "Master", -24.0f, 12.0f, 0.0f },      { "unidetune", "Uni Detune", 0.0f, 100.0f, -1.0f },
        { "unispread", "Uni Spread", 0.0f, 1.0f, -1.0f }, { "chorate", "Ch. Rate", 0.05f, 5.0f, 0.8f },
        { "chodepth", "Ch. Depth", 0.0f, 1.0f, -1.0f },  { "chomix", "Ch. Mix", 0.0f, 1.0f, -1.0f },
        { "layermix", "Layer Mix", 0.0f, 1.0f, -1.0f },
        { "balagan", "Balagan", 1.0f, 100.0f, -1.0f },   { "balfilter", "Bal. Filter", -1.0f, 1.0f, -1.0f },
    };
    return t[std::clamp(d, 0, kModCount - 1)];
}
inline int modIndexOf(const std::string& id)
{
    for (int d = 0; d < kModCount; ++d)
        if (id == modInfo(d).id) return d;
    return -1;
}
inline float modSkew(int d)
{
    const auto& m = modInfo(d);
    if (m.centre > m.lo && m.centre < m.hi) return std::log(0.5f) / std::log((m.centre - m.lo) / (m.hi - m.lo));
    return 1.0f;
}
inline float modToNorm(int d, float v)
{
    const auto& m = modInfo(d);
    const float p = std::clamp((v - m.lo) / (m.hi - m.lo), 0.0f, 1.0f);
    const float s = modSkew(d);
    return (s == 1.0f || p <= 0.0f) ? p : std::exp(std::log(p) * s);
}
inline float modFromNorm(int d, float n)
{
    const auto& m = modInfo(d);
    n = std::clamp(n, 0.0f, 1.0f);
    const float s = modSkew(d);
    const float p = (s == 1.0f || n <= 0.0f) ? n : std::exp(std::log(n) / s);
    return m.lo + (m.hi - m.lo) * p;
}

// ------------------------------------------------------------------ LFO shape
// points with a curve per segment, like Serum: x 0..1 (time), y 0..1 (value), curve -1..1 bends the segment
struct LfoPoint { float x, y, curve; };
constexpr int kLfoTable = 1024;
struct LfoTable { float v[kLfoTable + 1] {}; };

inline float lfoCurve(float t, float c)
{
    const float k = c * 6.0f;
    if (std::fabs(k) < 1.0e-3f) return t;
    return (1.0f - std::exp(-k * t)) / (1.0f - std::exp(-k));
}
inline std::shared_ptr<LfoTable> buildLfoTable(std::vector<LfoPoint> pts)
{
    auto tb = std::make_shared<LfoTable>();
    if (pts.size() < 2) pts = { { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } };
    std::stable_sort(pts.begin(), pts.end(), [](const LfoPoint& a, const LfoPoint& b) { return a.x < b.x; });
    size_t seg = 0;
    for (int i = 0; i <= kLfoTable; ++i)
    {
        const float x = (float) i / (float) kLfoTable;
        while (seg + 2 < pts.size() && x >= pts[seg + 1].x) ++seg;
        const auto& a = pts[seg];
        const auto& b = pts[seg + 1];
        const float w = b.x - a.x;
        const float t = w <= 1.0e-6f ? 1.0f : std::clamp((x - a.x) / w, 0.0f, 1.0f);
        tb->v[i] = std::clamp(a.y + (b.y - a.y) * lfoCurve(t, a.curve), 0.0f, 1.0f);
    }
    return tb;
}
constexpr int kLfoPresets = 7;
inline std::vector<LfoPoint> lfoPreset(int which)
{
    switch (which)
    {
        case 1: return { { 0, 0, 0 }, { 0.5f, 1, 0 }, { 1, 0, 0 } };                                   // triangle
        case 2: return { { 0, 0, 0 }, { 1, 1, 0 } };                                                    // saw up
        case 3: return { { 0, 1, 0 }, { 1, 0, 0 } };                                                    // saw down
        case 4: return { { 0, 1, 0 }, { 0.5f, 1, 0 }, { 0.5f, 0, 0 }, { 1, 0, 0 } };                     // square
        case 5: return { { 0, 1, 0.6f }, { 1, 0, 0 } };                                                 // decay (envelope)
        case 6: return { { 0, 0, 0.6f }, { 1, 1, 0 } };                                                 // rise
        default: return { { 0, 0, -0.45f }, { 0.25f, 0.5f, 0.45f }, { 0.5f, 1, -0.45f }, { 0.75f, 0.5f, 0.45f }, { 1, 0, 0 } }; // sine
    }
}
inline const char* lfoPresetName(int which)
{
    static const char* n[] = { "Sine", "Triangle", "Saw up", "Saw down", "Square", "Decay", "Rise" };
    return n[std::clamp(which, 0, kLfoPresets - 1)];
}

// sync rates of the LFO (length of one cycle in quarter notes)
inline float lfoBeatsFor(int idx)
{
    static const float b[] = { 16.0f, 8.0f, 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f, 2.0f / 3.0f, 1.0f / 3.0f, 1.0f / 6.0f, 1.5f, 0.75f };
    return b[std::clamp(idx, 0, 12)];
}

// per-step extras of the pattern (same length as the pattern). Presets use them, the beat bar edits them.
struct StepExtras
{
    float semi[kPatternLen] {};          // pitch offset of the step in semitones
    unsigned char rev[kPatternLen] {};   // 1 = this step plays its slice reversed
    unsigned char rep[kPatternLen] {};   // 1 = repeat the slice of the previous step (stutter / roll)
    unsigned char len[kPatternLen] {};   // gate length in steps (0 or 1 = normal, 2, 4 = long slice)
    unsigned char wah[kPatternLen] {};   // v6: 1 = this step gets the wah (amount from the WAH knob)
    unsigned char src[kPatternLen] {};   // v7: 1-based step of the same bar whose chop this step plays (0 = its own chop)
    unsigned char bal[kPatternLen] {};   // v8: 1 = BALAGAN on this step (red dot)
    float pan[kPatternLen] {};           // v8: panning of the step, -1 = left .. 0 = centre .. 1 = right
    DotEnv env[kPatternLen];             // v11: own envelope of the dot (chosen dots + the envelope knobs)
};

struct Slice
{
    int start = 0, end = 0;   // source sample range
    bool reverse = false;
    float semi = 0.0f;        // extra pitch offset of this slice
    float vel = 1.0f;         // pattern velocity, 0 = muted
    int seg = 0;              // index of the source segment (the slice rectangle on the waveform)
    float len = 1.0f;         // gate length in steps
    bool wah = false;         // v6: the wah opens on this slice
    bool bal = false;         // v8: BALAGAN roll on this slice
    float pan = 0.0f;         // v8: -1 left .. 1 right
    SegEnv env;               // v10: own envelope of the slice's box (env.on = false -> the knobs of SLICE ENVELOPE)
    DotEnv dot;               // v11: own envelope of the dot this step belongs to (wins over the box and the knobs)
    bool whole = false;       // v11: LOCK ONE SHOT - this step plays the whole locked one shot
    bool trim = false;        // v12: the dot has its own locators - start / end are already cut to them, the envelope fits inside
};

struct Plan
{
    std::shared_ptr<const SampleData> sample;
    std::vector<Slice> slices; // one per step of the whole loop
    std::vector<std::pair<int, int>> segments; // the source pieces before slip (what the waveform shows)
    int regionStart = 0, regionEnd = 0;         // the region between the markers (used by One Shot)
    int stepsPerBar = 16, bars = 1;
    bool locked = false;                        // v11: LOCK ONE SHOT - every step plays the whole region
    int totalSteps() const { return (int) slices.size(); }
};

// ------------------------------------------------------------------ onset detection
struct Onset { int pos; float strength; };

inline std::vector<Onset> detectOnsets(const SampleData& s, int r0, int r1)
{
    constexpr int hop = 256, win = 512;
    std::vector<Onset> out;
    if (r1 - r0 < win * 2) return out;
    const int n = (r1 - r0 - win) / hop;
    std::vector<float> lg((size_t) n), flux((size_t) n, 0.0f);
    for (int k = 0; k < n; ++k)
    {
        double e = 0.0;
        for (int i = 0; i < win; ++i)
        {
            const float v = 0.5f * (s.L[(size_t) (r0 + k * hop + i)] + s.R[(size_t) (r0 + k * hop + i)]);
            e += (double) v * v;
        }
        lg[(size_t) k] = std::log((float) std::sqrt(e / win) + 1.0e-4f);
    }
    double mean = 0.0;
    for (int k = 1; k < n; ++k) { flux[(size_t) k] = std::max(0.0f, lg[(size_t) k] - lg[(size_t) k - 1]); mean += flux[(size_t) k]; }
    mean /= std::max(1, n - 1);
    double var = 0.0;
    for (int k = 1; k < n; ++k) var += (flux[(size_t) k] - mean) * (flux[(size_t) k] - mean);
    const double sd = std::sqrt(var / std::max(1, n - 1));
    const float thr = (float) (mean + 0.8 * sd) + 0.05f;
    int last = -100000;
    for (int k = 2; k < n - 2; ++k)
    {
        const float f = flux[(size_t) k];
        if (f < thr) continue;
        bool peak = true;
        for (int j = -4; j <= 4 && peak; ++j)
            if (k + j >= 1 && k + j < n && flux[(size_t) (k + j)] > f) peak = false;
        if (!peak) continue;
        // refine to the first sample that really rises inside the window, with a tiny pre-roll
        const int w0 = r0 + k * hop, w1 = std::min(r1 - 1, w0 + win + hop);
        float mx = 0.0f;
        for (int i = w0; i <= w1; ++i) mx = std::max(mx, std::fabs(0.5f * (s.L[(size_t) i] + s.R[(size_t) i])));
        int hit = w0;
        for (int i = w0; i <= w1; ++i)
            if (std::fabs(0.5f * (s.L[(size_t) i] + s.R[(size_t) i])) > 0.12f * mx) { hit = i; break; }
        const int pos = std::max(r0, hit - 48);
        if (pos - last < 1500) continue;
        out.push_back({ pos, f });
        last = pos;
    }
    return out;
}

// builds the list of source segments (exactly `total` of them) for the region
inline std::vector<std::pair<int, int>> buildSegments(const SampleData& s, const Params& p, int r0, int r1, int total,
                                                      const std::vector<float>& manualCuts)
{
    std::vector<Onset> cuts;
    if (p.pick == 1 && !manualCuts.empty())
    {
        for (float c : manualCuts)
        {
            const int pos = (int) (c * (float) s.size());
            if (pos > r0 + 64 && pos < r1 - 64) cuts.push_back({ pos, 1.0f });
        }
    }
    else if (p.pick != 1)
        cuts = detectOnsets(s, r0, r1);

    std::sort(cuts.begin(), cuts.end(), [](const Onset& a, const Onset& b) { return a.strength > b.strength; });
    if ((int) cuts.size() > total - 1) cuts.resize((size_t) std::max(0, total - 1));
    std::vector<int> pts { r0 };
    for (auto& c : cuts) pts.push_back(c.pos);
    std::sort(pts.begin(), pts.end());
    pts.erase(std::unique(pts.begin(), pts.end()), pts.end());
    pts.push_back(r1);
    // fewer pieces than steps: split the longest piece until it matches
    while ((int) pts.size() - 1 < total)
    {
        size_t bi = 0; int bl = -1;
        for (size_t i = 0; i + 1 < pts.size(); ++i)
            if (pts[i + 1] - pts[i] > bl) { bl = pts[i + 1] - pts[i]; bi = i; }
        if (bl < 512) break;
        pts.insert(pts.begin() + (long) bi + 1, pts[bi] + bl / 2);
    }
    std::vector<std::pair<int, int>> seg;
    for (size_t i = 0; i + 1 < pts.size(); ++i) seg.push_back({ pts[i], pts[i + 1] });
    if (seg.empty()) seg.push_back({ r0, r1 });
    return seg;
}

inline std::shared_ptr<Plan> buildPlan(std::shared_ptr<const SampleData> sample, const Params& p, const float* pattern,
                                       const std::vector<float>& manualCuts, const StepExtras* ex = nullptr,
                                       const std::vector<float>* slips = nullptr, const std::vector<SegEnv>* segEnv = nullptr)
{
    auto plan = std::make_shared<Plan>();
    plan->sample = sample;
    plan->stepsPerBar = stepsPerBarFor(p.grid);
    plan->bars = barsFor(p.bars);
    const int total = std::min(kMaxSteps, plan->stepsPerBar * plan->bars);
    plan->slices.assign((size_t) total, Slice());
    if (!sample || sample->size() < 1024)
        return plan;
    const int N = sample->size();
    int r0 = (int) (std::clamp(p.regionStart, 0.0f, 1.0f) * (float) N), r1 = (int) (std::clamp(p.regionEnd, 0.0f, 1.0f) * (float) N);
    if (r1 - r0 < 1024) { r0 = std::max(0, std::min(r0, N - 1024)); r1 = std::min(N, r0 + 1024); }
    plan->regionStart = r0; plan->regionEnd = r1;
    // v11: LOCK ONE SHOT - no slicing: one piece (the locked one shot) that every dot plays whole.
    // Slip and the boxes' own envelopes belong to the sliced picture, so they are left out here.
    plan->locked = p.osLock != 0;
    if (plan->locked) { slips = nullptr; segEnv = nullptr; }
    const auto seg = plan->locked ? std::vector<std::pair<int, int>> { { r0, r1 } } : buildSegments(*sample, p, r0, r1, total, manualCuts);
    plan->segments = seg;
    std::mt19937 rng((uint32_t) p.seed * 2654435761u + 12345u);
    std::uniform_real_distribution<float> uni(0.0f, 1.0f);
    const float chaos = p.pick == 2 ? 1.0f : p.random;
    for (int i = 0; i < total; ++i)
    {
        Slice sl;
        size_t si = (size_t) i % seg.size();
        if (chaos > 0.0f && uni(rng) < chaos) si = (size_t) (rng() % seg.size());
        sl.start = seg[si].first; sl.end = seg[si].second;
        if (p.random > 0.0f && uni(rng) < p.random * 0.5f) sl.semi = std::round((uni(rng) * 2.0f - 1.0f) * 7.0f * p.random);
        sl.reverse = uni(rng) < p.reverse;
        sl.vel = pattern[i % plan->stepsPerBar];
        sl.seg = (int) si;
        if (ex != nullptr)
        {
            const int k = i % plan->stepsPerBar;
            if (ex->rep[k] && i > 0)
            {
                // stutter: same piece of audio (and direction) as the previous step
                const Slice& prev = plan->slices[(size_t) i - 1];
                sl.seg = prev.seg; sl.reverse = prev.reverse;
                si = (size_t) prev.seg;
            }
            sl.semi += ex->semi[k];
            if (ex->rev[k]) sl.reverse = true;
            if (ex->len[k] > 1) sl.len = (float) ex->len[k];
            sl.wah = ex->wah[k] != 0;
            sl.bal = ex->bal[k] != 0;
            sl.pan = std::clamp(ex->pan[k], -1.0f, 1.0f);
            sl.dot = ex->env[k]; // v11
        }
        sl.whole = plan->locked; // v11
        // slip: the slice keeps its place in the rhythm, only the audio inside it moves
        int a = seg[si].first, b = seg[si].second;
        if (sl.len > 1.0f) b = std::max(b, r1); // a long slice keeps reading audio until its gate closes
        if (slips != nullptr && si < slips->size())
        {
            int off = (int) ((*slips)[si] * (float) N);
            off = std::clamp(off, -a, (N - 1) - b);
            a += off; b += off;
        }
        sl.start = a; sl.end = b;
        plan->slices[(size_t) i] = sl;
    }
    if (ex != nullptr)
    {
        // v7: "Copy to next / all dots" - a step can play the chop of another step of the same bar
        auto range = [&](int si, float len, int& a, int& b)
        {
            a = seg[(size_t) si].first; b = seg[(size_t) si].second;
            if (len > 1.0f) b = std::max(b, r1);
            if (slips != nullptr && (size_t) si < slips->size())
            {
                int off = (int) ((*slips)[(size_t) si] * (float) N);
                off = std::clamp(off, -a, (N - 1) - b);
                a += off; b += off;
            }
        };
        bool anySrc = false;
        for (int k = 0; k < plan->stepsPerBar; ++k) anySrc = anySrc || ex->src[k] > 0;
        if (anySrc)
        {
            std::vector<int> own((size_t) total);
            for (int i = 0; i < total; ++i) own[(size_t) i] = plan->slices[(size_t) i].seg;
            for (int i = 0; i < total; ++i)
            {
                const int k = i % plan->stepsPerBar;
                if (ex->src[k] == 0 || (int) ex->src[k] > plan->stepsPerBar) continue;
                const int j = i - k + (int) ex->src[k] - 1;
                if (j < 0 || j >= total || j == i) continue;
                auto& sl = plan->slices[(size_t) i];
                sl.seg = own[(size_t) j];
                range(sl.seg, sl.len, sl.start, sl.end);
            }
            // a stutter after a copied step repeats the copied chop
            for (int i = 1; i < total; ++i)
            {
                if (!ex->rep[i % plan->stepsPerBar]) continue;
                auto& sl = plan->slices[(size_t) i];
                const auto& prev = plan->slices[(size_t) i - 1];
                sl.seg = prev.seg; sl.reverse = prev.reverse;
                range(sl.seg, sl.len, sl.start, sl.end);
            }
        }
    }
    // v10: every slice takes the own envelope of the box it plays (so a copied chop carries it too)
    if (segEnv != nullptr)
        for (auto& sl : plan->slices)
            if (sl.seg >= 0 && sl.seg < (int) segEnv->size()) sl.env = (*segEnv)[(size_t) sl.seg];
    // v12: a dot with its own locators plays only that part of its chop (0..1 of the box it plays, slip included;
    // in LOCK ONE SHOT the box is the whole locked one shot). The envelope then lives between them.
    for (auto& sl : plan->slices)
    {
        if (!sl.dot.trimmed() || sl.seg < 0 || sl.seg >= (int) seg.size()) continue;
        int a = seg[(size_t) sl.seg].first, b = seg[(size_t) sl.seg].second;
        if (slips != nullptr && (size_t) sl.seg < slips->size())
        {
            int off = (int) ((*slips)[(size_t) sl.seg] * (float) N);
            off = std::clamp(off, -a, (N - 1) - b);
            a += off; b += off;
        }
        const double len = (double) (b - a);
        const int a2 = a + (int) std::lround(len * sl.dot.trimA());
        const int b2 = sl.dot.has(kDotEnd) ? a + (int) std::lround(len * sl.dot.trimB()) : sl.end; // no end set: a long slice keeps reading
        if (b2 - a2 < 32) continue;
        sl.start = a2; sl.end = b2; sl.trim = true;
    }
    return plan;
}

// v5: level of a slice from its dot (linear 0..1, top = 1 = 0 dB) and the Velocity Depth (0..1)
inline float sliceLevel(float dot, float depth)
{
    if (dot <= 0.0f) return 0.0f;
    return std::pow(std::min(1.0f, dot), std::clamp(depth, 0.0f, 1.0f));
}
inline float dotToDb(float dot) { return dot <= 0.0f ? -100.0f : 20.0f * std::log10(std::min(1.0f, dot)); }
inline float dbToDot(float db) { return std::pow(10.0f, std::min(0.0f, db) / 20.0f); }
constexpr float kDotMinDb = -48.0f; // lowest level a dot can be dragged to (a click still mutes it)

// ------------------------------------------------------------------ v7: noise textures
// one shared generator: every type is built in code (filters, random events), no recordings
struct NoiseGen
{
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        auto coef = [&](float f, float k, float* c)
        {
            const float g = std::tan(3.14159265f * std::min(f, 0.45f * (float) sr) / (float) sr);
            c[0] = 1.0f / (1.0f + g * (g + k)); c[1] = g * c[0]; c[2] = g * c[1];
        };
        const float metalF[4] = { 1870.0f, 2950.0f, 4310.0f, 6280.0f };
        for (int i = 0; i < 4; ++i) coef(metalF[i], 0.03f, mc[i]);
        hpA = std::exp(-2.0f * 3.14159265f * 2500.0f / (float) sr);
        lpA = std::exp(-2.0f * 3.14159265f * 400.0f / (float) sr);
    }
    // -1..1, same generator as the v6 white noise
    float rnd() { st = st * 1664525u + 1013904223u; return (float) (st >> 8) / 8388608.0f - 1.0f; }
    float uni() { return 0.5f * (rnd() + 1.0f); }
    void next(int type, float& l, float& r)
    {
        float m = 0.0f;
        switch (type)
        {
            default: m = rnd(); l = m; r = m * 0.9f; return;     // White (exactly as before)
            case 1: m = pink() * 0.9f; break;
            case 2: m = brown() * 3.0f; break;
            case 3: m = wind(); break;
            case 4: m = metal(); break;
            case 5: m = water(); break;
            case 6: m = vinyl(); break;
            case 7: m = tape(); break;
            case 8: m = fire(); break;
            case 9: m = digital(); break;
        }
        l = r = m;
    }

private:
    float svf(float x, const float* c, float* s, float k) // band-pass, unity at the centre
    {
        const float v3 = x - s[1];
        const float v1 = c[0] * s[0] + c[1] * v3;
        const float v2 = s[1] + c[1] * s[0] + c[2] * v3;
        s[0] = 2.0f * v1 - s[0]; s[1] = 2.0f * v2 - s[1];
        return k * v1;
    }
    float pink()
    {
        const float w = rnd();
        b[0] = 0.99886f * b[0] + w * 0.0555179f; b[1] = 0.99332f * b[1] + w * 0.0750759f; b[2] = 0.96900f * b[2] + w * 0.1538520f;
        b[3] = 0.86650f * b[3] + w * 0.3104856f; b[4] = 0.55000f * b[4] + w * 0.5329522f; b[5] = -0.7616f * b[5] - w * 0.0168980f;
        const float o = (b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + w * 0.5362f) * 0.11f;
        b[6] = w * 0.115926f;
        return o * 3.0f;
    }
    float brown() { br = (br + 0.02f * rnd()) / 1.02f; return br * 3.2f; }
    float wind()
    {
        // band-passed pink noise whose centre and level drift like gusts
        if (--gustCount <= 0) { gustTarget = uni(); gustCount = (int) (sr * (0.25 + 0.6 * uni())); }
        gust += (gustTarget - gust) * (float) (6.0 / sr);
        float c[3];
        const float f = 220.0f + 1100.0f * gust;
        const float g = std::tan(3.14159265f * f / (float) sr);
        c[0] = 1.0f / (1.0f + g * (g + 0.35f)); c[1] = g * c[0]; c[2] = g * c[1];
        return svf(pink(), c, ws, 0.35f) * (0.35f + 0.9f * gust) * 3.8f;
    }
    float metal()
    {
        // ringing inharmonic resonators, like a struck sheet of metal
        const float x = rnd() * 0.5f + (uni() < 12.0f / (float) sr ? rnd() * 6.0f : 0.0f);
        float o = 0.0f;
        for (int i = 0; i < 4; ++i) o += svf(x, mc[i], ms[i], 0.03f) * (1.0f - 0.15f * (float) i);
        return o * 13.0f;
    }
    float water()
    {
        // bubbles: short sine chirps that rise, on top of a soft brown bed
        if (uni() < 22.0f / (float) sr)
            for (auto& bb : bub)
                if (bb.amp < 0.01f) { bb.f = 300.0f + 1200.0f * uni(); bb.amp = 0.3f + 0.7f * uni(); bb.dec = std::exp(-1.0f / ((0.012f + 0.03f * uni()) * (float) sr)); bb.ph = 0.0f; break; }
        float o = 0.0f;
        for (auto& bb : bub)
        {
            if (bb.amp < 0.01f) continue;
            bb.ph += 6.2831853f * bb.f / (float) sr;
            bb.f *= 1.0f + 2.5f / (float) sr;
            o += std::sin(bb.ph) * bb.amp;
            bb.amp *= bb.dec;
        }
        return o * 1.5f + brown() * 0.4f;
    }
    float crackle(float rate, float tauMs, float level)
    {
        if (uni() < rate / (float) sr) crk = (uni() < 0.5f ? -1.0f : 1.0f) * level * std::pow(uni(), 2.0f);
        const float o = crk;
        crk *= std::exp(-1.0f / (tauMs * 0.001f * (float) sr));
        // high-pass so the pops click instead of thump
        const float hp = o - crkPrev + 0.995f * crkHp;
        crkPrev = o; crkHp = hp;
        return hp;
    }
    float vinyl() { return crackle(9.0f, 0.5f, 2.6f) + pink() * 0.35f + brown() * 0.5f; }
    float tape()
    {
        const float w = rnd();
        const float hp = w - hpLast; hpLast = hpA * hpLast + (1.0f - hpA) * w; // white above ~2.5 kHz
        flt += (float) (6.2831853 * 0.6 / sr);
        if (flt > 6.2831853f) flt -= 6.2831853f;
        return (hp * 0.75f + pink() * 0.15f) * (1.0f + 0.15f * std::sin(flt));
    }
    float fire()
    {
        lp = lpA * lp + (1.0f - lpA) * brown(); // low roar
        return lp * 2.6f + crackle(45.0f, 1.2f, 2.8f);
    }
    float digital()
    {
        // sample-and-hold noise with coarse steps (bit-crushed)
        if (--holdCount <= 0) { hold = std::round(rnd() * 4.0f) / 4.0f; holdCount = 2 + (int) (uni() * 30.0f); }
        return hold * 0.75f;
    }

    double sr = 44100.0;
    uint32_t st = 22222u;
    float b[7] {};
    float br = 0.0f;
    float ws[2] {}, gust = 0.3f, gustTarget = 0.5f;
    int gustCount = 0;
    float mc[4][3] {}, ms[4][2] {};
    struct Bubble { float f = 0, amp = 0, dec = 0, ph = 0; } bub[4];
    float crk = 0.0f, crkPrev = 0.0f, crkHp = 0.0f;
    float hpA = 0.7f, hpLast = 0.0f, flt = 0.0f;
    float lpA = 0.9f, lp = 0.0f;
    float hold = 0.0f;
    int holdCount = 0;
};

// ------------------------------------------------------------------ playback
struct SliceVoice
{
    bool active = false;
    std::shared_ptr<const SampleData> sample;
    double pos = 0.0, rate = 1.0;
    int segStart = 0, segEnd = 0;
    bool reverse = false;
    double t = 0.0;            // seconds-in-samples since trigger
    double gateSamples = 0.0;
    float gain = 1.0f;
    float level = 0.0f;        // current envelope level (for the release start)
    float relStartLevel = 0.0f;
    double relT = -1.0;
    float s1[2] = { 0, 0 }, s2[2] = { 0, 0 };
    uint64_t age = 0;
    // v4: everything below is re-evaluated at control rate, so the LFO can move it while the slice plays
    double baseRate = 1.0;     // rate without the Pitch knob (key + slice pitch + sample rate)
    double gateBase = 0.0;     // gate length in samples at Length = 100 %
    float sliceVel = 1.0f, midiVel = 1.0f;
    bool oneShot = false;
    double lfoT = 0.0;         // samples since trigger (LFO Trig / Env)
    int nUni = 1;
    double upos[8] {};
    bool udone[8] {};
    double cRate[8] {};
    float panL[8] {}, panR[8] {};
    double cAtk = 1.0, cDec = 1.0, cRel = 1.0;
    float cSus = 1.0f, cNoise = 0.0f, fa1 = 0.0f, fa2 = 0.0f, fa3 = 0.0f, fk = 1.4f;
    bool useFilter = false;
    int ctl = 0;
    // v6: per-slice wah (a resonant band-pass that sweeps up when the slice starts, then falls back)
    bool wahOn = false;
    float ws1[2] = { 0, 0 }, ws2[2] = { 0, 0 }, wa1 = 0.0f, wa2 = 0.0f, wa3 = 0.0f, wk = 0.3f, wMix = 0.0f;
    // v7: ADD LAYER - the dropped sound starts from its beginning with every chop
    std::shared_ptr<const SampleData> layer;
    double layerPos = 0.0;
    float cLayer = 0.0f;
    // v8
    float stepPan = 0.0f;      // panning of the dot this slice came from
    bool roll = false;         // part of a BALAGAN roll
    double rollAt = 0.0, rollLen = 1.0; // where in the roll this hit started, and the roll length (samples)
    double atkCap = 0.0, relCap = 0.0;  // the short hits of a roll keep their attack / release inside the hit
    // v9: BALAGAN filter = its own sweeping low-pass on the roll hits (the Cutoff knob is not touched)
    bool balF = false;
    float bs1[2] = { 0, 0 }, bs2[2] = { 0, 0 }, ba1 = 0.0f, ba2 = 0.0f, ba3 = 0.0f;
    float cMain = 1.0f;        // MAIN MIX
    // v10: SLICE envelope - the ADSR fits inside the slice (release ends where the slice ends), own values per slice
    bool envFit = false;
    SegEnv ownEnv;
    double fadeLen = 88.0;     // the last ~2 ms of every slice fade out, so a cut never clicks
    DotEnv dot;                // v11: own envelope of the dot (each value below 0 = not set)
    // v12
    float envNow = 0.0f;       // where the envelope of this chop is right now (0..1) - the ENV source
    double fadeMin = 0.0;      // BALAGAN hits: the smallest fade in / out of every repeat (no clicks at high speed)
    // v13
    bool fullLen = false;      // One Shot / LOCK ONE SHOT: Length does not close the sound, it plays to the right locator
    bool layerOnly = false;    // only the layer (UNLINK from Balagan: the layer plays the step once, not every repeat)
    bool noLayer = false;      // a BALAGAN repeat without the layer
    double layerStep = 1.0, layerOrigin = 0.0, sampleRatio = 1.0;
    double lAtk = 4.0, lDec = 1.0e4, lRel = 44.0;
    float lSus = 1.0f, layerGain = 1.0f;
    bool lUseFilter = false;
    float lfa1 = 0.0f, lfa2 = 0.0f, lfa3 = 0.0f, lfk = 1.4f;                  // the layer's filter without the LFO
    float lf1[2] = { 0, 0 }, lf2[2] = { 0, 0 }, lb1[2] = { 0, 0 }, lb2[2] = { 0, 0 }, lw1[2] = { 0, 0 }, lw2[2] = { 0, 0 };
};

class Engine
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        for (auto& v : voices) v.active = false;
        choLen = (int) (sr * 0.06) + 4;
        choL.assign((size_t) choLen, 0.0f); choR.assign((size_t) choLen, 0.0f);
        choW = 0; choPh = 0.0;
        noiseGen.prepare(sampleRate);
    }
    void setParams(const Params& p) { prm = p; }
    void setPlan(std::shared_ptr<const Plan> p) { plan = std::move(p); }
    void setLfo(std::shared_ptr<const LfoTable> t) { lfo = std::move(t); }
    void setLayer(std::shared_ptr<const SampleData> l) { layer = std::move(l); } // v7: ADD LAYER sound (nullptr = none)
    // v5: Standalone only - the plugin runs on its own clock (Tempo / BPM knob) and ignores any host transport
    void setInternalClock(bool on) { internalClock = on; }
    bool usesInternalClock() const { return internalClock; }
    void setHost(bool hasTransport, bool playing, double ppq, double bpm)
    {
        if (internalClock) { hasTransport = false; playing = false; ppq = 0.0; bpm = 0.0; }
        hostHas = hasTransport; hostBpm = bpm > 1.0 ? bpm : 0.0;
        if (playing && !hostPlaying && prm.mode == 1) lastStep = -1;
        hostPlaying = playing; hostPpq = ppq;
        needSync = hasTransport && playing;
    }
    void noteOn(int note, float vel)
    {
        // v13: LOOP in One Shot - one press starts the one shot and it plays again and again (left to right locator)
        // after the key is released; the next press stops it, the press after that starts it again
        if (prm.latch && prm.mode == 0 && prm.oneShot)
        {
            if (latched) { latched = false; return; }
            latched = true; latchOS = true; latchNote = note; latchVel = vel;
            osLoopT = 0.0;
            fireOneShot(note, vel);
            return;
        }
        // v6: LOOP on - the first press starts the loop and it keeps going after the key is released,
        // the next press stops it, the press after that starts it again from the beginning
        if (prm.latch && prm.mode == 0 && !prm.oneShot)
        {
            if (latched) { latched = false; return; }
            latched = true; latchOS = false; latchNote = note; latchVel = vel;
            if (held.empty()) restartLoop(); // v9: every start of the loop begins on the first dot (Standalone and Cubase)
            return;
        }
        noteOff(note);
        // v9: PLAY - the loop starts from the first dot whenever a key starts it, LOOP on or off, Standalone or Cubase
        // (in Cubase the timing comes from where the MIDI note is, not from the bar position)
        if (held.empty() && !latched && prm.mode == 0 && !prm.oneShot) restartLoop();
        held.push_back({ note, vel });
        if (prm.oneShot) fireOneShot(note, vel);
    }
    void restartLoop()
    {
        stepPos = 0.0; lastStep = -1; freeInit = true; roll.on = false;
        anchorStart = -(double) blockSample * spsNow; // v13: the playhead starts from here (sample exact)
    }
    void noteOff(int note)
    {
        held.erase(std::remove_if(held.begin(), held.end(), [note](const Held& h) { return h.note == note; }), held.end());
    }
    void allNotesOff() { held.clear(); latched = false; roll.on = false; for (auto& v : voices) v.active = false; }
    void killVoices() { roll.on = false; for (auto& v : voices) v.active = false; } // v6: CLEAR (the sample is gone)
    bool isLatched() const { return latched; }
    // v6: is something playing right now? (the playheads only run while this is true)
    bool isActive() const { return activeNow.load(std::memory_order_relaxed); }
    int liveStep() const { return live.load(std::memory_order_relaxed); }
    float lfoPhase() const { return lfoShow.load(std::memory_order_relaxed); } // where the LFO is now (0..1, for the display)
    bool anyVoiceActive() const { for (auto& v : voices) if (v.active) return true; return false; }
    // v13: the playhead on screen follows the sound. The processor calls beginBlock() before an audio block and reads
    // the anchor after it: the step where the block started, steps per sample, the loop length and the swing.
    void beginBlock() { blockSample = 0; }
    struct Anchor { double start = -1.0, sps = 0.0; int total = 1; float swing = 0.0f; bool running = false; };
    Anchor anchor() const { return { anchorStart, spsNow, anchorTotal, cSwing, anchorRunning }; }

    double bpm() const { return hostBpm > 0.0 ? hostBpm : (double) prm.tempo; }

    void render(float* outL, float* outR, int n)
    {
        for (int i = 0; i < n; ++i) { outL[i] = 0.0f; outR[i] = 0.0f; }
        // v6: turning LOOP off (or leaving PLAY mode) ends a latched loop
        if (latched && (!prm.latch || prm.mode != 0 || (prm.oneShot != 0) != latchOS)) latched = false; // v13: One Shot can loop too
        if (!plan || plan->totalSteps() == 0 || !plan->sample) { live.store(-1); activeNow.store(false); lfoShow.store(0.0f); return; }
        const int spb = plan->stepsPerBar, total = plan->totalSteps();
        const double stepsPerSample = bpm() / 60.0 * (spb / 4.0) / sr;
        const bool running = prm.oneShot ? false : (hostHas ? hostPlaying : true);
        if (needSync)
        {
            // v9: only AUTO follows the bar position of the project; PLAY counts from the key press
            if (prm.mode == 1) stepPos = hostPpq * (spb / 4.0);
            freeBeats = hostPpq;
        }
        else if (!hostHas && !freeInit) { stepPos = 0.0; freeInit = true; }
        const double stepDur = 1.0 / stepsPerSample;
        const double beatsPerSample = bpm() / 60.0 / sr;
        // v6: the loop counts as playing only while a key is held (PLAY), the LOOP button holds it (latched),
        // or it runs by itself (AUTO). One Shot: while the sample sounds.
        const bool keysOn = !held.empty() || latched;
        const bool active = prm.oneShot ? (anyVoiceActive() || (latched && latchOS)) : (running && (prm.mode == 1 || keysOn));
        activeNow.store(active, std::memory_order_relaxed);
        spsNow = stepsPerSample;
        if (blockSample == 0) anchorStart = stepPos; // v13: where the loop is at the start of this audio block
        anchorTotal = total;
        anchorRunning = running && active;
        const bool split = prm.layUnlink != 0;       // v13: UNLINK - the layer goes its own way
        const bool lkFilter = layerLinked(prm, kLinkFilter), lkBal = layerLinked(prm, kLinkBalagan), lkWah = layerLinked(prm, kLinkWah),
                   lkChorus = layerLinked(prm, kLinkChorus), lkLfo = layerLinked(prm, kLinkLfo);

        for (int i = 0; i < n; ++i)
        {
            if ((ctlG++ & 15) == 0) updateGlobal();
            if (running)
            {
                const long long ns = (long long) std::floor(stepPos);
                const double frac = stepPos - (double) ns;
                const double delay = (ns & 1) ? cSwing * 0.33 : 0.0;
                if (ns != lastStep && frac >= delay)
                {
                    lastStep = ns;
                    triggerStep((int) (((ns % total) + total) % total), stepDur);
                }
                stepPos += stepsPerSample;
                // v6: the playhead only moves while something plays; otherwise it waits at the start
                live.store(active ? (int) (((long long) std::floor(stepPos) % total + total) % total) : -1, std::memory_order_relaxed);
            }
            else live.store(-1, std::memory_order_relaxed);
            if (roll.on) tickRoll();
            if (latched && latchOS)
            {
                // v13: LOOP in One Shot - the next pass starts when this one reaches the right locator
                osLoopT += 1.0;
                if (osLoopT >= osLoopLen) { osLoopT = 0.0; fireOneShot(latchNote, latchVel); }
            }
            ++blockSample;
            freeBeats += beatsPerSample;
            freeSec += 1.0 / sr;
            globalT += 1.0;

            float l = 0.0f, r = 0.0f, ll = 0.0f, lr = 0.0f; // v13: ll / lr = the unlinked layer (after the chorus)
            bool noiseMade = false;
            float nzL = 0.0f, nzR = 0.0f;
            for (auto& v : voices)
            {
                if (!v.active) continue;
                if ((v.ctl++ & 15) == 0) updateVoice(v);
                // amplitude envelope
                float env;
                if (v.relT < 0.0)
                {
                    if (v.t < v.cAtk) env = (float) (v.t / v.cAtk);
                    else if (v.t < v.cAtk + v.cDec) env = 1.0f + (v.cSus - 1.0f) * (float) ((v.t - v.cAtk) / v.cDec);
                    else env = v.cSus;
                    v.level = env;
                    v.t += 1.0;
                    if (v.t >= v.gateSamples) { v.relT = 0.0; v.relStartLevel = v.level; }
                }
                else
                {
                    env = v.relStartLevel * (1.0f - (float) (v.relT / v.cRel));
                    v.relT += 1.0;
                    if (v.relT >= v.cRel) { v.active = false; continue; }
                }
                v.envNow = env; // v12: the ENV source follows the envelope of this chop
                v.lfoT += 1.0;
                // read the sample (one read per unison voice, each with its own detune and pan)
                const auto& sm = *v.sample;
                const int N = sm.size();
                float xl = 0.0f, xr = 0.0f;
                if (!v.layerOnly)
                {
                    bool any = false;
                    for (int k = 0; k < v.nUni; ++k)
                    {
                        if (v.udone[k]) continue;
                        const int ip = (int) std::floor(v.upos[k]);
                        const bool outside = v.reverse ? (ip < v.segStart) : (ip >= v.segEnd - 1);
                        if (outside || ip < 0 || ip + 1 >= N) { v.udone[k] = true; continue; }
                        const float fr = (float) (v.upos[k] - ip);
                        xl += (sm.L[(size_t) ip] + (sm.L[(size_t) ip + 1] - sm.L[(size_t) ip]) * fr) * v.panL[k];
                        xr += (sm.R[(size_t) ip] + (sm.R[(size_t) ip + 1] - sm.R[(size_t) ip]) * fr) * v.panR[k];
                        v.upos[k] += v.reverse ? -v.cRate[k] : v.cRate[k];
                        any = true;
                    }
                    if (!any) { v.active = false; continue; }
                    {
                        // v10: tiny fade in the last ~2 ms of the slice audio - no click where a slice is cut
                        const double left = v.reverse ? v.upos[0] - (double) v.segStart : (double) (v.segEnd - 1) - v.upos[0];
                        if (left < v.fadeLen) { const float f = (float) std::max(0.0, left / v.fadeLen); xl *= f; xr *= f; }
                    }
                    xl *= v.cMain; xr *= v.cMain; // v9: MAIN MIX (the layer and the noise keep their own levels)
                    v.pos = v.upos[0];
                    if (v.cNoise > 0.0f)
                    {
                        // v7: the Noise knob mixes in the chosen texture (White = the v6 noise)
                        if (!noiseMade) { noiseGen.next(prm.noiseType, nzL, nzR); noiseMade = true; }
                        xl += nzL * v.cNoise * 0.35f; xr += nzR * v.cNoise * 0.35f;
                    }
                }
                float yl = 0.0f, yr = 0.0f;
                bool layerLeft = false;
                if (v.layer && v.cLayer > 0.0005f && !v.noLayer)
                {
                    // v7: ADD LAYER, read at its own speed from the start of the chop, then through the same filter and envelope
                    // v13: from its own start point (silence skipped / dragged), with its own envelope (Release ends at its end)
                    const auto& ls = *v.layer;
                    if (v.layerPos < 0.0) layerLeft = true; // still waiting (dragged later than the chop)
                    else
                    {
                        const int ip = (int) v.layerPos;
                        if (ip + 1 < ls.size())
                        {
                            const float fr = (float) (v.layerPos - ip);
                            const double since = (v.layerPos - v.layerOrigin) / v.layerStep;
                            const double left = ((double) (ls.size() - 1) - v.layerPos) / v.layerStep;
                            float le;
                            if (since < v.lAtk) le = (float) (std::max(0.0, since) / v.lAtk);
                            else if (since < v.lAtk + v.lDec) le = 1.0f + (v.lSus - 1.0f) * (float) ((since - v.lAtk) / v.lDec);
                            else le = v.lSus;
                            if (left < v.lRel) le *= (float) std::max(0.0, left / v.lRel);
                            const float g = v.cLayer * le;
                            yl = (ls.L[(size_t) ip] + (ls.L[(size_t) ip + 1] - ls.L[(size_t) ip]) * fr) * g;
                            yr = (ls.R[(size_t) ip] + (ls.R[(size_t) ip + 1] - ls.R[(size_t) ip]) * fr) * g;
                            layerLeft = true;
                        }
                    }
                    v.layerPos += v.layerStep;
                }
                if (v.layerOnly && !layerLeft) { v.active = false; continue; }
                if (!split) { xl += yl; xr += yr; yl = yr = 0.0f; } // as before: the layer is part of the chop
                if (v.useFilter)
                {
                    float* in[2] = { &xl, &xr };
                    for (int c = 0; c < 2; ++c)
                    {
                        const float v3 = *in[c] - v.s2[c];
                        const float v1 = v.fa1 * v.s1[c] + v.fa2 * v3;
                        const float v2 = v.s2[c] + v.fa2 * v.s1[c] + v.fa3 * v3;
                        v.s1[c] = 2.0f * v1 - v.s1[c];
                        v.s2[c] = 2.0f * v2 - v.s2[c];
                        if (ftype == 0) *in[c] = v2;                              // low-pass
                        else if (ftype == 1) *in[c] = *in[c] - v.fk * v1 - v2;    // high-pass
                        else *in[c] = v.fk * v1;                                  // band-pass (unity at the centre)
                    }
                }
                if (v.balF)
                {
                    // v9: BALAGAN filter - low-pass that moves along the roll
                    float* in[2] = { &xl, &xr };
                    for (int c = 0; c < 2; ++c)
                    {
                        const float v3 = *in[c] - v.bs2[c];
                        const float v1 = v.ba1 * v.bs1[c] + v.ba2 * v3;
                        const float v2 = v.bs2[c] + v.ba2 * v.bs1[c] + v.ba3 * v3;
                        v.bs1[c] = 2.0f * v1 - v.bs1[c];
                        v.bs2[c] = 2.0f * v2 - v.bs2[c];
                        *in[c] = v2;
                    }
                }
                if (v.wahOn && v.wMix > 0.0005f)
                {
                    // v6: wah = resonant band-pass mixed with the dry slice
                    float* in[2] = { &xl, &xr };
                    for (int c = 0; c < 2; ++c)
                    {
                        const float v3 = *in[c] - v.ws2[c];
                        const float v1 = v.wa1 * v.ws1[c] + v.wa2 * v3;
                        const float v2 = v.ws2[c] + v.wa2 * v.ws1[c] + v.wa3 * v3;
                        v.ws1[c] = 2.0f * v1 - v.ws1[c];
                        v.ws2[c] = 2.0f * v2 - v.ws2[c];
                        const float bp = v.wk * v1;
                        *in[c] = *in[c] * (1.0f - v.wMix) + bp * v.wMix * 1.6f;
                    }
                }
                l += xl * env * v.gain;
                r += xr * env * v.gain;
                if (split && (yl != 0.0f || yr != 0.0f))
                {
                    // v13: UNLINK - the layer only goes through the parts it is linked to (each with its own filter memory)
                    float* in[2] = { &yl, &yr };
                    if (lkFilter && (lkLfo ? v.useFilter : v.lUseFilter))
                    {
                        const float a1 = lkLfo ? v.fa1 : v.lfa1, a2 = lkLfo ? v.fa2 : v.lfa2, a3 = lkLfo ? v.fa3 : v.lfa3, fk = lkLfo ? v.fk : v.lfk;
                        for (int c = 0; c < 2; ++c)
                        {
                            const float v3 = *in[c] - v.lf2[c];
                            const float v1 = a1 * v.lf1[c] + a2 * v3;
                            const float v2 = v.lf2[c] + a2 * v.lf1[c] + a3 * v3;
                            v.lf1[c] = 2.0f * v1 - v.lf1[c];
                            v.lf2[c] = 2.0f * v2 - v.lf2[c];
                            if (ftype == 0) *in[c] = v2;
                            else if (ftype == 1) *in[c] = *in[c] - fk * v1 - v2;
                            else *in[c] = fk * v1;
                        }
                    }
                    if (lkBal && v.balF)
                        for (int c = 0; c < 2; ++c)
                        {
                            const float v3 = *in[c] - v.lb2[c];
                            const float v1 = v.ba1 * v.lb1[c] + v.ba2 * v3;
                            const float v2 = v.lb2[c] + v.ba2 * v.lb1[c] + v.ba3 * v3;
                            v.lb1[c] = 2.0f * v1 - v.lb1[c];
                            v.lb2[c] = 2.0f * v2 - v.lb2[c];
                            *in[c] = v2;
                        }
                    if (lkWah && v.wahOn && v.wMix > 0.0005f)
                        for (int c = 0; c < 2; ++c)
                        {
                            const float v3 = *in[c] - v.lw2[c];
                            const float v1 = v.wa1 * v.lw1[c] + v.wa2 * v3;
                            const float v2 = v.lw2[c] + v.wa2 * v.lw1[c] + v.wa3 * v3;
                            v.lw1[c] = 2.0f * v1 - v.lw1[c];
                            v.lw2[c] = 2.0f * v2 - v.lw2[c];
                            *in[c] = *in[c] * (1.0f - v.wMix) + v.wk * v1 * v.wMix * 1.6f;
                        }
                    if (lkChorus) { l += yl * env * v.layerGain; r += yr * env * v.layerGain; }
                    else { ll += yl * env * v.layerGain; lr += yr * env * v.layerGain; }
                }
            }
            // chorus: two modulated delay taps, one per side, a quarter cycle apart
            if (choLen > 4)
            {
                choL[(size_t) choW] = l; choR[(size_t) choW] = r;
                if (cChoMix > 0.0005f)
                {
                    const double base = 0.012 * sr, dep = 0.008 * sr * cChoDepth;
                    const double dl = base + dep * (0.5 + 0.5 * std::sin(choPh)), dr = base + dep * (0.5 + 0.5 * std::sin(choPh + 1.5707963));
                    auto tap = [&](const std::vector<float>& b, double d)
                    {
                        double rp = (double) choW - d;
                        while (rp < 0.0) rp += choLen;
                        const int i0 = (int) rp, i1 = (i0 + 1) % choLen;
                        const float f = (float) (rp - i0);
                        return b[(size_t) i0] + (b[(size_t) i1] - b[(size_t) i0]) * f;
                    };
                    const float wl = tap(choL, dl), wr = tap(choR, dr);
                    l = l * (1.0f - cChoMix) + wl * cChoMix;
                    r = r * (1.0f - cChoMix) + wr * cChoMix;
                }
                choPh += 6.283185307 * cChoRate / sr;
                if (choPh > 6.283185307) choPh -= 6.283185307;
                choW = (choW + 1) % choLen;
            }
            l *= cMaster; r *= cMaster;
            if (split) { const float lm = lkLfo ? cMaster : cMasterRaw; l += ll * lm; r += lr * lm; } // v13: unlinked from the chorus
            outL[i] = softClip(l);
            outR[i] = softClip(r);
        }
    }

private:
    struct Held { int note; float vel; };
    // v5: smooth limiter above 0.9 (no jump), so a loud Master stays clean
    static float softClip(float x)
    {
        const float a = std::fabs(x);
        if (a <= 0.9f) return x;
        const float y = 0.9f + 0.1f * std::tanh((a - 0.9f) / 0.1f);
        return x < 0.0f ? -y : y;
    }

    // ---------------------------------------------------------------- LFO
    double cycleSamples() const
    {
        if (prm.lfoSync) return std::max(16.0, (double) prm.lfoBeats * 60.0 / bpm() * sr);
        return sr / std::max(0.01, (double) prm.lfoHz);
    }
    float lfoAt(double phase, bool hold) const
    {
        if (!lfo) return 0.0f;
        if (hold && phase >= 1.0) return lfo->v[kLfoTable];
        phase -= std::floor(phase);
        const double x = phase * kLfoTable;
        const int i0 = std::min(kLfoTable - 1, (int) x);
        const float f = (float) (x - i0);
        return lfo->v[i0] + (lfo->v[i0 + 1] - lfo->v[i0]) * f;
    }
    double freePhase() const
    {
        return prm.lfoSync ? freeBeats / std::max(1.0e-3, (double) prm.lfoBeats) : freeSec * (double) prm.lfoHz;
    }
    // LFO value for one slice (Trig / Env start with the slice, Free follows the song)
    float voiceLfo(const SliceVoice& v) const
    {
        if (prm.lfoMode == 2) return lfoAt(freePhase(), false);
        return lfoAt(v.lfoT / cycleSamples(), prm.lfoMode == 1);
    }
    // v12: the LFO and the ENV both move the knob (each with its own amount)
    float modded(int d, float base, float lfoVal, float envVal = 0.0f) const
    {
        const float a = prm.modAmt[d], e = prm.envAmt[d];
        if (a == 0.0f && e == 0.0f) return base;
        return modFromNorm(d, modToNorm(d, base) + a * lfoVal + e * envVal);
    }
    // v12: attack / decay / sustain of a dot (its own values or the knobs) at `t` samples - the envelope of a whole
    // BALAGAN roll (the ENV on Balagan and on Bal. Filter moves along the roll, not along every tiny repeat)
    float adsAt(double t, const DotEnv& dt) const
    {
        const double a = std::max(0.002, (double) dt.get(kDotEnvAtk, prm.attackMs) * 0.001) * sr;
        const double d = std::max(0.002, (double) dt.get(kDotEnvDec, prm.decayMs) * 0.001) * sr;
        const float s = std::clamp(dt.get(kDotEnvSus, prm.sustain), 0.0f, 1.0f);
        if (t < a) return (float) (t / a);
        if (t < a + d) return 1.0f + (s - 1.0f) * (float) ((t - a) / d);
        return s;
    }
    bool anyMod() const { for (int d = 0; d < kModCount; ++d) if (prm.modAmt[d] != 0.0f) return true; return false; }

    // values that belong to the whole output (swing, master, chorus)
    void updateGlobal()
    {
        const float g = prm.lfoMode == 2 ? lfoAt(freePhase(), false) : lfoAt(globalT / cycleSamples(), prm.lfoMode == 1);
        const double show = prm.lfoMode == 2 ? freePhase() : globalT / cycleSamples();
        // v6: the LFO line moves only while something plays, otherwise it waits at the start
        const float shown = prm.lfoMode == 1 ? (float) std::min(1.0, show) : (float) (show - std::floor(show));
        lfoShow.store(activeNow.load(std::memory_order_relaxed) ? shown : 0.0f, std::memory_order_relaxed);
        cSwing = modded(mdSwing, prm.swing, g);
        cMaster = std::pow(10.0f, modded(mdMaster, prm.masterDb, g) / 20.0f);
        cMasterRaw = std::pow(10.0f, prm.masterDb / 20.0f); // v13: for a layer that is unlinked from the LFO
        cChoRate = modded(mdChoRate, prm.choRate, g);
        cChoDepth = modded(mdChoDepth, prm.choDepth, g);
        cChoMix = modded(mdChoMix, prm.choMix, g);
        ftype = std::clamp(prm.filterType, 0, 2);
    }

    // values of one slice
    void updateVoice(SliceVoice& v)
    {
        const float m = voiceLfo(v);
        const float ev = v.envNow; // v12: ENV
        // v11: a dot with its own values wins, then the box's own envelope (v10), then the knobs
        const auto& dt = v.dot;
        // v13: One Shot / LOCK ONE SHOT - Length does not close the sound, it always plays to the right locator
        const float length = v.fullLen ? 1.0f : modded(mdLength, dt.has(kDotEnvLen) ? dt.v[kDotEnvLen] : prm.length, m);
        v.gateSamples = std::max(32.0, (double) length * v.gateBase);
        // v10: a slice with its own envelope uses its own A / D / S / R (the LFO still moves them)
        const bool own = v.ownEnv.on;
        const float aMs = dt.has(kDotEnvAtk) ? dt.v[kDotEnvAtk] : (own ? v.ownEnv.attackMs : prm.attackMs);
        const float dMs = dt.has(kDotEnvDec) ? dt.v[kDotEnvDec] : (own ? v.ownEnv.decayMs : prm.decayMs);
        const float rMs = dt.has(kDotEnvRel) ? dt.v[kDotEnvRel] : (own ? v.ownEnv.releaseMs : prm.releaseMs);
        const float sus = dt.has(kDotEnvSus) ? dt.v[kDotEnvSus] : (own ? v.ownEnv.sustain : prm.sustain);
        v.cAtk = std::max(0.002, (double) modded(mdAttack, aMs, m) * 0.001) * sr;
        v.cDec = std::max(0.002, (double) modded(mdDecay, dMs, m) * 0.001) * sr;
        v.cRel = std::max(0.003, (double) modded(mdRelease, rMs, m) * 0.001) * sr;
        v.cSus = modded(mdSustain, sus, m);
        if (v.atkCap > 0.0) v.cAtk = std::min(v.cAtk, v.atkCap); // v8: short roll hits
        if (v.relCap > 0.0) v.cRel = std::min(v.cRel, v.relCap);
        if (v.fadeMin > 0.0) { v.cAtk = std::max(v.cAtk, v.fadeMin); v.cRel = std::max(v.cRel, v.fadeMin); } // v12: every repeat fades in / out
        // v12: chosen dots can have their own Noise, Pitch, Cutoff, Resonance and Volume
        v.cNoise = std::clamp(modded(mdNoise, dt.get(kDotNoise, prm.noise), m, ev), 0.0f, 1.0f);
        const double rate = v.baseRate * std::pow(2.0, (double) modded(mdPitch, dt.get(kDotPitch, prm.pitch), m, ev) / 12.0);
        v.rate = rate;
        v.fadeLen = std::max(8.0, 0.002 * sr * rate); // v10: ~2 ms of output, measured in source samples
        if (v.envFit)
        {
            // v10: SLICE - the envelope lives inside the slice: the release ends where the audio of the slice ends
            // (or where Length closes it, if that comes first), so Release really shortens / softens every chop
            const double audioLen = (double) std::max(1, v.segEnd - v.segStart) / std::max(1.0e-6, rate);
            const double gate = std::min(v.gateSamples, std::max(32.0, (double) length * audioLen)); // Length counts inside the slice too
            // the attack always gets to finish; a long release then fades over the rest of the slice
            v.cRel = std::max(0.003 * sr, std::min(v.cRel, gate - std::min(v.cAtk, gate * 0.5)));
            v.gateSamples = std::max(std::min(v.cAtk, gate * 0.5), gate - v.cRel);
        }
        const float detune = modded(mdUniDetune, prm.uniDetune, m), spread = modded(mdUniSpread, prm.uniSpread, m);
        for (int k = 0; k < v.nUni; ++k)
        {
            const float off = v.nUni > 1 ? (2.0f * (float) k / (float) (v.nUni - 1) - 1.0f) : 0.0f;
            v.cRate[k] = rate * std::pow(2.0, (double) (off * detune) / 1200.0);
            const float p = off * spread;
            // v8: the pan of the dot sits on top of the unison spread
            v.panL[k] = std::min(1.0f, 1.0f - p) * std::min(1.0f, 1.0f - v.stepPan);
            v.panR[k] = std::min(1.0f, 1.0f + p) * std::min(1.0f, 1.0f + v.stepPan);
        }
        v.cLayer = v.layer ? std::clamp(modded(mdLayerMix, prm.layerMix, m), 0.0f, 1.0f) : 0.0f; // v7
        const bool split = prm.layUnlink != 0, lkLfo = layerLinked(prm, kLinkLfo);
        if (v.layer)
        {
            // v13: the layer's own envelope, its speed (Pitch only when linked to it) and, with UNLINK, its own level
            if (split && !lkLfo) v.cLayer = std::clamp(prm.layerMix, 0.0f, 1.0f);
            v.lAtk = std::max(1.0, (double) prm.layAtkMs * 0.001 * sr);
            v.lDec = std::max(1.0, (double) prm.layDecMs * 0.001 * sr);
            v.lRel = std::max(1.0, (double) prm.layRelMs * 0.001 * sr);
            v.lSus = std::clamp(prm.laySus, 0.0f, 1.0f);
            double step = v.layer->sr / sr;
            if (layerLinked(prm, kLinkPitch))
            {
                const double r2 = lkLfo ? rate : v.baseRate * std::pow(2.0, (double) modded(mdPitch, dt.get(kDotPitch, prm.pitch), 0.0f, ev) / 12.0);
                step *= r2 / std::max(1.0e-9, v.sampleRatio);
            }
            v.layerStep = step;
        }
        const float vol = modded(mdVolume, dt.get(kDotVolume, prm.volume), m), velAmt = modded(mdVelocity, prm.velocity, m);
        const float uniNorm = 1.0f / std::sqrt((float) v.nUni);
        // v5: the dot is a level in dB (top = 0 dB = the original). Velocity Depth scales that dB:
        // 100 % = the dot as set, 50 % = half the dB (-20 dB -> -10 dB), 0 % = every slice at full level
        v.gain = uniNorm * (v.oneShot ? vol * (1.0f - velAmt + velAmt * v.midiVel)
                                      : vol * sliceLevel(v.sliceVel, velAmt) * (0.6f + 0.4f * v.midiVel));
        if (split)
        {
            // v13: UNLINK - the layer keeps the dot's level and the Volume, but not the unison / LFO it is unlinked from
            const float lvol = lkLfo ? vol : dt.get(kDotVolume, prm.volume), lvel = lkLfo ? velAmt : prm.velocity;
            v.layerGain = (layerLinked(prm, kLinkUnison) ? uniNorm : 1.0f)
                          * (v.oneShot ? lvol * (1.0f - lvel + lvel * v.midiVel) : lvol * sliceLevel(v.sliceVel, lvel) * (0.6f + 0.4f * v.midiVel));
        }
        const float cutoff = modded(mdCutoff, dt.get(kDotCutoff, prm.cutoff), m, ev);
        const float res = std::clamp(modded(mdResonance, dt.get(kDotRes, prm.resonance), m), 0.0f, 1.0f);
        v.cMain = std::clamp(prm.mainMix, 0.0f, 1.0f); // v9
        v.balF = false;
        if (v.roll)
        {
            // v9: BALAGAN -> Filter. Its own low-pass, never closed all the way, so the roll is always heard.
            // + = the roll starts dark and opens up, - = starts open and closes. The open end is the Cutoff knob (LP) or fully
            // open (HP / BP); the amount sets how far below that the dark end is: 100 % = 1/25 of it (never under 400 Hz).
            const float rollEnv = prm.envAmt[mdBalFilter] != 0.0f ? adsAt(v.rollAt + v.lfoT, dt) : 0.0f; // v12: ENV along the roll
            const float amt = std::clamp(modded(mdBalFilter, prm.balFilter, m, rollEnv), -1.0f, 1.0f);
            if (std::fabs(amt) > 0.001f)
            {
                const float prog = (float) std::clamp((v.rollAt + v.lfoT) / std::max(1.0, v.rollLen), 0.0, 1.0);
                const float depth = std::fabs(amt) * (amt > 0.0f ? 1.0f - prog : prog); // 0 = open, 1 = darkest
                const float fOpen = ftype == 0 ? std::min(18000.0f, cutoff) : 18000.0f;
                const float f = std::max(400.0f, fOpen * std::pow(1.0f / 25.0f, depth));
                const float k = 1.0f; // a light peak, so the sweep is easy to hear
                const float bg = std::tan(3.14159265f * std::min(f, 0.45f * (float) sr) / (float) sr);
                v.ba1 = 1.0f / (1.0f + bg * (bg + k)); v.ba2 = bg * v.ba1; v.ba3 = bg * v.ba2;
                v.balF = true;
            }
        }
        v.useFilter = cutoff < 19000.0f || ftype != 0 || res > 0.001f;
        // resonance 0 = the original gentle filter (k = 1.4), 100 % = sharp peak (k = 0.08)
        v.fk = 1.4f - 1.32f * res;
        const float fg = std::tan(3.14159265f * std::min(cutoff, 0.45f * (float) sr) / (float) sr);
        v.fa1 = 1.0f / (1.0f + fg * (fg + v.fk)); v.fa2 = fg * v.fa1; v.fa3 = fg * v.fa2;
        if (split && v.layer && !lkLfo)
        {
            // v13: the layer is linked to the filter but not to the LFO - the same filter, without the LFO's movement
            const float c2 = modded(mdCutoff, dt.get(kDotCutoff, prm.cutoff), 0.0f, ev);
            const float r2 = std::clamp(dt.get(kDotRes, prm.resonance), 0.0f, 1.0f);
            v.lUseFilter = c2 < 19000.0f || ftype != 0 || r2 > 0.001f;
            v.lfk = 1.4f - 1.32f * r2;
            const float g2 = std::tan(3.14159265f * std::min(c2, 0.45f * (float) sr) / (float) sr);
            v.lfa1 = 1.0f / (1.0f + g2 * (g2 + v.lfk)); v.lfa2 = g2 * v.lfa1; v.lfa3 = g2 * v.lfa2;
        }
        // v6: wah sweep - opens fast at the start of the slice, then falls back. Speed sets how long it takes.
        v.wMix = v.wahOn ? std::clamp(prm.wahAmt, 0.0f, 1.0f) : 0.0f;
        if (v.wMix > 0.0f)
        {
            const double sweep = 0.7 - 0.62 * (double) std::clamp(prm.wahSpeed, 0.0f, 1.0f); // 0.7 s .. 0.08 s
            const double atk = sweep * 0.18, tsec = v.lfoT / sr;
            const double w = tsec < atk ? tsec / atk : std::exp(-(tsec - atk) / sweep * 2.2);
            const float f = 320.0f * std::pow(9.0f, (float) w); // 320 Hz .. 2.9 kHz
            v.wk = 0.3f;
            const float wg = std::tan(3.14159265f * std::min(f, 0.45f * (float) sr) / (float) sr);
            v.wa1 = 1.0f / (1.0f + wg * (wg + v.wk)); v.wa2 = wg * v.wa1; v.wa3 = wg * v.wa2;
        }
    }

    SliceVoice* grabVoice()
    {
        for (auto& c : voices) if (!c.active) return &c;
        SliceVoice* v = &voices[0];
        for (auto& c : voices) if (c.age < v->age) v = &c;
        return v;
    }

    void startVoice(SliceVoice* v, int a, int b, bool rev, double baseRate)
    {
        v->active = true;
        v->sample = plan->sample;
        v->baseRate = baseRate;
        v->segStart = a; v->segEnd = b; v->reverse = rev;
        v->pos = rev ? (double) (b - 2) : (double) a;
        v->nUni = std::clamp(prm.uniVoices, 1, 8);
        for (int k = 0; k < 8; ++k) { v->upos[k] = v->pos; v->udone[k] = false; }
        v->t = 0.0; v->relT = -1.0; v->level = 0.0f; v->lfoT = 0.0;
        v->s1[0] = v->s1[1] = v->s2[0] = v->s2[1] = 0.0f;
        v->ws1[0] = v->ws1[1] = v->ws2[0] = v->ws2[1] = 0.0f;
        v->bs1[0] = v->bs1[1] = v->bs2[0] = v->bs2[1] = 0.0f; // v9
        v->layer = layer; // v7: the layer starts from the top with every chop
        v->layerOnly = curLayerOnly; v->noLayer = curNoLayer; v->fullLen = curFullLen; // v13
        v->sampleRatio = plan->sample->sr / sr;
        if (layer)
        {
            // v13: from its own start point (the silence before the sound is skipped; dragged = earlier / later)
            const double st = (double) prm.layerStartSec * layer->sr;
            v->layerPos = std::min(st, (double) std::max(0, layer->size() - 2));
            v->layerOrigin = std::max(0.0, v->layerPos);
            v->layerStep = layer->sr / sr;
        }
        else v->layerPos = 0.0;
        for (int c = 0; c < 2; ++c) v->lf1[c] = v->lf2[c] = v->lb1[c] = v->lb2[c] = v->lw1[c] = v->lw2[c] = 0.0f;
        v->stepPan = curPan; v->roll = curRoll; v->rollAt = curRollAt; v->rollLen = curRollLen;
        v->atkCap = curAtkCap; v->relCap = curRelCap;
        v->fadeMin = curRoll ? curFade : 0.0; v->envNow = 0.0f; // v12
        v->age = ++ageCounter;
        v->ctl = 0;
        globalT = 0.0;
        updateVoice(*v);
        v->ctl = 1;
    }

    // One Shot: the region between the markers plays once, every knob shapes it
    void fireOneShot(int note, float midiVel)
    {
        if (!plan || !plan->sample) return;
        const int a = plan->regionStart, b = plan->regionEnd;
        if (b - a < 16) return;
        float rndSemi = 0.0f;
        osState = osState * 1664525u + 1013904223u;
        const float u1 = (float) (osState >> 8) / 16777216.0f;
        osState = osState * 1664525u + 1013904223u;
        const float u2 = (float) (osState >> 8) / 16777216.0f;
        if (prm.random > 0.0f && u1 < prm.random * 0.5f) rndSemi = std::round((u2 * 2.0f - 1.0f) * 7.0f * prm.random);
        osState = osState * 1664525u + 1013904223u;
        const bool rev = (float) (osState >> 8) / 16777216.0f < prm.reverse;
        SliceVoice* v = grabVoice();
        const float semi = (prm.keytrack ? (float) (note - 60) : 0.0f) + rndSemi;
        const double baseRate = std::pow(2.0, (double) semi / 12.0) * plan->sample->sr / sr;
        v->oneShot = true; v->midiVel = midiVel; v->sliceVel = 1.0f; v->wahOn = false;
        // v12: the envelope lives between the locators - Attack starts at the left one, Release ends at the right one
        // (or where Length closes the sound, if that comes first). Moving a locator moves the envelope with it.
        v->envFit = true; v->ownEnv = SegEnv();
        v->dot = DotEnv();                       // v11
        curPan = 0.0f; curRoll = false; curAtkCap = curRelCap = 0.0;
        // Length = how much of the sample plays before the release starts (100 % = all of it)
        v->gateBase = (double) (b - a) / (baseRate * std::pow(2.0, (double) prm.pitch / 12.0));
        curFullLen = true; // v13: Length does not cut a one shot - it plays to the right locator
        curLayerOnly = curNoLayer = false;
        startVoice(v, a, b, rev, baseRate);
        curFullLen = false;
        osLoopLen = std::max(64.0, (double) (b - a) / std::max(1.0e-6, v->rate)); // v13: LOOP - one pass, left to right locator
    }

    // fires one chop for every key that is down (PLAY), or one key (AUTO)
    void fireForKeys(const Slice& sl, int a, int b, double gateBase)
    {
        auto fire = [&](int note, float midiVel)
        {
            SliceVoice* v = grabVoice();
            const float semi = (prm.keytrack ? (float) (note - 60) : 0.0f) + sl.semi;
            const double baseRate = std::pow(2.0, (double) semi / 12.0) * plan->sample->sr / sr;
            v->oneShot = false; v->midiVel = midiVel; v->sliceVel = sl.vel; v->wahOn = sl.wah;
            v->ownEnv = sl.env; v->envFit = prm.envSlice != 0 || sl.env.on || sl.trim; // v10 (v12: a dot with its own locators too)
            v->dot = sl.dot; // v11
            v->gateBase = gateBase;
            const double pitchF = std::pow(2.0, (double) sl.dot.get(kDotPitch, prm.pitch) / 12.0); // v12: the dot's own Pitch
            if (gateBase < 0.0)
            {
                // v11: LOCK ONE SHOT - the dot plays the locked one shot exactly like One Shot does:
                // Length = how much of it plays (100 % = all), the envelope is not squeezed into the step.
                // v12: ... and like One Shot the envelope lives between the locators (the dot's own ones, if it has them)
                v->gateBase = (double) (b - a) / (baseRate * pitchF);
                v->envFit = true;
            }
            curFullLen = gateBase < 0.0; // v13: the locked one shot plays to its right locator too, Length does not cut it
            int a2 = a, b2 = b;
            if (curRoll && prm.balSuper)
            {
                // SUPER: the hit starts from a random place inside the chop (keeps at least one hit of audio)
                const double hitSrc = gateBase * baseRate * pitchF;
                const int room = (b - a) - (int) std::min((double) (b - a) * 0.5, std::max(64.0, hitSrc));
                if (room > 16)
                {
                    const int off = (int) (rnd01() * (float) room);
                    if (sl.reverse) b2 = b - off; else a2 = a + off;
                }
            }
            startVoice(v, a2, b2, sl.reverse, baseRate);
            curFullLen = false;
        };
        if (prm.mode == 0)
        {
            for (auto& h : held) fire(h.note, h.vel);
            if (latched) fire(latchNote, latchVel); // v6: LOOP keeps playing after the key is released
        }
        else
        {
            if (held.empty()) fire(60, 1.0f);
            else fire(held.back().note, held.back().vel);
        }
    }

    void triggerStep(int loopStep, double stepDur)
    {
        const Slice& sl = plan->slices[(size_t) loopStep];
        if (sl.vel <= 0.001f || sl.end - sl.start < 16) return;
        roll.on = false;                       // a new step that plays ends any roll that is still going
        curPan = sl.pan;
        curRoll = false; curAtkCap = curRelCap = 0.0;
        if (sl.bal)
        {
            // v8: BALAGAN - the chop repeats on its own fast clock, only inside this step
            // v13: a layer that is UNLINKED from Balagan does not repeat: it plays the step once, like a normal dot
            const bool sepLayer = layer != nullptr && prm.layUnlink != 0 && (prm.layLinks & kLinkBalagan) == 0;
            roll.on = true; roll.sl = sl; roll.t = 0.0; roll.len = stepDur * (double) sl.len;
            const double iv = rollInterval();
            roll.next = iv;
            curRoll = true; curRollAt = 0.0; curRollLen = roll.len;
            setRollCaps(iv);
            curNoLayer = sepLayer;
            fireForKeys(sl, sl.start, sl.end, std::min(iv, roll.len));
            curNoLayer = false;
            curRoll = false; curAtkCap = curRelCap = curFade = 0.0;
            if (sepLayer)
            {
                curLayerOnly = true;
                fireForKeys(sl, sl.start, sl.end, sl.whole ? -1.0 : stepDur * (double) sl.len);
                curLayerOnly = false;
            }
            return;
        }
        fireForKeys(sl, sl.start, sl.end, sl.whole ? -1.0 : stepDur * (double) sl.len); // v11: -1 = the whole one shot
    }

    // v8: BALAGAN knob 1..100 -> repeats per quarter note.
    // v12 FREE: 2^(2 + c/25) with c = the knob through a curve (1 = about 1/16, 100 = 1/256, more room in the middle).
    // v12 BPM: the knob picks a note value (1/8, 1/8T, 1/16 ... 1/256) and every repeat lands on the beat grid.
    double rollInterval() const
    {
        float lv = 0.0f, ev = 0.0f;
        if (prm.modAmt[mdBalagan] != 0.0f)
            lv = prm.lfoMode == 2 ? lfoAt(freePhase(), false) : lfoAt(roll.t / cycleSamples(), prm.lfoMode == 1);
        if (prm.envAmt[mdBalagan] != 0.0f) ev = adsAt(roll.t, roll.sl.dot); // v12: ENV - the roll speeds up / slows down along the dot
        const float k = std::clamp(modded(mdBalagan, prm.balagan, lv, ev), 1.0f, 100.0f);
        const double perQuarter = balaganPerQuarter(k, prm.balMode);
        return std::max(48.0, 60.0 / bpm() * sr / perQuarter);
    }
    // the short hits of a roll keep their attack / release inside the hit. v12: and every hit fades in and out
    // (about 1.5 ms, less when the repeats are very fast), so a fast roll stays clean, without clicks.
    void setRollCaps(double iv)
    {
        curAtkCap = iv * 0.25; curRelCap = std::max(24.0, iv * 0.35);
        curFade = std::max(12.0, std::min(0.0015 * sr, iv * 0.2));
    }
    void tickRoll()
    {
        roll.t += 1.0;
        if (roll.t >= roll.len) { roll.on = false; return; }
        if (roll.t < roll.next) return;
        const double iv = rollInterval();
        roll.next += iv;
        if (roll.len - roll.t < 24.0) return; // no click-sized hit at the very end of the step
        curPan = roll.sl.pan;
        curRoll = true; curRollAt = roll.t; curRollLen = roll.len;
        setRollCaps(iv);
        curNoLayer = layer != nullptr && prm.layUnlink != 0 && (prm.layLinks & kLinkBalagan) == 0; // v13
        fireForKeys(roll.sl, roll.sl.start, roll.sl.end, std::min(iv, roll.len - roll.t));
        curNoLayer = false;
        curRoll = false; curAtkCap = curRelCap = curFade = 0.0;
    }
    float rnd01() { balRnd = balRnd * 1664525u + 1013904223u; return (float) (balRnd >> 8) / 16777216.0f; }

    double sr = 44100.0;
    Params prm;
    std::shared_ptr<const Plan> plan;
    std::shared_ptr<const LfoTable> lfo;
    std::shared_ptr<const SampleData> layer; // v7
    NoiseGen noiseGen;                       // v7
    // v8: BALAGAN roll + what the next started voice gets
    struct Roll { bool on = false; Slice sl; double t = 0.0, len = 0.0, next = 0.0; } roll;
    float curPan = 0.0f;
    bool curRoll = false;
    bool curLayerOnly = false, curNoLayer = false, curFullLen = false; // v13
    double curRollAt = 0.0, curRollLen = 1.0, curAtkCap = 0.0, curRelCap = 0.0, curFade = 0.0;
    uint32_t balRnd = 4242u;
    std::vector<Held> held;
    SliceVoice voices[24];
    uint64_t ageCounter = 0;
    uint32_t noiseState = 22222u, osState = 777u;
    double stepPos = 0.0, hostPpq = 0.0, hostBpm = 0.0;
    long long lastStep = -1;
    bool hostHas = false, hostPlaying = false, needSync = false, freeInit = false, internalClock = false;
    std::atomic<int> live { -1 };
    // v6: LOOP (latch) + "is something playing" for the playheads
    bool latched = false;
    bool latchOS = false;                    // v13: the LOOP holds a One Shot
    double osLoopT = 0.0, osLoopLen = 1.0e9; // v13: where in the One Shot loop pass we are, and how long a pass is
    int latchNote = 60;
    float latchVel = 1.0f;
    std::atomic<bool> activeNow { false };
    std::atomic<float> lfoShow { 0.0f };
    // LFO clocks
    double freeBeats = 0.0, freeSec = 0.0, globalT = 1.0e9;
    unsigned ctlG = 0;
    // control-rate global values
    float cSwing = 0.0f, cMaster = 0.7f, cMasterRaw = 0.7f, cChoRate = 0.6f, cChoDepth = 0.5f, cChoMix = 0.0f;
    // v13: the playhead - where the loop was at the start of the audio block (the screen goes on from there)
    double anchorStart = -1.0, spsNow = 0.0;
    int blockSample = 0, anchorTotal = 1;
    bool anchorRunning = false;
    int ftype = 0;
    // chorus
    std::vector<float> choL, choR;
    int choLen = 0, choW = 0;
    double choPh = 0.0;
};
} // namespace bs
