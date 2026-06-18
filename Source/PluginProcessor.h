#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "GlideUnit.h"
#include "SubEngine.h"
#include "ClickEngine.h"
#include "SaturationCore.h"

/**
 * Main Audio Processor for G0G monophonic synthesizer plugin.
 */
class G0GAudioProcessor : public juce::AudioProcessor {
public:
    G0GAudioProcessor();
    ~G0GAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Real-time accessors for the Editor
    bool isVoiceActive() const { return subEngine.isActive(); }
    float getHoldProgress() const { return subEngine.getHoldProgress(); }
    float getDecayProgress() const { return subEngine.getDecayProgress(); }
    float getFilterEnvProgress() const { return subEngine.getFilterEnvProgress(); }
    int getActiveMidiNote() const { return glideUnit.getActiveMidiNote(); }
    float getGlideFrequency() const { return glideUnit.getCurrentFrequency(); }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // DSP Core Elements
    GlideUnit glideUnit;
    SubEngine subEngine;
    ClickEngine clickEngine;
    SaturationCore saturationCore;

    // Post-Drive SVF Low-Pass Filter
    juce::dsp::StateVariableTPTFilter<float> postDriveFilter;

    // Parameter smoothing values
    juce::LinearSmoothedValue<float> smoothedMasterVol;
    juce::LinearSmoothedValue<float> smoothedCutoff;
    juce::LinearSmoothedValue<float> smoothedDrive;
    juce::LinearSmoothedValue<float> smoothedClickLevel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G0GAudioProcessor)
};
