#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "Parameters.h"
#include "Sequence.h"
#include "Presets.h"
#include "DSP/Life.h"
#include "DSP/EnvelopeModule.h"
#include "DSP/StereoModule.h"
#include "DSP/PitchModule.h"
#include "DSP/VocoderModule.h"
#include "DSP/BloomModule.h"

class OrganikaAudioProcessor : public juce::AudioProcessor
{
public:
    OrganikaAudioProcessor();
    ~OrganikaAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    org::Sequence sequence;
    org::PresetManager presets;
    org::VocoderModule vocoder;

    // ---- for the editor
    std::atomic<int> currentStep { -1 };
    std::atomic<float> outLevel { 0.0f };

    struct Hit
    {
        std::atomic<float> gain { 0.0f }, tilt { 0.0f }, pan { 0.0f };
        std::atomic<int> row { -1 };
    };
    static constexpr int kHitHistory = 28;
    std::array<Hit, kHitHistory> hits;
    std::atomic<int> hitCount { 0 };

    static constexpr int kScope = 512;
    std::array<std::atomic<float>, kScope> scopeL {}, scopeR {};
    std::atomic<int> scopePos { 0 };

private:
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    float param (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    void readModuleParams();
    void registerHit (const org::HitInfo& h, int row);

    org::OnsetDetector onset;
    org::LifeEngine life;
    org::EnvelopeModule envelope;
    org::StereoModule stereo;
    org::PitchModule pitch;
    org::BloomModule bloom;

    std::array<org::Smooth, org::kRows> wetS;
    juce::Random rng;
    double internalPpq = 0.0;
    float lastOut = 1.0f, lastMix = 1.0f;
    int scopeDecim = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrganikaAudioProcessor)
};
