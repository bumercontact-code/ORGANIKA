#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <vector>
#include "Biquad.h"

namespace org
{
// Two-head crossfading delay-line pitch shifter (classic "tape splice" design).
class GrainShifter
{
public:
    void prepare()
    {
        buf.assign ((size_t) kSize, 0.0f);
        w = 0;
        phase = 0.0f;
    }

    float process (float x, float ratio, float window) noexcept
    {
        buf[(size_t) w] = x;
        phase += (1.0f - ratio) / window;
        phase -= std::floor (phase);
        float p2 = phase + 0.5f;
        if (p2 >= 1.0f)
            p2 -= 1.0f;
        const float s = std::sin (juce::MathConstants<float>::pi * phase);
        const float g1 = s * s;
        const float y = g1 * read (phase * window + 2.0f) + (1.0f - g1) * read (p2 * window + 2.0f);
        w = (w + 1) & kMask;
        return y;
    }

private:
    float read (float d) const noexcept
    {
        float pos = (float) w - d;
        if (pos < 0.0f)
            pos += (float) kSize;
        const int i = (int) pos;
        const float f = pos - (float) i;
        const float x0 = buf[(size_t) (i & kMask)], x1 = buf[(size_t) ((i + 1) & kMask)];
        return x0 + f * (x1 - x0);
    }

    static constexpr int kSize = 1 << 15, kMask = kSize - 1;
    std::vector<float> buf;
    int w = 0;
    float phase = 0.0f;
};

struct PitchVoice
{
    bool on = false;
    float semi = 0.0f, fine = 0.0f, level = 0.5f;
};

struct PitchParams
{
    std::array<PitchVoice, 3> voices;
    float scatter = 10.0f;   // cents, re-rolled per hit
    float spread = 0.5f;     // 0..1
    float glideMs = 20.0f;
    float wet = 0.5f;        // 0..1
};

// Three pitched copies layered on top of the original hit.
class PitchModule
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& v : st)
        {
            v.l.prepare();
            v.r.prepare();
            v.semis = v.target = 0.0f;
            v.scatter = 0.0f;
        }
        window = (float) (0.04 * sr);
    }

    void setParams (const PitchParams& np) noexcept
    {
        p = np;
        glideCoef = p.glideMs < 0.5f ? 1.0f : 1.0f - std::exp (-1.0f / (p.glideMs * 0.001f * (float) sr));
    }

    void onHit (juce::Random& rng) noexcept
    {
        for (auto& v : st)
            v.scatter = (rng.nextFloat() * 2.0f - 1.0f) * p.scatter;
    }

    void process (float& l, float& r, float wet) noexcept
    {
        float sl = 0.0f, sr2 = 0.0f;
        for (size_t i = 0; i < 3; ++i)
        {
            const auto& pv = p.voices[i];
            auto& v = st[i];
            if (! pv.on)
                continue;
            v.target = pv.semi + (pv.fine + v.scatter) * 0.01f;
            v.semis += (v.target - v.semis) * glideCoef;
            const float ratio = std::exp2 (v.semis / 12.0f);
            const float yl = v.l.process (l, ratio, window);
            const float yr = v.r.process (r, ratio, window);

            // voice I sits in the centre, II to the left, III to the right
            static constexpr float side[] { 0.0f, -1.0f, 1.0f };
            const float pos = side[i] * p.spread;
            const float a = (pos + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            const float gl = juce::jmin (1.0f, std::cos (a) * juce::MathConstants<float>::sqrt2);
            const float gr = juce::jmin (1.0f, std::sin (a) * juce::MathConstants<float>::sqrt2);
            sl += yl * pv.level * gl;
            sr2 += yr * pv.level * gr;
        }
        const float g = wet * p.wet * 0.9f;
        l += sl * g;
        r += sr2 * g;
    }

private:
    struct VoiceState
    {
        GrainShifter l, r;
        float semis = 0.0f, target = 0.0f, scatter = 0.0f;
    };

    double sr = 44100.0;
    PitchParams p;
    std::array<VoiceState, 3> st;
    float window = 1764.0f, glideCoef = 1.0f;
};
} // namespace org
