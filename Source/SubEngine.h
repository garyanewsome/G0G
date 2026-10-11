#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

/**
 * Advanced Sub-bass generator featuring a multi-waveform morphing oscillator,
 * pitch drop envelope, shape-bending AHD amplitude contour, and a dedicated filter envelope.
 */
class SubEngine {
public:
    enum class EnvState { Idle, Hold, Decay, Release };

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
        releaseSamplesElapsed = 0;
        lastAmp = 0.0f;
        noteActive = false;
        triggerVelocity = 0.0f;
    }

    // Starts the pitch punch + amplitude envelope. With resetPhase the sub
    // always starts at the same point of its cycle, so every hit has the same
    // attack (an 808 kick is identical each time).
    void trigger(float velocity, bool resetPhase = true) {
        envState = EnvState::Hold;
        holdSamplesElapsed = 0;
        decaySamplesElapsed = 0;
        pitchSamplesElapsed = 0;
        filterSamplesElapsed = 0;
        releaseSamplesElapsed = 0;
        noteActive = true;
        triggerVelocity = velocity;
        if (resetPhase) phase = 0.0;
    }

    // Key released and no other key held.
    //  gate == false (one-shot): the tail rings out at its own decay time.
    //  gate == true: the sound fades out over the release time, so the note
    //                length decides how long the bass lasts.
    void release(bool gate = false) {
        if (gate) {
            if (envState == EnvState::Hold || envState == EnvState::Decay) {
                frozenHoldProgress = getHoldProgress();
                frozenDecayProgress = getDecayProgress();
                releaseStartAmp = lastAmp;
                releaseSamplesElapsed = 0;
                envState = EnvState::Release;
            }
        } else if (envState == EnvState::Hold) {
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

    void setReleaseTime(float releaseTimeMs) {
        releaseSamplesTotal = juce::jmax(1, static_cast<int>((releaseTimeMs / 1000.0f) * sampleRate));
    }

    void setDecayShape(float shape) {
        decayShape = juce::jlimit(-1.0f, 1.0f, shape);
    }

    void setFilterEnvParams(float depthHz, float decayTimeMs) {
        filterEnvDepth = depthHz;
        filterEnvSamplesTotal = static_cast<int>((decayTimeMs / 1000.0f) * sampleRate);
    }

    // Advances the filter envelope by ONE SAMPLE and returns the cutoff
    // offset in Hz. Must be called once per rendered sample.
    float getNextFilterEnvOffset() {
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

        // Square, band-limited with PolyBLEP so it does not alias on higher notes.
        const float dt = static_cast<float>(juce::jlimit(1.0e-5, 0.49, static_cast<double>(frequency) / sampleRate));
        float sqVal = (phi < 0.5f) ? 1.0f : -1.0f;
        sqVal += polyBlep(phi, dt);
        float phiHalf = phi + 0.5f;
        if (phiHalf >= 1.0f) phiHalf -= 1.0f;
        sqVal -= polyBlep(phiHalf, dt);

        // Linearly interpolate waves based on oscMorph (0 = Sine, 1 = Tri, 2 = Square)
        float oscValue = 0.0f;
        if (oscMorph < 1.0f) {
            oscValue = (1.0f - oscMorph) * sineVal + oscMorph * triVal;
        } else {
            float m2 = oscMorph - 1.0f;
            oscValue = (1.0f - m2) * triVal + m2 * sqVal;
        }

        // 4. Amplitude envelope: hold -> shaped decay, or a gated release
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
                amp = decayAmplitude(ratio);

                decaySamplesElapsed++;
                if (ratio >= 1.0f || amp < 0.0001f) {
                    envState = EnvState::Idle;
                    noteActive = false;
                }
            } else {
                envState = EnvState::Idle;
                noteActive = false;
            }
        } else if (envState == EnvState::Release) {
            float ratio = static_cast<float>(releaseSamplesElapsed) / static_cast<float>(releaseSamplesTotal);
            // Normalised exponential fade, exactly zero at the end.
            const float floorV = std::exp(-5.0f);
            amp = releaseStartAmp * juce::jmax(0.0f, (std::exp(-5.0f * ratio) - floorV) / (1.0f - floorV));
            releaseSamplesElapsed++;
            if (ratio >= 1.0f || amp < 0.0001f) {
                envState = EnvState::Idle;
                noteActive = false;
            }
        }
        lastAmp = amp;

        return oscValue * amp * triggerVelocity;
    }

    bool isActive() const { return envState != EnvState::Idle; }
    EnvState getEnvState() const { return envState; }
    
    float getHoldProgress() const {
        if (envState == EnvState::Release)
            return frozenHoldProgress;
        if (envState == EnvState::Hold && holdSamplesTotal > 0)
            return static_cast<float>(holdSamplesElapsed) / static_cast<float>(holdSamplesTotal);
        return 0.0f;
    }
    
    float getDecayProgress() const {
        if (envState == EnvState::Release)
            return frozenDecayProgress;
        if (envState == EnvState::Decay && decaySamplesTotal > 0)
            return static_cast<float>(decaySamplesElapsed) / static_cast<float>(decaySamplesTotal);
        return 0.0f;
    }

private:
    // Polynomial band-limited step correction for a discontinuity at t = 0
    // (t in [0,1), dt = phase increment per sample in cycles).
    static float polyBlep(float t, float dt) {
        if (t < dt) {
            t /= dt;
            return t + t - t * t - 1.0f;
        }
        if (t > 1.0f - dt) {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }
        return 0.0f;
    }

    // Raw shape: -1.0 = linear, 0.0 = exponential, 1.0 = tight exponential.
    float shapeRaw(float ratio) const {
        if (decayShape == 0.0f)
            return std::exp(-5.0f * ratio);
        if (decayShape < 0.0f) {
            float lin = 1.0f - ratio;
            float expVal = std::exp(-5.0f * ratio);
            return (1.0f + decayShape) * expVal - decayShape * lin;
        }
        float rate = 5.0f + decayShape * 7.0f;
        return std::exp(-rate * ratio);
    }

    // Normalised so the tail is exactly 1 at the start and exactly 0 at the
    // end (the raw curves stop ~0.7% short, which clicked when the voice cut).
    float decayAmplitude(float ratio) const {
        const float end = shapeRaw(1.0f);
        return juce::jmax(0.0f, (shapeRaw(ratio) - end) / (1.0f - end));
    }

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

    // Gated release
    int releaseSamplesTotal = 4800;
    int releaseSamplesElapsed = 0;
    float releaseStartAmp = 0.0f;
    float lastAmp = 0.0f;
    float frozenHoldProgress = 0.0f;
    float frozenDecayProgress = 0.0f;

    bool noteActive = false;
    float triggerVelocity = 0.0f;
};
