#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "G0GLookAndFeel.h"

/**
 * Main editor for the G0G synthesizer: sectioned panels, metal knobs, hover
 * definitions, preset browser, performance wheels and an envelope visualizer.
 */
class G0GAudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer {
public:
    G0GAudioProcessorEditor(G0GAudioProcessor&);
    ~G0GAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    // One column: knob + title label (carries the hover definition).
    struct Knob {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    // A drop-down choice (click type, drive mode).
    struct Choice {
        juce::ComboBox box;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    struct Section {
        juce::String title;
        Choice* choice = nullptr;          // optional selector above the knobs
        std::vector<Knob*> knobs;
        float weight = 1.0f;               // width relative to column count
        juce::Rectangle<float> bounds;
    };

    void timerCallback() override;
    void drawEnvelopeVisualizer(juce::Graphics& g, juce::Rectangle<int> area);

    void addKnob(Knob&, const juce::String& paramId, const juce::String& title,
                 const juce::String& tooltip);
    void addChoice(Choice&, const juce::String& paramId, const juce::StringArray& items,
                   const juce::String& tooltip);

    void refreshPresetList(const juce::String& selectName = {});
    void stepPreset(int delta);
    void promptSavePreset();

    G0GAudioProcessor& audioProcessor;
    G0GLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 350 };

    // Sub / glide / punch / amp envelope
    Knob morph, glideTime, glideCurve, pitchDepth, pitchDecay, hold, decay, decayCurve;
    // Click / drive
    Choice clickType, driveMode;
    Knob clickLevel, driveGain;
    // Filter / master
    Knob cutoff, filterEnvDepth, filterEnvDecay, masterVol;
    // Performance wheels
    Knob pitchWheel, modWheel;

    std::vector<Section> sections;

    juce::MidiKeyboardComponent keyboardComponent;

    // Preset browser
    juce::ComboBox presetBox;
    juce::TextButton prevPresetButton, nextPresetButton, savePresetButton;
    juce::Array<juce::File> userPresetFiles;

    juce::Rectangle<int> visualizerArea;   // actual pixels
    float uiScale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G0GAudioProcessorEditor)
};
