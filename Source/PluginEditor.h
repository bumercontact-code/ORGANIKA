#pragma once

#include "PluginProcessor.h"
#include "UI/Panels.h"

// The whole plugin face is laid out on a fixed 1180 x 720 canvas and scaled to the window.
class Canvas : public juce::Component, private juce::Timer
{
public:
    explicit Canvas (OrganikaAudioProcessor& p);
    ~Canvas() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    static constexpr int kWidth = 1180, kHeight = 720;

private:
    void timerCallback() override;
    void syncPresetText();
    void loadFactory (int index);
    void showLibrary (bool show);
    void showModule (int row);
    void showSequencer();

    OrganikaAudioProcessor& proc;
    juce::SharedResourcePointer<theme::Resources> resources;
    ui::KnobLookAndFeel lnf;

    ui::PresetBar presetBar;
    ui::FlatButton saveButton { "SAVE" };
    ui::Segments bars;
    ui::Knob lifeKnob, mixKnob, outKnob;
    ui::Chip staticChip;
    ui::SequencerView seqView;
    ui::ModuleView moduleView;
    ui::Fingerprint fingerprint;
    ui::OutMeter meter;
    ui::PresetLibrary library;
    ui::SaveBox saveBox;
    ui::Toast toast;

    juce::String lastPresetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Canvas)
};

class OrganikaEditor : public juce::AudioProcessorEditor
{
public:
    explicit OrganikaEditor (OrganikaAudioProcessor& p);
    void paint (juce::Graphics& g) override { g.fillAll (theme::ground); }
    void resized() override;

private:
    Canvas canvas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrganikaEditor)
};
