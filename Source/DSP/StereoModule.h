#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <vector>
#include "Biquad.h"

namespace org
{
struct StereoParams
{
    int mode = 1;                                 // 0 sides, 1 pan, 2 mono
    float low = 0.0f, mid = 0.0f, high = 1.5f;    // dB
    float monoHz = 120.0f;
    float width = 1.2f;                           // 0..2
    float pan = 0.0f;                             // -1..1
    float haasMs = 6.0f;
    float wander = 0.5f;                          // 0..1

    bool sameEq (const StereoParams& o) const noexcept { return low == o.low && mid == o.mid && high == o.high && monoHz == o.monoHz; }
};

// 3-band EQ, then a mid/side stage. SIDES pushes the hit outwards (and flips side on every
// hit), PAN places it with a per-hit Wander, MONO folds it to the centre.
class StereoModule
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        haasBuf.assign ((size_t) kSize, 0.0f);
        w = 0;
        for (auto* b : { &lsL, &lsR, &pkL, &pkR, &hsL, &hsR, &sideLp })
            b->reset();
        wanderS.setTime (sr, 25.0);
        flipS.setTime (sr, 6.0);
        haasS.setTime (sr, 30.0);
        wanderS.v = 0.0f;
        flipS.v = 1.0f;
        eqDirty = true;
        updateEq();
    }

    void setParams (const StereoParams& np) noexcept
    {
        eqDirty = eqDirty || ! np.sameEq (p);
        p = np;
        if (eqDirty)
            updateEq();
    }

    void onHit (juce::Random& rng) noexcept
    {
        wanderTarget = (rng.nextFloat() * 2.0f - 1.0f) * p.wander;
        flip = -flip;
    }

    void process (float& l, float& r, float wet) noexcept
    {
        float el = hsL.process (pkL.process (lsL.process (l)));
        float er = hsR.process (pkR.process (lsR.process (r)));

        haasBuf[(size_t) w] = er;
        const float hd = haasS.next (p.mode == 2 ? 0.0f : p.haasMs * 0.001f * (float) sr);
        if (hd >= 1.0f)
            er = read (hd);
        w = (w + 1) & kMask;

        float mid = (el + er) * 0.5f, side = (el - er) * 0.5f;
        side -= sideLp.process (side);

        float gl = 1.0f, gr = 1.0f;
        const float wv = wanderS.next (wanderTarget);
        const float fl = flipS.next (flip);
        switch (p.mode)
        {
            case 0:
                side *= p.width * 1.3f * fl;
                mid *= 1.0f - 0.35f * juce::jmin (1.0f, p.width);
                break;
            case 1:
            {
                side *= p.width;
                const float pos = juce::jlimit (-1.0f, 1.0f, p.pan + wv);
                const float a = (pos + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
                gl = juce::jmin (1.0f, std::cos (a) * juce::MathConstants<float>::sqrt2);
                gr = juce::jmin (1.0f, std::sin (a) * juce::MathConstants<float>::sqrt2);
                break;
            }
            default:
                side = 0.0f;
                break;
        }

        const float ol = (mid + side) * gl, orr = (mid - side) * gr;
        l += (ol - l) * wet;
        r += (orr - r) * wet;
    }

private:
    void updateEq()
    {
        lsL.lowShelf (sr, 220.0, p.low);
        pkL.peak (sr, 2500.0, 0.8, p.mid);
        hsL.highShelf (sr, 8000.0, p.high);
        for (auto pr : { std::make_pair (&lsL, &lsR), std::make_pair (&pkL, &pkR), std::make_pair (&hsL, &hsR) })
        {
            pr.second->b0 = pr.first->b0; pr.second->b1 = pr.first->b1; pr.second->b2 = pr.first->b2;
            pr.second->a1 = pr.first->a1; pr.second->a2 = pr.first->a2;
        }
        sideLp.lowPass (sr, p.monoHz, 0.707);
        eqDirty = false;
    }

    float read (float d) const noexcept
    {
        float pos = (float) w - d;
        if (pos < 0.0f)
            pos += (float) kSize;
        const int i = (int) pos;
        const float f = pos - (float) i;
        const float x0 = haasBuf[(size_t) (i & kMask)], x1 = haasBuf[(size_t) ((i + 1) & kMask)];
        return x0 + f * (x1 - x0);
    }

    static constexpr int kSize = 1 << 13, kMask = kSize - 1;
    double sr = 44100.0;
    StereoParams p;
    Biquad lsL, lsR, pkL, pkR, hsL, hsR, sideLp;
    std::vector<float> haasBuf;
    int w = 0;
    bool eqDirty = true;
    float wanderTarget = 0.0f, flip = 1.0f;
    Smooth wanderS, flipS, haasS;
};
} // namespace org
