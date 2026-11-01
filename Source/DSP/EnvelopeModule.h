#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace org
{
struct EnvParams
{
    bool shortMode = true;
    float a = 8.0f, d = 220.0f, s = 0.45f, r = 300.0f; // ms, ms, 0..1, ms
    float curve = 0.2f;                                // -1 (log) .. +1 (exp)
    float snap = 70.0f;                                // ms, SHORT decay length
    float drift = 0.35f;                               // per-hit length variation
};

// Re-shapes every hit. LONG = ADSR (hold = one sixteenth), SHORT = attack 0, sustain 0,
// one exponential-style decay. The gain only applies as far as the step's wet amount.
class EnvelopeModule
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        stage = Stage::bypass;
        e = 1.0f;
        t = 0.0;
    }

    void setParams (const EnvParams& np) noexcept
    {
        p = np;
        k = 1.5f * std::pow (4.0f, juce::jlimit (-1.0f, 1.0f, p.curve));
    }

    void onHit (juce::Random& rng, double stepSamples) noexcept
    {
        const float m = 1.0f + (rng.nextFloat() - 0.5f) * 1.2f * p.drift;
        startLevel = e;
        t = 0.0;
        hold = stepSamples;
        if (p.shortMode)
        {
            lenA = 0.0003 * sr;
            lenD = juce::jmax (1.0, (double) (p.snap * m) * 0.001 * sr);
            stage = Stage::attack;
        }
        else
        {
            lenA = juce::jmax (1.0, (double) p.a * 0.001 * sr);
            lenD = juce::jmax (1.0, (double) (p.d * m) * 0.001 * sr);
            lenR = juce::jmax (1.0, (double) (p.r * m) * 0.001 * sr);
            stage = Stage::attack;
        }
    }

    void process (float& l, float& r, float wet) noexcept
    {
        advance();
        const float g = 1.0f + (e - 1.0f) * wet;
        l *= g;
        r *= g;
    }

    float level() const noexcept { return e; }

private:
    enum class Stage { bypass, attack, decay, hold, release, idle };

    void advance() noexcept
    {
        t += 1.0;
        switch (stage)
        {
            case Stage::bypass:
                e = 1.0f;
                break;

            case Stage::attack:
            {
                const float x = (float) juce::jmin (1.0, t / lenA);
                e = startLevel + (1.0f - startLevel) * x;
                if (t >= lenA) { stage = Stage::decay; t = 0.0; }
                break;
            }

            case Stage::decay:
            {
                const float x = (float) juce::jmin (1.0, t / lenD);
                const float shape = std::pow (1.0f - x, k);
                if (p.shortMode)
                {
                    e = shape;
                    if (t >= lenD) { stage = Stage::idle; e = 0.0f; }
                }
                else
                {
                    e = p.s + (1.0f - p.s) * shape;
                    if (t >= lenD) { stage = Stage::hold; t = 0.0; }
                }
                break;
            }

            case Stage::hold:
                e = p.s;
                if (t + lenA + lenD >= hold) { stage = Stage::release; t = 0.0; relLevel = e; }
                break;

            case Stage::release:
            {
                const float x = (float) juce::jmin (1.0, t / lenR);
                e = relLevel * std::pow (1.0f - x, k);
                if (t >= lenR) { stage = Stage::idle; e = 0.0f; }
                break;
            }

            case Stage::idle:
                e = 0.0f;
                break;
        }
    }

    double sr = 44100.0, t = 0.0, lenA = 1.0, lenD = 1.0, lenR = 1.0, hold = 1.0;
    EnvParams p;
    Stage stage = Stage::bypass;
    float e = 1.0f, k = 1.5f, startLevel = 0.0f, relLevel = 0.0f;
};
} // namespace org
