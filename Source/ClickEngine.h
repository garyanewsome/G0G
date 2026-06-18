#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

/**
 * ClickEngine creates a transient noise knock on note-on.
 * Emits a short burst of White or Pink noise, passed through a 2-pole High-Pass Filter (800 Hz).
 */
class ClickEngine {
public:
    ClickEngine() = default;

    void prepare(double newSampleRate) {
        sampleRate = newSampleRate;
        reset();
        
        // Pre-calculate 800Hz 2-pole Butterworth HPF coefficients
        float cutoff = 800.0f;
        float q = 0.7071f; // Butterworth alignment
        float w0 = 2.0f * juce::MathConstants<float>::pi * cutoff / static_cast<float>(sampleRate);
        float cosw0 = std::cos(w0);
        float alpha = std::sin(w0) / (2.0f * q);

        b0 = (1.0f + cosw0) / 2.0f;
        b1 = -(1.0f + cosw0);
        b2 = (1.0f + cosw0) / 2.0f;
        float a0 = 1.0f + alpha;
        a1 = -2.0f * cosw0;
        a2 = 1.0f - alpha;

        // Normalize coefficients
        b0 /= a0;
        b1 /= a0;
        b2 /= a0;
        a1 /= a0;
        a2 /= a0;
    }

    void reset() {
        samplesElapsed = 0;
        active = false;
        
        // Clear filter history
        x1 = 0.0f;
        x2 = 0.0f;
        y1 = 0.0f;
        y2 = 0.0f;

        // Reset pink noise filter registers
        pinkB0 = pinkB1 = pinkB2 = pinkB3 = pinkB4 = pinkB5 = pinkB6 = 0.0f;
    }

    void trigger(int type) {
        // type: 0 = Off, 1 = White, 2 = Pink
        clickType = type;
        if (clickType > 0) {
            active = true;
            samplesElapsed = 0;
            // Clear filter state on trigger to avoid pops
            x1 = x2 = y1 = y2 = 0.0f;
        } else {
            active = false;
        }
    }

    void setDuration(float durationMs) {
        clickSamplesTotal = static_cast<int>((durationMs / 1000.0f) * sampleRate);
    }

    float getNextSample() {
        if (!active || samplesElapsed >= clickSamplesTotal) {
            active = false;
            return 0.0f;
        }

        samplesElapsed++;

        // 1. Generate Noise sample
        float noise = 0.0f;
        if (clickType == 1) {
            // White Noise
            noise = random.nextFloat() * 2.0f - 1.0f;
        } else if (clickType == 2) {
            // Pink Noise (Kellet Refined Voss method)
            float white = random.nextFloat() * 2.0f - 1.0f;
            pinkB0 = 0.99886f * pinkB0 + white * 0.0555179f;
            pinkB1 = 0.99332f * pinkB1 + white * 0.0750759f;
            pinkB2 = 0.96900f * pinkB2 + white * 0.1538520f;
            pinkB3 = 0.86650f * pinkB3 + white * 0.3104856f;
            pinkB4 = 0.55000f * pinkB4 + white * 0.5329522f;
            pinkB5 = -0.7616f * pinkB5 - white * 0.0168980f;
            noise = pinkB0 + pinkB1 + pinkB2 + pinkB3 + pinkB4 + pinkB5 + pinkB6 + white * 0.5362f;
            pinkB6 = white * 0.115926f;
            
            // Adjust pink noise gain to roughly match white noise RMS
            noise *= 0.12f;
        }

        // 2. Process HPF (800 Hz Biquad)
        float filtered = b0 * noise + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = noise;
        y2 = y1;
        y1 = filtered;

        // 3. Smooth envelope tail to zero to prevent clicks
        float progress = static_cast<float>(samplesElapsed) / static_cast<float>(clickSamplesTotal);
        float envelope = 1.0f - progress;

        return filtered * envelope;
    }

    bool isActive() const { return active; }

private:
    double sampleRate = 44100.0;
    juce::Random random;
    
    int clickSamplesTotal = 0;
    int samplesElapsed = 0;
    bool active = false;
    int clickType = 1; // 0 = Off, 1 = White, 2 = Pink

    // Coefficients
    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    
    // History
    float x1 = 0.0f, x2 = 0.0f;
    float y1 = 0.0f, y2 = 0.0f;

    // Pink noise filter state
    float pinkB0 = 0.0f, pinkB1 = 0.0f, pinkB2 = 0.0f, pinkB3 = 0.0f, pinkB4 = 0.0f, pinkB5 = 0.0f, pinkB6 = 0.0f;
};
