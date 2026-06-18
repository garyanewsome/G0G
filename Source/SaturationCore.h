#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

/**
 * Multi-mode waveshaping saturation core.
 * Features Tube (asymmetric odd/even harmonics), Clip (tanh odd harmonics), and Foldback (industrial wavefolding).
 */
class SaturationCore {
public:
    enum class Mode {
        Bypass = 0,
        Tube,
        Clip,
        Foldback
    };

    SaturationCore() = default;

    void process(float* left, float* right, int numSamples, Mode mode, float driveGain) {
        if (mode == Mode::Bypass || driveGain <= 1.001f) {
            return;
        }

        for (int i = 0; i < numSamples; ++i) {
            left[i] = processSample(left[i], mode, driveGain);
            right[i] = processSample(right[i], mode, driveGain);
        }
    }

    float processSample(float x, Mode mode, float drive) {
        if (mode == Mode::Bypass || drive <= 1.001f) {
            return x;
        }

        if (mode == Mode::Clip) {
            // Symmetric tanh clipping
            float s = x * drive;
            float limit = std::tanh(drive);
            return (limit > 0.0f) ? (std::tanh(s) / limit) : 0.0f;
        } 
        
        if (mode == Mode::Tube) {
            // Asymmetric tube clipping (positive half driven harder than negative half)
            float limitPos = std::tanh(drive);
            float limitNeg = std::tanh(drive * 0.5f);
            
            if (x >= 0.0f) {
                float s = x * drive;
                return (limitPos > 0.0f) ? (std::tanh(s) / limitPos) : 0.0f;
            } else {
                float s = x * drive * 0.5f;
                return (limitNeg > 0.0f) ? (std::tanh(s) / limitNeg) : 0.0f;
            }
        } 
        
        if (mode == Mode::Foldback) {
            // Foldback: mirror peaks exceeding hard threshold back into boundary
            float s = x * drive;
            float threshold = 0.7f;
            
            if (s > threshold) {
                s = threshold - (s - threshold);
                if (s < -threshold) s = -threshold; // clamp to prevent infinite fold wrap
            } else if (s < -threshold) {
                s = -threshold - (s + threshold);
                if (s > threshold) s = threshold;
            }
            
            return s / threshold; // output normalized to unity range
        }

        return x;
    }
};
