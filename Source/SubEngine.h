#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

/**
 * Advanced Sub-bass generator featuring a multi-waveform morphing oscillator,
 * pitch drop envelope, shape-bending AHD amplitude contour, and a dedicated filter envelope.
 */
class SubEngine {
public:
    enum class EnvState { Idle, Hold, Decay };

    SubEngine() = default;

    void prepare(double newSampleRate) {
        sampleRate = newSampleRate;
        reset();
    }

    void reset() {
        phase = 0.0;
        envState = EnvState::Idle;
        holdSamplesElapsed = 0;
        decaySamplesElapsed = 0;
        pitchSamplesElapsed = 0;
        filterSamplesElapsed = 0;
        noteActive = false;
        triggerVelocity = 0.0f;
    }

    void trigger(float velocity) {
        envState = EnvState::Hold;
        holdSamplesElapsed = 0;
        decaySamplesElapsed = 0;
        pitchSamplesElapsed = 0;
        filterSamplesElapsed = 0;
        noteActive = true;
        triggerVelocity = velocity;
    }

    void release() {
        if (envState == EnvState::Hold) {
            envState = EnvState::Decay;
            decaySamplesElapsed = 0;
        }
    }

    void setHoldTime(float holdTimeMs) {
        holdSamplesTotal = static_cast<int>((holdTimeMs / 1000.0f) * sampleRate);
    }

    void setDecayTime(float decayTimeMs) {
        decaySamplesTotal = static_cast<int>((decayTimeMs / 1000.0f) * sampleRate);
    }

    void setPitchParams(float depthSemitones, float decayTimeMs) {
        pitchDepth = depthSemitones;
        pitchSamplesTotal = static_cast<int>((decayTimeMs / 1000.0f) * sampleRate);
    }

    void setOscMorph(float morph) {
        oscMorph = juce::jlimit(0.0f, 2.0f, morph);
    }

    void setDecayShape(float shape) {
        decayShape = juce::jlimit(-1.0f, 1.0f, shape);
    }

    void setFilterEnvParams(float depthHz, float decayTimeMs) {
        filterEnvDepth = depthHz;
        filterEnvSamplesTotal = static_cast<int>((decayTimeMs / 1000.0f) * sampleRate);
    }

    float getFilterEnvOffset() {
        if (envState == EnvState::Idle) {
            return 0.0f;
        }

        if (filterEnvSamplesTotal > 0 && filterSamplesElapsed < filterEnvSamplesTotal) {
            float ratio = static_cast<float>(filterSamplesElapsed) / static_cast<float>(filterEnvSamplesTotal);
            filterSamplesElapsed++;
            return filterEnvDepth * std::exp(-5.0f * ratio);
        }
        return 0.0f;
    }

    float getFilterEnvProgress() const {
        if (envState != EnvState::Idle && filterEnvSamplesTotal > 0) {
            float progress = static_cast<float>(filterSamplesElapsed) / static_cast<float>(filterEnvSamplesTotal);
            return juce::jlimit(0.0f, 1.0f, progress);
        }
        return 0.0f;
    }

