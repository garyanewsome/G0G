#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace {
// Transparent below 0.9; above it the knee eases into a hard ceiling at 1.0 so
// the output never exceeds full scale (drive + DC removal can overshoot).
inline float softLimit(float x) {
    constexpr float threshold = 0.9f, room = 1.0f - threshold;
    const float a = std::abs(x);
    if (a <= threshold)
        return x;
    return std::copysign(threshold + room * std::tanh((a - threshold) / room), x);
}
} // namespace

G0GAudioProcessor::G0GAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    params.glideTime      = apvts.getRawParameterValue("glide_time");
    params.glideCurve     = apvts.getRawParameterValue("glide_curve");
    params.hold           = apvts.getRawParameterValue("ahd_hold");
    params.decay          = apvts.getRawParameterValue("ahd_decay");
    params.decayCurve     = apvts.getRawParameterValue("decay_curve");
    params.pitchDepth     = apvts.getRawParameterValue("pitch_depth");
    params.pitchDecay     = apvts.getRawParameterValue("pitch_decay");
    params.oscMorph       = apvts.getRawParameterValue("osc_morph");
    params.clickType      = apvts.getRawParameterValue("click_type");
    params.clickLevel     = apvts.getRawParameterValue("click_level");
    params.driveMode      = apvts.getRawParameterValue("drive_mode");
    params.driveGain      = apvts.getRawParameterValue("drive_gain");
    params.cutoff         = apvts.getRawParameterValue("cutoff");
    params.filterEnvDecay = apvts.getRawParameterValue("filter_env_decay");
    params.filterEnvDepth = apvts.getRawParameterValue("filter_env_depth");
    params.masterVol      = apvts.getRawParameterValue("master_volume");
    params.release        = apvts.getRawParameterValue("release");
    params.gateMode       = apvts.getRawParameterValue("gate_mode");
    params.legato         = apvts.getRawParameterValue("legato");
    params.tune           = apvts.getRawParameterValue("tune");
    params.fineTune       = apvts.getRawParameterValue("fine_tune");
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
    return static_cast<int>(Presets::factory().size());
}

int G0GAudioProcessor::getCurrentProgram() {
    return currentProgram;
}

void G0GAudioProcessor::setCurrentProgram(int index) {
    if (index < 0 || index >= getNumPrograms())
        return;
    currentProgram = index;
    Presets::applyFactory(*this, index);
}

const juce::String G0GAudioProcessor::getProgramName(int index) {
    if (index < 0 || index >= getNumPrograms())
        return {};
    return Presets::factory()[static_cast<size_t>(index)].name;
}

void G0GAudioProcessor::changeProgramName(int /*index*/, const juce::String& /*newName*/) {}

void G0GAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    glideUnit.prepare(sampleRate);
    subEngine.prepare(sampleRate);
    clickEngine.prepare(sampleRate);
    clickEngine.setDuration(15.0f); // Fixed 15ms transient click

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(juce::jmax(samplesPerBlock, kChunk));
    spec.numChannels = 2;

    postDriveFilter.prepare(spec);
    postDriveFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

    lowCutFilter.prepare(spec);
    lowCutFilter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    lowCutFilter.setCutoffFrequency(20.0f);

    // 2x oversampled drive (polyphase half-band IIR, low latency).
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampling->initProcessing(static_cast<size_t>(kChunk));
    setLatencySamples(juce::roundToInt(oversampling->getLatencyInSamples()));

    smoothedMasterVol.reset(sampleRate, 0.05);
    smoothedCutoff.reset(sampleRate, 0.05);
    smoothedDrive.reset(sampleRate, 0.05);
    smoothedClickLevel.reset(sampleRate, 0.05);
    smoothersPrimed = false;
    bendSmoothed = 0.0f;
}

void G0GAudioProcessor::releaseResources() {
}

