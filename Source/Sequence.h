#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <atomic>
#include "Parameters.h"

namespace org
{
// The step pattern: 5 rows x 64 sixteenths, each with an on flag and a mix (0..1).
// Steps are not host parameters; they live here (lock-free for the audio thread)
// and are written into the plugin state / presets as a "SEQ" child tree.
class Sequence
{
public:
    Sequence() { clear(); }

    void clear()
    {
        for (int r = 0; r < kRows; ++r)
            for (int i = 0; i < kMaxSteps; ++i)
                set (r, i, false, 0.75f);
    }

    bool isOn (int r, int i) const noexcept   { return on[idx (r, i)].load (std::memory_order_relaxed); }
    float getMix (int r, int i) const noexcept { return mix[idx (r, i)].load (std::memory_order_relaxed); }

    void set (int r, int i, bool isOnNow, float m) noexcept
    {
        on[idx (r, i)].store (isOnNow, std::memory_order_relaxed);
        mix[idx (r, i)].store (juce::jlimit (0.0f, 1.0f, m), std::memory_order_relaxed);
        version.fetch_add (1, std::memory_order_relaxed);
    }

    void setOn (int r, int i, bool b) noexcept   { set (r, i, b, getMix (r, i)); }
    void setMix (int r, int i, float m) noexcept { set (r, i, isOn (r, i), m); }

    int getVersion() const noexcept { return version.load (std::memory_order_relaxed); }

    juce::ValueTree toValueTree() const
    {
        juce::ValueTree t ("SEQ");
        for (int r = 0; r < kRows; ++r)
        {
            juce::StringArray tokens;
            for (int i = 0; i < kMaxSteps; ++i)
                tokens.add (juce::String (isOn (r, i) ? 1 : 0) + ":" + juce::String (getMix (r, i), 2));
            t.setProperty ("r" + juce::String (r), tokens.joinIntoString (" "), nullptr);
        }
        return t;
    }

    void fromValueTree (const juce::ValueTree& t)
    {
        if (! t.hasType ("SEQ"))
            return;
        for (int r = 0; r < kRows; ++r)
        {
            const auto tokens = juce::StringArray::fromTokens (t.getProperty ("r" + juce::String (r)).toString(), " ", "");
            for (int i = 0; i < kMaxSteps; ++i)
            {
                if (i < tokens.size())
                    set (r, i, tokens[i].upToFirstOccurrenceOf (":", false, false).getIntValue() != 0,
                         tokens[i].fromFirstOccurrenceOf (":", false, false).getFloatValue());
                else
                    set (r, i, false, 0.75f);
            }
        }
    }

private:
    static size_t idx (int r, int i) noexcept { return (size_t) (juce::jlimit (0, kRows - 1, r) * kMaxSteps + juce::jlimit (0, kMaxSteps - 1, i)); }

    std::array<std::atomic<bool>, kRows * kMaxSteps> on;
    std::array<std::atomic<float>, kRows * kMaxSteps> mix;
    std::atomic<int> version { 0 };
};
} // namespace org
