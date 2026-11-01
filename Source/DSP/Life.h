#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <vector>
#include "Biquad.h"

namespace org
{
// Transient detector: a fast envelope jumping well above a slow one marks a new hit.
class OnsetDetector
{
public:
    void prepare (double sampleRate)
    {
        auto c = [sampleRate] (double ms) { return (float) (1.0 - std::exp (-1.0 / (ms * 0.001 * sampleRate))); };
        fastA = c (0.3);
        fastR = c (12.0);
        slowA = c (25.0);
        slowR = c (150.0);
        holdoff = (int) (0.035 * sampleRate);
        fast = slow = 0.0f;
        counter = holdoff;
    }

    bool process (float x) noexcept
    {
        const float a = std::abs (x);
        fast += (a - fast) * (a > fast ? fastA : fastR);
        slow += (a - slow) * (a > slow ? slowA : slowR);
        if (counter < holdoff)
        {
            ++counter;
            return false;
        }
        if (fast > 0.002f && fast > slow * 2.0f)
        {
            counter = 0;
            return true;
        }
        return false;
    }

private:
    float fastA = 1.0f, fastR = 1.0f, slowA = 1.0f, slowR = 1.0f, fast = 0.0f, slow = 0.0f;
    int holdoff = 1000, counter = 0;
};

struct HitInfo
{
    float gainDb = 0.0f, tiltDb = 0.0f, pan = 0.0f, cents = 0.0f;
};

// LIFE: on every detected hit, roll a small variation in level, tone, place, pitch and
// timing. Pitch and timing come from two crossfaded delay taps: each hit gets a fresh tap
// with a new start delay (micro timing) that drifts at a constant rate (micro pitch).
class LifeEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        bufL.assign ((size_t) kSize, 0.0f);
        bufR.assign ((size_t) kSize, 0.0f);
        w = 0;
        for (auto& t : taps)
            t = { 1.0, 0.0 };
        active = 0;
        xf = 0.0f;
        xfStep = (float) (1.0 / (0.003 * sr));
        maxDelay = juce::jmin ((double) kSize - 4.0, 0.012 * sr);
        gainS.setTime (sr, 2.0);
        tiltS.setTime (sr, 2.0);
        panS.setTime (sr, 4.0);
        gainS.v = 1.0f;
        tiltS.v = panS.v = 0.0f;
        tiltL.reset();
        tiltR.reset();
        counter = 0;
        target = {};
    }

    HitInfo onHit (juce::Random& rng, float amount) noexcept
    {
        HitInfo h;
        h.gainDb = -rng.nextFloat() * 6.0f * amount;
        h.tiltDb = (rng.nextFloat() - 0.5f) * 9.0f * amount;
        h.pan = (rng.nextFloat() - 0.5f) * 0.4f * amount;
        h.cents = (rng.nextFloat() - 0.5f) * 36.0f * amount;
        target = h;

        active ^= 1;
        auto& t = taps[(size_t) active];
        t.delay = 1.0 + rng.nextFloat() * 0.007 * sr * amount;
        t.slope = 1.0 - std::pow (2.0, (double) h.cents / 1200.0);
        return h;
    }

    void process (float& l, float& r) noexcept
    {
        bufL[(size_t) w] = l;
        bufR[(size_t) w] = r;

        float yl = 0.0f, yr = 0.0f;
        for (int k = 0; k < 2; ++k)
        {
            auto& t = taps[(size_t) k];
            t.delay += t.slope;
            if (t.delay < 1.0 || t.delay > maxDelay)
            {
                t.delay = juce::jlimit (1.0, maxDelay, t.delay);
                t.slope = 0.0;
            }
            const float wk = k == 1 ? xf : 1.0f - xf;
            if (wk > 0.0f)
            {
                yl += wk * read (bufL, t.delay);
                yr += wk * read (bufR, t.delay);
            }
        }
        xf = active == 1 ? juce::jmin (1.0f, xf + xfStep) : juce::jmax (0.0f, xf - xfStep);
        w = (w + 1) & kMask;

        if ((counter++ & 31) == 0)
        {
            const float td = tiltS.v;
            tiltL.highShelf (sr, 4500.0, td);
            tiltR.b0 = tiltL.b0; tiltR.b1 = tiltL.b1; tiltR.b2 = tiltL.b2; tiltR.a1 = tiltL.a1; tiltR.a2 = tiltL.a2;
        }
        tiltS.next (target.tiltDb);
        const float g = gainS.next (juce::Decibels::decibelsToGain (target.gainDb));
        const float p = panS.next (target.pan);

        yl = tiltL.process (yl) * g * (1.0f - juce::jmax (0.0f, p));
        yr = tiltR.process (yr) * g * (1.0f + juce::jmin (0.0f, p));
        l = yl;
        r = yr;
    }

private:
    float read (const std::vector<float>& b, double d) const noexcept
    {
        double pos = (double) w - d;
        if (pos < 0.0)
            pos += (double) kSize;
        const int i = (int) pos;
        const float f = (float) (pos - (double) i);
        const float x0 = b[(size_t) (i & kMask)], x1 = b[(size_t) ((i + 1) & kMask)];
        return x0 + f * (x1 - x0);
    }

    struct Tap
    {
        double delay, slope;
    };

    static constexpr int kSize = 1 << 13, kMask = kSize - 1;
    double sr = 44100.0, maxDelay = 500.0;
    std::vector<float> bufL, bufR;
    int w = 0, active = 0;
    unsigned int counter = 0;
    Tap taps[2] { { 1.0, 0.0 }, { 1.0, 0.0 } };
    float xf = 0.0f, xfStep = 0.01f;
    Smooth gainS, tiltS, panS;
    Biquad tiltL, tiltR;
    HitInfo target;
};
} // namespace org
