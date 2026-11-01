#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <vector>
#include "Biquad.h"

namespace org
{
struct BloomParams
{
    float x = 0.3f;      // 0 chorus .. 0.5 flanger .. 1 phaser
    float y = 0.55f;     // depth
    float rate = 0.6f;   // Hz
    float fb = 0.35f;    // 0..0.95
    float wind = 0.5f;   // per-hit drift
    float moss = 0.35f;  // wandering dark filter
    float grain = 0.2f;  // soft saturation
    float wet = 0.6f;
};

// A living modulation unit: chorus, flanger and phaser on one morph axis. Every hit gives
// the "wind" a new direction (rate, depth, morph), and a dark filter wanders slowly.
class BloomModule
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        bufL.assign ((size_t) kSize, 0.0f);
        bufR.assign ((size_t) kSize, 0.0f);
        w = 0;
        phase = 0.0;
        fbL = fbR = phL = phR = 0.0f;
        for (auto& a : apL) a = 0.0f;
        for (auto& a : apR) a = 0.0f;
        rateS.setTime (sr, 80.0);
        depthS.setTime (sr, 80.0);
        xS.setTime (sr, 120.0);
        walkS.setTime (sr, 600.0);
        rateS.v = depthS.v = 1.0f;
        xS.v = walkS.v = 0.0f;
        mossL.reset();
        mossR.reset();
        counter = 0;
    }

    void setParams (const BloomParams& np) noexcept { p = np; }

    void onHit (juce::Random& hitRng) noexcept
    {
        windRate = 1.0f + (hitRng.nextFloat() - 0.5f) * 1.4f * p.wind;
        windDepth = 1.0f + (hitRng.nextFloat() - 0.5f) * p.wind;
        windX = (hitRng.nextFloat() - 0.5f) * 0.35f * p.wind;
    }

    void process (float& l, float& r, float wet) noexcept
    {
        const float rm = rateS.next (windRate), dm = depthS.next (windDepth), xo = xS.next (windX);
        const float x = juce::jlimit (0.0f, 1.0f, p.x + xo);
        const float depth = juce::jlimit (0.0f, 1.5f, p.y * dm);

        phase += (double) (p.rate * rm) / sr;
        if (phase >= 1.0)
            phase -= 1.0;
        const float twoPi = juce::MathConstants<float>::twoPi;
        const float s0 = std::sin ((float) phase * twoPi), s1 = std::sin ((float) phase * twoPi + 1.5708f);
        const float s2 = std::sin ((float) phase * twoPi + 3.1416f);

        const float wc = juce::jlimit (0.0f, 1.0f, 1.0f - 2.0f * x);
        const float wf = 1.0f - std::abs (2.0f * x - 1.0f);
        const float wp = juce::jlimit (0.0f, 1.0f, 2.0f * x - 1.0f);

        // shared delay line, flanger feedback goes back into it
        const float fbAmt = juce::jmin (0.92f, p.fb) * wf;
        bufL[(size_t) w] = l + fbL * fbAmt;
        bufR[(size_t) w] = r + fbR * fbAmt;

        float yl = 0.0f, yr = 0.0f;
        if (wc > 0.0f)
        {
            const float fs = (float) sr;
            const float cl = 0.5f * (read (bufL, (0.016f + 0.005f * depth * s0) * fs) + read (bufL, (0.023f + 0.004f * depth * s2) * fs));
            const float cr = 0.5f * (read (bufR, (0.016f + 0.005f * depth * s1) * fs) + read (bufR, (0.021f - 0.004f * depth * s0) * fs));
            yl += wc * cl;
            yr += wc * cr;
        }
        if (wf > 0.0f)
        {
            const float fs = (float) sr;
            const float fl = read (bufL, (0.0008f + 0.0035f * depth * (0.5f + 0.5f * s0)) * fs);
            const float fr = read (bufR, (0.0008f + 0.0035f * depth * (0.5f + 0.5f * s1)) * fs);
            fbL = fl;
            fbR = fr;
            yl += wf * fl;
            yr += wf * fr;
        }
        if (wp > 0.0f)
        {
            const float fbP = juce::jmin (0.7f, p.fb * 0.75f);
            yl += wp * phaser (apL, l + phL * fbP, 0.5f + 0.5f * s0, depth, phL);
            yr += wp * phaser (apR, r + phR * fbP, 0.5f + 0.5f * s1, depth, phR);
        }
        w = (w + 1) & kMask;

        // moss: a low-pass whose cutoff walks around slowly
        if ((counter++ & 63) == 0)
        {
            if ((counter & 8191) == 1)
                walkTarget = rng.nextFloat() * 2.0f - 1.0f;
            const float base = 18000.0f * std::pow (1200.0f / 18000.0f, p.moss);
            const float fc = juce::jlimit (300.0f, 19000.0f, base * std::exp2 (walkS.v * p.moss * 1.2f));
            mossL.lowPass (sr, fc, 0.6);
            mossR.b0 = mossL.b0; mossR.b1 = mossL.b1; mossR.b2 = mossL.b2; mossR.a1 = mossL.a1; mossR.a2 = mossL.a2;
        }
        walkS.next (walkTarget);
        if (p.moss > 0.01f)
        {
            yl = mossL.process (yl);
            yr = mossR.process (yr);
        }

        if (p.grain > 0.01f)
        {
            const float drive = 1.0f + p.grain * 6.0f, norm = 1.0f / std::tanh (drive);
            yl = std::tanh (yl * drive) * norm;
            yr = std::tanh (yr * drive) * norm;
        }

        const float amt = wet * p.wet;
        l = l * (1.0f - amt * 0.5f) + yl * amt;
        r = r * (1.0f - amt * 0.5f) + yr * amt;
    }

private:
    float phaser (std::array<float, 6>& st, float x, float lfo, float depth, float& out) noexcept
    {
        const float f = 180.0f * std::pow (25.0f, juce::jlimit (0.0f, 1.0f, lfo * depth));
        const float t = std::tan (juce::MathConstants<float>::pi * juce::jmin (f, (float) sr * 0.45f) / (float) sr);
        const float a = (t - 1.0f) / (t + 1.0f);
        float y = x;
        for (auto& z : st)
        {
            const float v = a * y + z;
            z = y - a * v;
            y = v;
        }
        out = y;
        return 0.5f * (x + y);
    }

    float read (const std::vector<float>& b, float d) const noexcept
    {
        float pos = (float) w - juce::jmax (1.0f, d);
        if (pos < 0.0f)
            pos += (float) kSize;
        const int i = (int) pos;
        const float f = pos - (float) i;
        const float x0 = b[(size_t) (i & kMask)], x1 = b[(size_t) ((i + 1) & kMask)];
        return x0 + f * (x1 - x0);
    }

    static constexpr int kSize = 1 << 14, kMask = kSize - 1;
    double sr = 44100.0, phase = 0.0;
    BloomParams p;
    std::vector<float> bufL, bufR;
    int w = 0;
    unsigned int counter = 0;
    float fbL = 0.0f, fbR = 0.0f, phL = 0.0f, phR = 0.0f;
    std::array<float, 6> apL {}, apR {};
    float windRate = 1.0f, windDepth = 1.0f, windX = 0.0f, walkTarget = 0.0f;
    Smooth rateS, depthS, xS, walkS;
    Biquad mossL, mossR;
    juce::Random rng;
};
} // namespace org
