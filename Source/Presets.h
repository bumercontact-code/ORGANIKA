#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include "Sequence.h"

namespace org
{
struct FactoryPreset
{
    const char* group;
    const char* name;
};

inline const juce::StringArray kPresetGroups { "WATER", "SOIL", "WOOD", "LEAF", "NIGHT" };

const std::vector<FactoryPreset>& factoryPresets();

// Factory presets are grown from a fixed seed per slot, flavoured by their group
// (Water = width and motion, Soil = short envelopes, Wood = pitch, Leaf = bloom,
// Night = vocoder). User presets are XML files holding parameters and steps.
class PresetManager
{
public:
    PresetManager (juce::AudioProcessorValueTreeState& s, Sequence& q) : apvts (s), sequence (q) {}

    void loadFactory (int index);
    void saveUser (const juce::String& name);
    bool loadUser (const juce::String& name);
    juce::StringArray userPresetNames() const;
    static juce::File userDirectory();

    juce::String getName() const;
    juce::String getCategory() const;
    void setName (const juce::String& category, const juce::String& name);
    int getFactoryIndex() const { return factoryIndex; }

private:
    void set (const juce::String& id, float plainValue);
    void resetToDefaults();

    juce::AudioProcessorValueTreeState& apvts;
    Sequence& sequence;
    int factoryIndex = -1;
};
} // namespace org
