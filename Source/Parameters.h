#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <functional>

namespace org
{
inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

//==============================================================================
inline constexpr int kRows = 5;
inline constexpr int kMaxSteps = 64;
inline constexpr int kMaxBands = 40;

// row order is also the processing order of the chain
inline const juce::StringArray kRowNames { "ENVELOPE", "STEREO", "PITCH", "VOCODER", "BLOOM" };
inline const juce::StringArray kRowSubs  { "SHAPE OF THE HIT", "FIELD & TONE", "LAYERED VOICES", "THROAT & BANDS", "LIVING MODULATION" };
inline const juce::StringArray kBarNames { "1 BAR", "2 BARS", "4 BARS" };
inline constexpr int kBarCounts[] { 1, 2, 4 };

inline const juce::StringArray kNoteNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

inline juce::String rowOnId (int r)  { return "on" + juce::String (r + 1); }
inline juce::String rowAmtId (int r) { return "amt" + juce::String (r + 1); }

//==============================================================================
inline juce::String fmtSigned (float v, int decimals)
{
    const auto s = decimals == 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, decimals);
    const float threshold = decimals == 0 ? 0.49f : 0.049f;
    return (v > threshold ? "+" : "") + s;
}

inline juce::String fmtHz (float v)
{
    if (v >= 1000.0f)
        return juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz";
    return juce::String (juce::roundToInt (v)) + " Hz";
}

inline juce::String fmtDb (float v)   { return fmtSigned (v, 1) + " dB"; }
inline juce::String fmtPct (float v)  { return juce::String (juce::roundToInt (v)) + "%"; }

inline juce::String fmtMs (float v)
{
    if (v >= 1000.0f)
        return juce::String (v / 1000.0f, 2) + " s";
    if (v < 10.0f)
        return juce::String (v, 1) + " ms";
    return juce::String (juce::roundToInt (v)) + " ms";
}

inline juce::String fmtPan (float v)
{
    const int r = juce::roundToInt (v);
    if (r == 0)
        return "C";
    return (r < 0 ? "L" : "R") + juce::String (std::abs (r));
}

inline juce::String fmtNote (float v)
{
    const int n = juce::roundToInt (v);
    return kNoteNames[n % 12] + juce::String (2 + n / 12);
}

