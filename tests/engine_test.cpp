// offline engine test: synthetic sample of 8 clicks/tones, check onset detection, step timing and level
#include <cstdio>
#include <cmath>
#include "../Source/Rhythms.h"
using namespace bs;

int main()
{
    auto sm = std::make_shared<SampleData>();
    sm->sr = 44100.0;
    const int N = 44100 * 2;                       // 2 s
    sm->L.assign((size_t) N, 0.0f); sm->R.assign((size_t) N, 0.0f);
    const int onsets[] = { 0, 4000, 11000, 20000, 33000, 45000, 60000, 75000 };
    for (int k = 0; k < 8; ++k)
        for (int i = 0; i < 6000 && onsets[k] + i < N; ++i)
        {
            const float e = std::exp(-i / 1500.0f);
            const float v = 0.7f * e * std::sin(2.0f * 3.14159f * (200.0f + 100.0f * k) * i / 44100.0f);
            sm->L[(size_t) (onsets[k] + i)] += v; sm->R[(size_t) (onsets[k] + i)] += v;
        }
    Params p; p.grid = 1; p.bars = 0;              // 8 steps, 1 bar
    float pat[kPatternLen] = {};
    for (int i = 0; i < 8; ++i) pat[i] = 1.0f;
    auto plan = buildPlan(sm, p, pat, {});
    int ok = 0;
    for (int i = 0; i < plan->totalSteps(); ++i)
    {
        std::printf("step %d -> start %d end %d (expected near %d)\n", i, plan->slices[(size_t) i].start, plan->slices[(size_t) i].end, onsets[i]);
        if (std::abs(plan->slices[(size_t) i].start - onsets[i]) < 600) ++ok;
    }
    std::printf("onsets matched: %d / 8\n", ok);
    if (ok < 7) { std::printf("FAIL onsets\n"); return 1; }

    // playback: 120 bpm, 1/8 steps => step = 0.25 s = 11025 samples. AUTO mode, free-running
    Engine eng; eng.prepare(44100.0);
    p.mode = 1; p.length = 0.5f; p.cutoff = 20000.0f;
    eng.setParams(p); eng.setPlan(plan); eng.setHost(false, false, 0.0, 0.0);
    const int total = 44100 * 2;
    std::vector<float> L((size_t) total), R((size_t) total);
    for (int pos = 0; pos < total; pos += 512) { int n = std::min(512, total - pos); eng.render(L.data() + pos, R.data() + pos, n); }
    // find the start of each step: energy in the first 200 samples after each expected step boundary
    int audible = 0;
    for (int k = 0; k < 8; ++k)
    {
        double e = 0; for (int i = 0; i < 400; ++i) e += std::fabs(L[(size_t) (k * 11025 + 20 + i)]);
        std::printf("step %d energy %.3f\n", k, e);
        if (e > 1.0) ++audible;
    }
    float pk = 0; for (float v : L) pk = std::max(pk, std::fabs(v));
    std::printf("peak %.3f, audible steps %d\n", pk, audible);
    if (audible != 8 || pk > 1.0f || pk < 0.05f) { std::printf("FAIL playback\n"); return 1; }

    // mute step 3 via pattern, random mode must not crash
    pat[3] = 0.0f; p.pick = 2; p.random = 0.5f;
    auto plan2 = buildPlan(sm, p, pat, {});
    std::printf("muted step vel %.1f total steps %d\n", plan2->slices[3].vel, plan2->totalSteps());
    std::printf("ENGINE TEST OK\n");
    return 0;
}