bool G0GAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    // Stereo output only
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void G0GAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    buffer.clear();

    // On-screen keyboard events join the host's MIDI.
    keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples, true);

    // Parameters, read once per block (cached handles, no string lookups).
    glideUnit.setGlideTime(params.glideTime->load());
    glideUnit.setGlideCurve(params.glideCurve->load());
    subEngine.setHoldTime(params.hold->load());
    subEngine.setDecayTime(params.decay->load());
    subEngine.setPitchParams(params.pitchDepth->load(), params.pitchDecay->load());
    subEngine.setOscMorph(params.oscMorph->load());
    subEngine.setDecayShape(params.decayCurve->load());
    subEngine.setReleaseTime(params.release->load());
    const bool gateMode = params.gateMode->load() > 0.5f;
    const bool legatoMode = params.legato->load() > 0.5f;
    const float tuneRatio = std::exp2((params.tune->load() + params.fineTune->load() / 100.0f) / 12.0f);
    subEngine.setFilterEnvParams(params.filterEnvDepth->load(), params.filterEnvDecay->load());

    const int clickTypeVal = static_cast<int>(params.clickType->load());
    const auto driveMode = static_cast<SaturationCore::Mode>(static_cast<int>(params.driveMode->load()));

    const float masterTarget = juce::Decibels::decibelsToGain(params.masterVol->load());
    if (!smoothersPrimed) {
        // First block: start at the stored values instead of ramping from 0.
        smoothedMasterVol.setCurrentAndTargetValue(masterTarget);
        smoothedCutoff.setCurrentAndTargetValue(params.cutoff->load());
        smoothedDrive.setCurrentAndTargetValue(params.driveGain->load());
        smoothedClickLevel.setCurrentAndTargetValue(params.clickLevel->load());
        smoothersPrimed = true;
    } else {
        smoothedMasterVol.setTargetValue(masterTarget);
        smoothedCutoff.setTargetValue(params.cutoff->load());
        smoothedDrive.setTargetValue(params.driveGain->load());
        smoothedClickLevel.setTargetValue(params.clickLevel->load());
    }

    float* left = buffer.getWritePointer(0);
    float* right = buffer.getWritePointer(1);

    auto midiIt = midiMessages.begin();
    const auto midiEnd = midiMessages.end();
    float filterEnv[kChunk];

    // Work in short chunks: MIDI is applied at its exact sample position,
    // and the drive / filter / gain stages update their controls per chunk.
    for (int chunkStart = 0; chunkStart < numSamples; chunkStart += kChunk) {
        const int n = juce::jmin(kChunk, numSamples - chunkStart);

        for (int j = 0; j < n; ++j) {
            const int pos = chunkStart + j;

            // Apply every event scheduled at or before this sample.
            while (midiIt != midiEnd && (*midiIt).samplePosition <= pos) {
                const auto msg = (*midiIt).getMessage();
                if (msg.isNoteOn()) {
                    // Legato: a note played while another is held just slides
                    // (no new punch, click or envelope), like a real 808 line.
                    const bool slurred = legatoMode && glideUnit.isAnyNoteHeld() && subEngine.isActive();
                    glideUnit.noteOn(msg.getNoteNumber(), msg.getFloatVelocity());
                    if (!slurred) {
                        subEngine.trigger(msg.getFloatVelocity(), true);
                        clickEngine.trigger(clickTypeVal);
                    }
                } else if (msg.isNoteOff()) {
                    glideUnit.noteOff(msg.getNoteNumber());
                    // Only release the amplitude envelope once all keys are up.
                    if (!glideUnit.isAnyNoteHeld())
                        subEngine.release(gateMode);
                } else if (msg.isPitchWheel()) {
                    pitchBend.store(static_cast<float>(msg.getPitchWheelValue() - 8192) / 8192.0f);
                } else if (msg.isController() && msg.getControllerNumber() == 1) {
                    modWheel.store(static_cast<float>(msg.getControllerValue()) / 127.0f);
                } else if (msg.isAllNotesOff() || msg.isAllSoundOff()) {
                    glideUnit.reset();
                    subEngine.release(true);
                }
                ++midiIt;
            }

            // Pitch bend: +/-2 semitones, one-pole smoothed against zipper noise.
            bendSmoothed += (pitchBend.load() - bendSmoothed) * 0.005f;
            const float bendRatio = std::exp2(bendSmoothed * 2.0f / 12.0f);

            const float freq = glideUnit.getNextFrequency() * bendRatio * tuneRatio;
            const float subSample = subEngine.getNextSample(freq);
            const float clickSample = clickEngine.getNextSample() * smoothedClickLevel.getNextValue();
            filterEnv[j] = subEngine.getNextFilterEnvOffset();

            const float summed = subSample + clickSample;
            left[chunkStart + j] = summed;
            right[chunkStart + j] = summed;
        }

        // Saturation, oversampled 2x. Skipped when it would do nothing.
        const float drive = smoothedDrive.skip(n);
        if (driveMode != SaturationCore::Mode::Bypass && drive > 1.001f) {
            float* chans[2] = { left + chunkStart, right + chunkStart };
            juce::dsp::AudioBlock<float> chunkBlock(chans, 2, static_cast<size_t>(n));
            auto upBlock = oversampling->processSamplesUp(chunkBlock);
            saturationCore.process(upBlock.getChannelPointer(0), upBlock.getChannelPointer(1),
                                   static_cast<int>(upBlock.getNumSamples()), driveMode, drive);
            oversampling->processSamplesDown(chunkBlock);
        }

        // Post-drive low-pass: cutoff = base + envelope + mod wheel, per sample.
        const float baseCutoff = smoothedCutoff.skip(n);
        const float wheelOffset = modWheel.load() * 4000.0f;
        for (int j = 0; j < n; ++j) {
            const float cut = juce::jlimit(20.0f, 20000.0f, baseCutoff + filterEnv[j] + wheelOffset);
            postDriveFilter.setCutoffFrequency(cut);
            const float l = lowCutFilter.processSample(0, left[chunkStart + j]);
            const float r = lowCutFilter.processSample(1, right[chunkStart + j]);
            left[chunkStart + j] = postDriveFilter.processSample(0, l);
            right[chunkStart + j] = postDriveFilter.processSample(1, r);
        }

        // Master volume with smoothing.
        for (int j = 0; j < n; ++j) {
            const float g = smoothedMasterVol.getNextValue();
            left[chunkStart + j] = softLimit(left[chunkStart + j] * g);
            right[chunkStart + j] = softLimit(right[chunkStart + j] * g);
        }
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

    // 6b. Gate / legato / tuning
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("gate_mode", 1), "Gate Mode", juce::StringArray("One-Shot", "Gate"), 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("release", 1), "Release (ms)", juce::NormalisableRange<float>(5.0f, 2000.0f, 1.0f, 0.4f), 120.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("legato", 1), "Legato", juce::StringArray("Retrigger", "Legato"), 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tune", 1), "Tune (st)", juce::NormalisableRange<float>(-24.0f, 24.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("fine_tune", 1), "Fine Tune (ct)", juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f), 0.0f));

    // 7. Master Volume
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("master_volume", 1), "Master Vol (dB)", juce::NormalisableRange<float>(-60.0f, 6.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

// Global VST/AU entry-point creator function
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new G0GAudioProcessor();
}
