#include "PluginProcessor.h"
#include "PluginEditor.h"

G0GAudioProcessor::G0GAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

G0GAudioProcessor::~G0GAudioProcessor() = default;

const juce::String G0GAudioProcessor::getName() const {
    return "G0G";
}

bool G0GAudioProcessor::acceptsMidi() const {
    return true;
}

bool G0GAudioProcessor::producesMidi() const {
    return false;
}

bool G0GAudioProcessor::isMidiEffect() const {
    return false;
}

double G0GAudioProcessor::getTailLengthSeconds() const {
    return 0.5; // tail buffer to prevent abrupt cutoffs
}

int G0GAudioProcessor::getNumPrograms() {
    return 1;
}

int G0GAudioProcessor::getCurrentProgram() {
    return 0;
}

void G0GAudioProcessor::setCurrentProgram(int /*index*/) {}

const juce::String G0GAudioProcessor::getProgramName(int /*index*/) {
    return "Default";
}

void G0GAudioProcessor::changeProgramName(int /*index*/, const juce::String& /*newName*/) {}

void G0GAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    // 1. Prepare DSP components
    glideUnit.prepare(sampleRate);
    subEngine.prepare(sampleRate);
    clickEngine.prepare(sampleRate);
    clickEngine.setDuration(15.0f); // Fixed 15ms transient click

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 2;

    postDriveFilter.prepare(spec);
    postDriveFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

    // 2. Initialize parameter smoothers
    smoothedMasterVol.reset(sampleRate, 0.05);
    smoothedCutoff.reset(sampleRate, 0.05);
    smoothedDrive.reset(sampleRate, 0.05);
    smoothedClickLevel.reset(sampleRate, 0.05);
}

void G0GAudioProcessor::releaseResources() {
    // Clear heavy resources here if any
}

bool G0GAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    // Stereo output only
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void G0GAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    int numSamples = buffer.getNumSamples();

    // 1. Clear input buffer (synthesis generates audio)
    buffer.clear();

    // 2. Feed GUI keyboard midi events to DAWs
    keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples, true);

    // 3. Load dynamic parameters
    float glideMs = apvts.getRawParameterValue("glide_time")->load();
    glideUnit.setGlideTime(glideMs);
    
    float glideCurve = apvts.getRawParameterValue("glide_curve")->load();
    glideUnit.setGlideCurve(glideCurve);

    float holdMs = apvts.getRawParameterValue("ahd_hold")->load();
    subEngine.setHoldTime(holdMs);

    float decayMs = apvts.getRawParameterValue("ahd_decay")->load();
    subEngine.setDecayTime(decayMs);

    float pitchDepth = apvts.getRawParameterValue("pitch_depth")->load();
    float pitchDecayMs = apvts.getRawParameterValue("pitch_decay")->load();
    subEngine.setPitchParams(pitchDepth, pitchDecayMs);

    float oscMorph = apvts.getRawParameterValue("osc_morph")->load();
    subEngine.setOscMorph(oscMorph);

    float decayShape = apvts.getRawParameterValue("decay_curve")->load();
    subEngine.setDecayShape(decayShape);

    float filterEnvDepth = apvts.getRawParameterValue("filter_env_depth")->load();
    float filterEnvDecayMs = apvts.getRawParameterValue("filter_env_decay")->load();
    subEngine.setFilterEnvParams(filterEnvDepth, filterEnvDecayMs);

    int clickTypeVal = static_cast<int>(apvts.getRawParameterValue("click_type")->load());
    auto driveMode = static_cast<SaturationCore::Mode>(static_cast<int>(apvts.getRawParameterValue("drive_mode")->load()));

    smoothedMasterVol.setTargetValue(juce::Decibels::decibelsToGain(apvts.getRawParameterValue("master_volume")->load()));
    smoothedCutoff.setTargetValue(apvts.getRawParameterValue("cutoff")->load());
    smoothedDrive.setTargetValue(apvts.getRawParameterValue("drive_gain")->load());
    smoothedClickLevel.setTargetValue(apvts.getRawParameterValue("click_level")->load());

    // 4. Parse Midi events in the current block
    for (const auto metadata : midiMessages) {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn()) {
            float velocity = msg.getVelocity();
            glideUnit.noteOn(msg.getNoteNumber(), velocity);
            subEngine.trigger(velocity);
            clickEngine.trigger(clickTypeVal);
        } else if (msg.isNoteOff()) {
            glideUnit.noteOff(msg.getNoteNumber());
            // Only release amplitude envelope if no notes are held down
            if (!glideUnit.isAnyNoteHeld()) {
                subEngine.release();
            }
        }
    }

    // 5. Generate audio samples
    float* leftChannel = buffer.getWritePointer(0);
    float* rightChannel = buffer.getWritePointer(1);

    for (int i = 0; i < numSamples; ++i) {
        // Track the frequency slide
        float freq = glideUnit.getNextFrequency();
        
        // Generate sub signal
        float subSample = subEngine.getNextSample(freq);
        
        // Generate transient knock
        float clickSample = clickEngine.getNextSample() * smoothedClickLevel.getNextValue();

        // Sum monophonic voice
        float summed = subSample + clickSample;

        leftChannel[i] = summed;
        rightChannel[i] = summed;
    }

    // 6. Apply waveshaping saturation
    float drive = smoothedDrive.getCurrentValue();
    smoothedDrive.skip(numSamples);
    saturationCore.process(leftChannel, rightChannel, numSamples, driveMode, drive);

    // 7. Apply Post-Drive Lowpass filter
    float baseCutoff = smoothedCutoff.getCurrentValue();
    smoothedCutoff.skip(numSamples);
    
    float filterEnvOffset = subEngine.getFilterEnvOffset();
    float modulatedCutoff = baseCutoff + filterEnvOffset;
    postDriveFilter.setCutoffFrequency(juce::jlimit(20.0f, 20000.0f, modulatedCutoff));
    
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    postDriveFilter.process(context);

    // 8. Apply Master Volume Gain with smoothing
    if (smoothedMasterVol.isSmoothing()) {
        float startGain = smoothedMasterVol.getCurrentValue();
        smoothedMasterVol.skip(numSamples);
        float endGain = smoothedMasterVol.getCurrentValue();
        buffer.applyGainRamp(0, 0, numSamples, startGain, endGain);
        buffer.applyGainRamp(1, 0, numSamples, startGain, endGain);
    } else {
        buffer.applyGain(smoothedMasterVol.getTargetValue());
    }
}

