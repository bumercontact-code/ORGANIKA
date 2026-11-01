#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include "Biquad.h"
#include "../Parameters.h"

namespace org
{
struct VocParams
{
    int carrier = 0;          // 0 noise, 1 tone, 2 modulator, 3 pitch track
    int bands = 20;
    float low = 200.0f, high = 12000.0f;
    float bw = 1.0f;          // 0.1..2 (1 = bands just touch)
    float formant = 0.0f;     // semitones
    float toneHz = 220.0f;
    float attackMs = 3.0f, releaseMs = 120.0f;
    float gateDb = -80.0f;
    float unvoiced = 0.3f, sens = 0.6f, depth = 1.0f;
    bool enhance = false, retro = false;
    int chan = 1;             // 0 mono, 1 stereo, 2 L/R (left = modulator, right = carrier)
    float wet = 0.6f;

    bool sameBank (const VocParams& o) const noexcept
    {
        return bands == o.bands && low == o.low && high == o.high && bw == o.bw && formant == o.formant && retro == o.retro;
    }
};

// Channel vocoder after Ableton's: the incoming hit is the modulator, a band bank imposes
// its spectrum on the chosen carrier. Output level follows the input automatically.
class VocoderModule
{
public:
    std::array<std::atomic<float>, kMaxBands> bandLevels {};

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& c : ch)
            c.reset();
        bankDirty = true;
        updateBank();
        inRms = outRms = 0.0f;
        gain = 1.0f;
        rmsCoef = (float) (1.0 - std::exp (-1.0 / (0.25 * sr)));
        gainS.setTime (sr, 60.0);
        gainS.v = 1.0f;
    }

    void setParams (const VocParams& np) noexcept
    {
        bankDirty = bankDirty || ! np.sameBank (p);
        p = np;
        atk = (float) (1.0 - std::exp (-1.0 / (juce::jmax (0.05, (double) p.attackMs) * 0.001 * sr)));
        rel = (float) (1.0 - std::exp (-1.0 / (juce::jmax (0.5, (double) p.releaseMs) * 0.001 * sr)));
        gateLin = p.gateDb <= -79.5f ? 0.0f : juce::Decibels::decibelsToGain (p.gateDb);
        uvThr = 0.002f + 0.12f * (1.0f - p.sens) * (1.0f - p.sens);
        if (bankDirty)
            updateBank();
    }

    void process (float& l, float& r, float wet) noexcept
    {
        if (wet < 1.0e-4f)
        {
            idle = true;
            return;
        }
        idle = false;

        float yl, yr;
        if (p.chan == 0)
        {
            const float m = (l + r) * 0.5f;
            yl = yr = ch[0].process (*this, m, m);
        }
        else if (p.chan == 1)
        {
            yl = ch[0].process (*this, l, l);
            yr = ch[1].process (*this, r, r);
        }
        else
        {
            yl = yr = ch[0].process (*this, l, r);
        }

        // automatic make-up: keep the vocoded level near the input level
        const float in2 = (l * l + r * r) * 0.5f, out2 = (yl * yl + yr * yr) * 0.5f;
        inRms += (in2 - inRms) * rmsCoef;
        outRms += (out2 - outRms) * rmsCoef;
        if (inRms > 1.0e-9f)
            gain = juce::jlimit (0.05f, 24.0f, std::sqrt (inRms / juce::jmax (1.0e-10f, outRms)));
        const float g = gainS.next (gain);

        const float mixAmt = wet * p.wet;
        l += (yl * g - l) * mixAmt;
        r += (yr * g - r) * mixAmt;

        if ((++meterCount & 255) == 0)
            for (int b = 0; b < kMaxBands; ++b)
                bandLevels[(size_t) b].store (b < p.bands ? juce::jmax (ch[0].env[(size_t) b], ch[1].env[(size_t) b]) : 0.0f,
                                              std::memory_order_relaxed);
    }

    bool isIdle() const noexcept { return idle; }