//==============================================================================
inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    auto addF = [&l] (const juce::String& id, const juce::String& name, float lo, float hi, float def, float centre,
                      std::function<juce::String (float)> fmt, float interval)
    {
        juce::NormalisableRange<float> r (lo, hi, interval);
        if (centre > 0.0f)
            r.setSkewForCentre (centre);
        l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, r, def,
                   juce::AudioParameterFloatAttributes().withStringFromValueFunction ([fmt] (float v, int) { return fmt (v); })));
    };
    auto addB = [&l] (const juce::String& id, const juce::String& name, bool def)
    {
        l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
    };
    auto addC = [&l] (const juce::String& id, const juce::String& name, const juce::StringArray& items, int def)
    {
        l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def));
    };

    auto fSt   = [] (float v) { return fmtSigned (v, 0) + " st"; };
    auto fCt   = [] (float v) { return fmtSigned (v, 0) + " ct"; };
    auto fSc   = [] (float v) { return u8 ("\xc2\xb1") + juce::String (juce::roundToInt (v)) + " ct"; };
    auto fLfo  = [] (float v) { return juce::String (v, 2) + " Hz"; };
    auto fInt  = [] (float v) { return juce::String (juce::roundToInt (v)); };
    auto fCrv  = [] (float v) { return v < 0.0f ? "LOG " + juce::String (juce::roundToInt (-v)) : "EXP " + juce::String (juce::roundToInt (v)); };
    auto fFmt  = [] (float v) { return fmtSigned (v, 1); };

    // ---- master
    addB ("power", "Power", true);
    addF ("mix",    "Dry/Wet",   0.0f, 100.0f, 100.0f, 0.0f, fmtPct, 0.0f);
    addF ("output", "Output",  -24.0f,  12.0f,   0.0f, 0.0f, fmtDb,  0.0f);
    addF ("life",   "Life",      0.0f, 100.0f,  60.0f, 0.0f, fmtPct, 0.0f);
    addB ("static", "Static Compare", false);
    addC ("bars", "Length", kBarNames, 0);

    // ---- sequencer rows
    for (int r = 0; r < kRows; ++r)
    {
        const auto n = kRowNames[r].substring (0, 1) + kRowNames[r].substring (1).toLowerCase();
        addB (rowOnId (r), n + " On", true);
        addF (rowAmtId (r), n + " Amount", 0.0f, 100.0f, 100.0f, 0.0f, fmtPct, 0.0f);
    }

    // ---- 1 envelope
    addC ("envMode", "Env Mode", { "LONG", "SHORT" }, 1);
    addF ("envA",     "Attack",   0.1f,  500.0f,  8.0f, 40.0f,  fmtMs,  0.0f);
    addF ("envD",     "Decay",    5.0f, 1500.0f, 220.0f, 200.0f, fmtMs,  0.0f);
    addF ("envS",     "Sustain",  0.0f,  100.0f,  45.0f, 0.0f,   fmtPct, 0.0f);
    addF ("envR",     "Release",  5.0f, 2000.0f, 300.0f, 250.0f, fmtMs,  0.0f);
    addF ("envCurve", "Curve", -100.0f,  100.0f,  20.0f, 0.0f,   fCrv,   0.0f);
    addF ("envSnap",  "Snap",     5.0f,  400.0f,  70.0f, 60.0f,  fmtMs,  0.0f);
    addF ("envDrift", "Drift",    0.0f,  100.0f,  35.0f, 0.0f,   fmtPct, 0.0f);

    // ---- 2 stereo
    addC ("stMode", "Stereo Mode", { "SIDES", "PAN", "MONO" }, 1);
    addF ("stLow",    "Low",      -12.0f, 12.0f,   0.0f, 0.0f,  fmtDb,  0.0f);
    addF ("stMid",    "Mid",      -12.0f, 12.0f,   0.0f, 0.0f,  fmtDb,  0.0f);
    addF ("stHigh",   "High",     -12.0f, 12.0f,   1.5f, 0.0f,  fmtDb,  0.0f);
    addF ("stMonoHz", "Mono Below", 20.0f, 800.0f, 120.0f, 150.0f, fmtHz, 0.0f);
    addF ("stWidth",  "Width",      0.0f, 200.0f, 120.0f, 0.0f, fmtPct, 0.0f);
    addF ("stPan",    "Pan",     -100.0f, 100.0f,   0.0f, 0.0f, fmtPan, 0.0f);
    addF ("stHaas",   "Haas",       0.0f,  30.0f,   6.0f, 0.0f, fmtMs,  0.0f);
    addF ("stWander", "Wander",     0.0f, 100.0f,  50.0f, 0.0f, fmtPct, 0.0f);

    // ---- 3 pitch (three voices)
    const float semis[] { 12.0f, -12.0f, 7.0f };
    const float levels[] { 55.0f, 40.0f, 35.0f };
    for (int v = 0; v < 3; ++v)
    {
        const juce::String p ("p" + juce::String (v + 1)), n ("Voice " + juce::String (v + 1) + " ");
        addB (p + "On", n + "On", v == 0);
        addF (p + "Semi", n + "Semi", -24.0f, 24.0f, semis[v], 0.0f, fSt, 1.0f);
        addF (p + "Fine", n + "Fine", -50.0f, 50.0f, 0.0f, 0.0f, fCt, 0.0f);
        addF (p + "Lvl",  n + "Level", 0.0f, 100.0f, levels[v], 0.0f, fmtPct, 0.0f);
    }
    addF ("piScatter", "Scatter", 0.0f,  40.0f, 10.0f, 0.0f, fSc,    0.0f);
    addF ("piSpread",  "Spread",  0.0f, 100.0f, 50.0f, 0.0f, fmtPct, 0.0f);
    addF ("piGlide",   "Glide",   0.0f, 200.0f, 20.0f, 0.0f, fmtMs,  0.0f);
    addF ("piWet",     "Pitch Dry/Wet", 0.0f, 100.0f, 50.0f, 0.0f, fmtPct, 0.0f);

    // ---- 4 vocoder (after Ableton's Vocoder)
    addC ("voCarrier", "Carrier", { "NOISE", "TONE", "MODULATOR", "PITCH TRACK" }, 0);
    addF ("voBands",    "Bands",       4.0f,    40.0f,    20.0f, 0.0f,    fInt,   1.0f);
    addF ("voLow",      "Range Low",  20.0f,  2000.0f,   200.0f, 200.0f,  fmtHz,  0.0f);
    addF ("voHigh",     "Range High", 2000.0f, 18000.0f, 12000.0f, 6000.0f, fmtHz, 0.0f);
    addF ("voBw",       "BW",         10.0f,   200.0f,   100.0f, 0.0f,    fmtPct, 0.0f);
    addF ("voFormant",  "Formant",   -12.0f,    12.0f,     0.0f, 0.0f,    fFmt,   0.0f);
    addF ("voTone",     "Tone",        0.0f,    35.0f,    21.0f, 0.0f,    fmtNote, 1.0f);
    addF ("voAttack",   "Vocoder Attack",  0.1f, 100.0f,  3.0f, 10.0f,  fmtMs,  0.0f);
    addF ("voRelease",  "Vocoder Release", 1.0f, 3000.0f, 120.0f, 150.0f, fmtMs, 0.0f);
    addF ("voGate",     "Gate",      -80.0f,     0.0f,   -80.0f, 0.0f,    fmtDb,  0.0f);
    addF ("voUnvoiced", "Unvoiced",    0.0f,   100.0f,    30.0f, 0.0f,    fmtPct, 0.0f);
    addF ("voSens",     "Sensitivity", 0.0f,   100.0f,    60.0f, 0.0f,    fmtPct, 0.0f);
    addF ("voDepth",    "Depth",       0.0f,   100.0f,   100.0f, 0.0f,    fmtPct, 0.0f);
    addB ("voEnhance", "Enhance", false);
    addC ("voMode", "Vocoder Mode", { "PRECISE", "RETRO" }, 0);
    addC ("voChan", "Channels", { "MONO", "STEREO", "L/R" }, 1);
    addF ("voWet",      "Vocoder Dry/Wet", 0.0f, 100.0f, 60.0f, 0.0f, fmtPct, 0.0f);

    // ---- 5 bloom
    addF ("blX",     "Morph",    0.0f, 100.0f, 30.0f, 0.0f,  fmtPct, 0.0f);
    addF ("blY",     "Depth",    0.0f, 100.0f, 55.0f, 0.0f,  fmtPct, 0.0f);
    addF ("blRate",  "Rate",     0.05f,  8.0f,  0.6f, 0.63f, fLfo,   0.0f);
    addF ("blFb",    "Feedback", 0.0f,  95.0f, 35.0f, 0.0f,  fmtPct, 0.0f);
    addF ("blWind",  "Wind",     0.0f, 100.0f, 50.0f, 0.0f,  fmtPct, 0.0f);
    addF ("blMoss",  "Moss",     0.0f, 100.0f, 35.0f, 0.0f,  fmtPct, 0.0f);
    addF ("blGrain", "Grain",    0.0f, 100.0f, 20.0f, 0.0f,  fmtPct, 0.0f);
    addF ("blWet",   "Bloom Dry/Wet", 0.0f, 100.0f, 60.0f, 0.0f, fmtPct, 0.0f);

    return l;
}
} // namespace org
