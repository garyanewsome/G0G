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

    // Performance controls, written by MIDI on the audio thread and by the
    // on-screen wheels on the message thread.
    std::atomic<float> pitchBend { 0.0f };   // -1..1 (+/-2 semitones)
    std::atomic<float> modWheel  { 0.0f };   // 0..1  (opens the filter)

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    static constexpr int kChunk = 64;   // samples per control-update chunk

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Cached parameter handles so the audio thread never does string lookups.
    struct ParamPtrs {
        std::atomic<float>* glideTime = nullptr;
        std::atomic<float>* glideCurve = nullptr;
        std::atomic<float>* hold = nullptr;
        std::atomic<float>* decay = nullptr;
        std::atomic<float>* decayCurve = nullptr;
        std::atomic<float>* pitchDepth = nullptr;
        std::atomic<float>* pitchDecay = nullptr;
        std::atomic<float>* oscMorph = nullptr;
        std::atomic<float>* clickType = nullptr;
        std::atomic<float>* clickLevel = nullptr;
        std::atomic<float>* driveMode = nullptr;
        std::atomic<float>* driveGain = nullptr;
        std::atomic<float>* cutoff = nullptr;
        std::atomic<float>* filterEnvDecay = nullptr;
        std::atomic<float>* filterEnvDepth = nullptr;
        std::atomic<float>* masterVol = nullptr;
        std::atomic<float>* release = nullptr;
        std::atomic<float>* gateMode = nullptr;   // 0 one-shot, 1 gate
        std::atomic<float>* legato = nullptr;     // 0 retrigger, 1 legato
        std::atomic<float>* tune = nullptr;       // semitones
        std::atomic<float>* fineTune = nullptr;   // cents
        std::atomic<float>* velSens = nullptr;    // 0..1
        std::atomic<float>* quality = nullptr;    // 0 = 2x, 1 = 4x oversampling
    } params;

    // Drive runs oversampled to keep clip/foldback aliasing out of the audio.
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling2x, oversampling4x;
    int activeQuality = -1;
    bool smoothersPrimed = false;
    float bendSmoothed = 0.0f;
    int currentProgram = 0;

    // DSP Core Elements
    GlideUnit glideUnit;
    SubEngine subEngine;
    ClickEngine clickEngine;
    SaturationCore saturationCore;

    // Post-Drive SVF Low-Pass Filter
    juce::dsp::StateVariableTPTFilter<float> postDriveFilter;

    // Fixed ~20 Hz high-pass after the drive: removes the DC the asymmetric
    // tube mode creates, plus rumble nobody can hear but headroom pays for.
    juce::dsp::StateVariableTPTFilter<float> lowCutFilter;

    // Parameter smoothing values
    juce::LinearSmoothedValue<float> smoothedMasterVol;
    juce::LinearSmoothedValue<float> smoothedCutoff;
    juce::LinearSmoothedValue<float> smoothedDrive;
    juce::LinearSmoothedValue<float> smoothedClickLevel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(G0GAudioProcessor)
};
