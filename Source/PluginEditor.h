#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

/**
 * Custom LookAndFeel for G0G. Features premium flat dials with glowing trails and custom colors.
 */
class G0GLookAndFeel : public juce::LookAndFeel_V4 {
public:
    G0GLookAndFeel() {
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xFF232A35));
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFF00F0FF)); // Neon Aqua
        setColour(juce::Label::textColourId, juce::Colours::white);
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF1B222C));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF00F0FF));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF1B222C));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF2C3848));
        setColour(juce::ComboBox::textColourId, juce::Colours::white);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override {
        auto outline = slider.findColour(juce::Slider::rotarySliderOutlineColourId);
        auto fill    = slider.findColour(juce::Slider::rotarySliderFillColourId);

        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
        auto radius = std::min(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto toX = bounds.getCentreX();
        auto toY = bounds.getCentreY();
        auto lineW = 3.5f;
        auto arcRadius = radius - lineW * 1.5f;

        // Background Track Arc
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(toX, toY, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(outline);
        g.strokePath(backgroundArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Active Value Arc
        if (slider.isEnabled()) {
            juce::Path valueArc;
            auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
            valueArc.addCentredArc(toX, toY, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
            g.setColour(fill);
            g.strokePath(valueArc, juce::PathStrokeType(lineW + 1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            
            // Glowing pointer dot
            float dotX = toX + arcRadius * std::sin(angle);
            float dotY = toY - arcRadius * std::cos(angle);
            g.setColour(juce::Colours::white);
            g.fillEllipse(dotX - 2.5f, dotY - 2.5f, 5.0f, 5.0f);
        }
    }
};

/**
 * Main Audio Processor Editor for the G0G synthesizer plugin.
 */
class G0GAudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer {
public:
    G0GAudioProcessorEditor(G0GAudioProcessor&);
    ~G0GAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawEnvelopeVisualizer(juce::Graphics& g, juce::Rectangle<int> area);

    G0GAudioProcessor& audioProcessor;
    G0GLookAndFeel customLookAndFeel;

    // --- GUI Controls ---
    // Envelope & Glide
    juce::Slider glideTimeSlider;
    juce::Slider glideCurveSlider;
    juce::Slider holdSlider;
    juce::Slider decaySlider;
    juce::Slider decayCurveSlider;
    juce::Slider pitchDepthSlider;
    juce::Slider pitchDecaySlider;

    // Sub Waveform
    juce::Slider oscMorphSlider;

    // Transient Click
    juce::ComboBox clickTypeCombo;
    juce::Slider clickLevelSlider;

    // Saturation
    juce::ComboBox driveModeCombo;
    juce::Slider driveGainSlider;

    // LPF Filter & Master Volume
    juce::Slider cutoffSlider;
    juce::Slider filterEnvDecaySlider;
    juce::Slider filterEnvDepthSlider;
    juce::Slider masterVolSlider;

    // Visual frames (Group boxes)
    juce::GroupComponent envelopeGroup;
    juce::GroupComponent clickGroup;
    juce::GroupComponent saturationGroup;
    juce::GroupComponent filterGroup;

    // On-screen Keyboard
    juce::MidiKeyboardComponent keyboardComponent;

    // --- APVTS Attachments ---
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttachment> glideTimeAttach;
    std::unique_ptr<SliderAttachment> glideCurveAttach;
    std::unique_ptr<SliderAttachment> holdAttach;
    std::unique_ptr<SliderAttachment> decayAttach;
    std::unique_ptr<SliderAttachment> decayCurveAttach;
    std::unique_ptr<SliderAttachment> pitchDepthAttach;
    std::unique_ptr<SliderAttachment> pitchDecayAttach;

    std::unique_ptr<SliderAttachment> oscMorphAttach;

    std::unique_ptr<ComboBoxAttachment> clickTypeAttach;
    std::unique_ptr<SliderAttachment> clickLevelAttach;

    std::unique_ptr<ComboBoxAttachment> driveModeAttach;
    std::unique_ptr<SliderAttachment> driveGainAttach;

    std::unique_ptr<SliderAttachment> cutoffAttach;
    std::unique_ptr<SliderAttachment> filterEnvDecayAttach;
    std::unique_ptr<SliderAttachment> filterEnvDepthAttach;
    std::unique_ptr<SliderAttachment> masterVolAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G0GAudioProcessorEditor)
};