bool G0GAudioProcessor::hasEditor() const {
    return true;
}

juce::AudioProcessorEditor* G0GAudioProcessor::createEditor() {
    return new G0GAudioProcessorEditor(*this);
}

void G0GAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void G0GAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr) {
        if (xmlState->hasTagName(apvts.state.getType())) {
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout G0GAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 1. Envelope & Glide Parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("glide_time", 1), "Glide Time (ms)", juce::NormalisableRange<float>(0.0f, 1000.0f, 1.0f, 0.5f), 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("glide_curve", 1), "Glide Curve (Lin-Exp)", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("ahd_hold", 1), "Hold (ms)", juce::NormalisableRange<float>(0.0f, 500.0f, 1.0f, 0.5f), 20.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("ahd_decay", 1), "Decay (ms)", juce::NormalisableRange<float>(50.0f, 5000.0f, 1.0f, 0.4f), 800.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("decay_curve", 1), "Decay Curve Shape", -1.0f, 1.0f, 0.0f));

    // 2. Pitch Punch Parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("pitch_decay", 1), "Pitch Decay (ms)", juce::NormalisableRange<float>(10.0f, 150.0f, 1.0f, 0.5f), 30.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("pitch_depth", 1), "Pitch Depth (st)", juce::NormalisableRange<float>(0.0f, 48.0f, 1.0f), 24.0f));

    // 3. Sub Waveform Parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("osc_morph", 1), "Sub Wave Morph", 0.0f, 2.0f, 0.0f));

    // 4. Transient Click Parameters
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("click_type", 1), "Click Noise Type", juce::StringArray("Off", "White Noise", "Pink Noise"), 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("click_level", 1), "Click Vol", 0.0f, 1.0f, 0.15f));

    // 5. Saturation Parameters
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("drive_mode", 1), "Drive Mode", juce::StringArray("Bypass", "Tube", "Clip", "Foldback"), 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("drive_gain", 1), "Drive Gain", juce::NormalisableRange<float>(1.0f, 10.0f, 0.05f, 0.5f), 1.0f));

    // 6. Post Filter Parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("cutoff", 1), "LPF Cutoff (Hz)", juce::NormalisableRange<float>(100.0f, 8000.0f, 1.0f, 0.3f), 500.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("filter_env_decay", 1), "Filter Env Decay (ms)", juce::NormalisableRange<float>(10.0f, 1000.0f, 1.0f, 0.4f), 150.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("filter_env_depth", 1), "Filter Env Depth (Hz)", juce::NormalisableRange<float>(0.0f, 5000.0f, 1.0f, 0.4f), 1000.0f));

    // 7. Master Volume
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("master_volume", 1), "Master Vol (dB)", juce::NormalisableRange<float>(-60.0f, 6.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

// Global VST/AU entry-point creator function
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new G0GAudioProcessor();
}