private:
    struct Channel
    {
        std::array<Biquad, kMaxBands> ana, syn;
        std::array<float, kMaxBands> env {}, held {}, cenv {};
        Biquad hf, trackLp;
        float hfEnv = 0.0f, phase = 0.0f, trackHz = 220.0f, lastLp = 0.0f, meanEnv = 0.0f;
        int sinceCross = 0, retroCount = 0;

        void reset()
        {
            for (auto& b : ana) b.reset();
            for (auto& b : syn) b.reset();
            env.fill (0.0f);
            held.fill (0.0f);
            cenv.fill (0.0f);
            hf.reset();
            trackLp.reset();
            hfEnv = phase = lastLp = meanEnv = 0.0f;
            sinceCross = retroCount = 0;
        }

        float process (VocoderModule& v, float mod, float ext) noexcept
        {
            const auto& p = v.p;
            const float noise = v.rng.nextFloat() * 2.0f - 1.0f;

            // ---- carrier
            float c = 0.0f;
            if (p.chan == 2)
                c = ext;
            else
            {
                switch (p.carrier)
                {
                    case 0: c = noise; break;
                    case 1: c = saw (p.toneHz, v.sr); break;
                    case 2: c = mod; break;
                    default:
                    {
                        const float lp = trackLp.process (mod);
                        ++sinceCross;
                        if (lastLp <= 0.0f && lp > 0.0f)
                        {
                            const float hz = (float) v.sr / (float) juce::jmax (1, sinceCross);
                            if (hz > 50.0f && hz < 1200.0f)
                                trackHz += (hz - trackHz) * 0.3f;
                            sinceCross = 0;
                        }
                        lastLp = lp;
                        c = saw (trackHz, v.sr);
                        break;
                    }
                }
            }

            // ---- unvoiced: noise joins the carrier when the hit is bright
            const float h = std::abs (hf.process (mod));
            hfEnv += (h - hfEnv) * (h > hfEnv ? v.atk : v.rel);
            if (p.carrier != 0 && p.unvoiced > 0.0f)
                c += noise * p.unvoiced * juce::jmin (1.0f, hfEnv / v.uvThr);

            // ---- band bank
            const bool updateHeld = ! p.retro || (++retroCount & 63) == 0;
            float y = 0.0f, sum = 0.0f;
            for (int b = 0; b < p.bands; ++b)
            {
                const auto i = (size_t) b;
                const float a = std::abs (ana[i].process (mod));
                env[i] += (a - env[i]) * (a > env[i] ? v.atk : v.rel);
                if (updateHeld)
                    held[i] = env[i];
                sum += held[i];

                float e = held[i] < v.gateLin ? 0.0f : held[i];
                e = p.depth * e + (1.0f - p.depth) * meanEnv;

                float s = syn[i].process (c);
                if (p.enhance)
                {
                    const float as = std::abs (s);
                    cenv[i] += (as - cenv[i]) * (as > cenv[i] ? v.atk : v.rel);
                    s *= 0.3f / (cenv[i] + 0.003f);
                }
                y += s * e;
            }
            meanEnv = sum / (float) juce::jmax (1, p.bands);
            return y;
        }

        float saw (float hz, double sr) noexcept
        {
            phase += hz / (float) sr;
            if (phase >= 1.0f)
                phase -= 1.0f;
            return 2.0f * phase - 1.0f;
        }
    };

    void updateBank()
    {
        const int n = juce::jlimit (4, kMaxBands, p.bands);
        const double lo = juce::jmax (20.0, (double) p.low), hi = juce::jmax (lo * 1.5, (double) p.high);
        const double ratio = std::pow (hi / lo, 1.0 / n);
        double q = std::sqrt (ratio) / (ratio - 1.0) / juce::jmax (0.1, (double) p.bw);
        if (p.retro)
            q *= 2.5;
        const double fShift = std::pow (2.0, p.formant / 12.0);
        for (auto& c : ch)
        {
            for (int b = 0; b < n; ++b)
            {
                const double f = lo * std::pow (ratio, b + 0.5);
                c.ana[(size_t) b].bandPass (sr, f, q);
                c.syn[(size_t) b].bandPass (sr, f * fShift, q);
            }
            c.hf.highPass (sr, 5000.0, 0.707);
            c.trackLp.lowPass (sr, 900.0, 0.707);
        }
        bankDirty = false;
    }

    double sr = 44100.0;
    VocParams p;
    std::array<Channel, 2> ch;
    juce::Random rng;
    float atk = 0.1f, rel = 0.01f, gateLin = 0.0f, uvThr = 0.02f;
    float inRms = 0.0f, outRms = 0.0f, gain = 1.0f, rmsCoef = 0.001f;
    Smooth gainS;
    unsigned int meterCount = 0;
    bool bankDirty = true, idle = true;
};
} // namespace org