    float getNextSample(float baseFrequency) {
        if (envState == EnvState::Idle) {
            return 0.0f;
        }

        // 1. Calculate pitch envelope offset
        float pitchOffsetSemitones = 0.0f;
        if (pitchSamplesTotal > 0 && pitchSamplesElapsed < pitchSamplesTotal) {
            float ratio = static_cast<float>(pitchSamplesElapsed) / static_cast<float>(pitchSamplesTotal);
            pitchOffsetSemitones = pitchDepth * std::exp(-5.0f * ratio);
            pitchSamplesElapsed++;
        }

        float frequency = baseFrequency * std::pow(2.0f, pitchOffsetSemitones / 12.0f);

        // 2. Accumulate oscillator phase
        phase += (2.0 * juce::MathConstants<double>::pi * frequency) / sampleRate;
        if (phase >= 2.0 * juce::MathConstants<double>::pi) {
            phase -= 2.0 * juce::MathConstants<double>::pi;
        }

        // 3. Synthesize morphing waveforms (Sine -> Triangle -> Square)
        // Sine
        float sineVal = std::sin(phase);

        // Triangle
        float phi = static_cast<float>(phase / (2.0 * juce::MathConstants<double>::pi));
        float triVal = 0.0f;
        if (phi < 0.25f) triVal = 4.0f * phi;
        else if (phi < 0.75f) triVal = 2.0f - 4.0f * phi;
        else triVal = -4.0f + 4.0f * phi;

        // Square
        float sqVal = (sineVal >= 0.0f) ? 1.0f : -1.0f;

        // Linearly interpolate waves based on oscMorph (0 = Sine, 1 = Tri, 2 = Square)
        float oscValue = 0.0f;
        if (oscMorph < 1.0f) {
            oscValue = (1.0f - oscMorph) * sineVal + oscMorph * triVal;
        } else {
            float m2 = oscMorph - 1.0f;
            oscValue = (1.0f - m2) * triVal + m2 * sqVal;
        }

        // 4. Process shape-bent amplitude envelope (AHD)
        float amp = 0.0f;
        if (envState == EnvState::Hold) {
            amp = 1.0f;
            holdSamplesElapsed++;
            if (holdSamplesElapsed >= holdSamplesTotal) {
                envState = EnvState::Decay;
                decaySamplesElapsed = 0;
            }
        } else if (envState == EnvState::Decay) {
            if (decaySamplesTotal > 0) {
                float ratio = static_cast<float>(decaySamplesElapsed) / static_cast<float>(decaySamplesTotal);
                
                // Shape-bending: -1.0 = Linear, 0.0 = Exponential, 1.0 = Tight Exp
                if (decayShape == 0.0f) {
                    amp = std::exp(-5.0f * ratio);
                } else if (decayShape < 0.0f) {
                    // Linear morph
                    float lin = 1.0f - ratio;
                    float expVal = std::exp(-5.0f * ratio);
                    amp = (1.0f + decayShape) * expVal - decayShape * lin;
                } else {
                    // Tight exponential rates 5 to 12
                    float rate = 5.0f + decayShape * 7.0f;
                    amp = std::exp(-rate * ratio);
                }

                decaySamplesElapsed++;
                if (ratio >= 1.0f || amp < 0.0001f) {
                    envState = EnvState::Idle;
                    noteActive = false;
                }
            } else {
                envState = EnvState::Idle;
                noteActive = false;
            }
        }

        return oscValue * amp * triggerVelocity;
    }

    bool isActive() const { return envState != EnvState::Idle; }
    EnvState getEnvState() const { return envState; }
    
    float getHoldProgress() const {
        if (envState == EnvState::Hold && holdSamplesTotal > 0)
            return static_cast<float>(holdSamplesElapsed) / static_cast<float>(holdSamplesTotal);
        return 0.0f;
    }
    
    float getDecayProgress() const {
        if (envState == EnvState::Decay && decaySamplesTotal > 0)
            return static_cast<float>(decaySamplesElapsed) / static_cast<float>(decaySamplesTotal);
        return 0.0f;
    }

private:
    double sampleRate = 44100.0;
    double phase = 0.0;
    
    EnvState envState = EnvState::Idle;
    int holdSamplesTotal = 0;
    int holdSamplesElapsed = 0;
    int decaySamplesTotal = 0;
    int decaySamplesElapsed = 0;

    float pitchDepth = 24.0f;
    int pitchSamplesTotal = 0;
    int pitchSamplesElapsed = 0;

    // Advanced morph parameters
    float oscMorph = 0.0f;   // 0.0 = Sine, 1.0 = Tri, 2.0 = Square
    float decayShape = 0.0f; // -1.0 = Linear, 0.0 = Exp, 1.0 = Tight

    // Dedicated filter envelope parameters
    float filterEnvDepth = 1000.0f;
    int filterEnvSamplesTotal = 0;
    int filterSamplesElapsed = 0;

    bool noteActive = false;
    float triggerVelocity = 0.0f;
};
