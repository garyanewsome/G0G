#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <algorithm>
#include <cmath>

/**
 * Handles monophonic note tracking and frequency glide (portamento) interpolation.
 */
class GlideUnit {
public:
    GlideUnit() = default;

    void prepare(double newSampleRate) {
        sampleRate = newSampleRate;
        reset();
    }

    void reset() {
        noteStack.clear();
        currentFrequency = 0.0f;
        targetFrequency = 0.0f;
        sourceFrequency = 0.0f;
        glideSamplesTotal = 0.0f;
        glideSamplesElapsed = 0.0f;
        isGliding = false;
        activeMidiNote = -1;
    }

    void noteOn(int noteNumber, float /*velocity*/) {
        // Remove note from stack if it already exists to avoid duplicates
        noteStack.erase(std::remove(noteStack.begin(), noteStack.end(), noteNumber), noteStack.end());
        noteStack.push_back(noteNumber);

        float newTargetFreq = juce::MidiMessage::getMidiNoteInHertz(noteNumber);

        if (noteStack.size() == 1) {
            // First note: instantaneous frequency change
            currentFrequency = newTargetFreq;
            targetFrequency = newTargetFreq;
            sourceFrequency = newTargetFreq;
            isGliding = false;
        } else {
            // Subsequent note: glide from the current active frequency to the new note frequency
            sourceFrequency = currentFrequency;
            targetFrequency = newTargetFreq;
            glideSamplesElapsed = 0.0f;
            isGliding = (glideTimeSec > 0.0f);
            if (!isGliding) {
                currentFrequency = targetFrequency;
            }
        }
        activeMidiNote = noteNumber;
    }

    void noteOff(int noteNumber) {
        noteStack.erase(std::remove(noteStack.begin(), noteStack.end(), noteNumber), noteStack.end());

        if (activeMidiNote == noteNumber) {
            if (!noteStack.empty()) {
                // Return to the previously held note in the stack
                int prevNote = noteStack.back();
                activeMidiNote = prevNote;
                float newTargetFreq = juce::MidiMessage::getMidiNoteInHertz(prevNote);
                
                sourceFrequency = currentFrequency;
                targetFrequency = newTargetFreq;
                glideSamplesElapsed = 0.0f;
                isGliding = (glideTimeSec > 0.0f);
                if (!isGliding) {
                    currentFrequency = targetFrequency;
                }
            } else {
                activeMidiNote = -1;
                // No notes held, keep current frequency as target to allow decay phase to complete naturally
            }
        }
    }

    void setGlideTime(float glideTimeMs) {
        glideTimeSec = glideTimeMs / 1000.0f;
        glideSamplesTotal = glideTimeSec * static_cast<float>(sampleRate);
    }

    void setGlideCurve(float curve) {
        glideCurve = juce::jlimit(0.0f, 1.0f, curve);
    }

    float getNextFrequency() {
        if (isGliding && glideSamplesTotal > 0.0f) {
            glideSamplesElapsed += 1.0f;
            float ratio = glideSamplesElapsed / glideSamplesTotal;
            if (ratio >= 1.0f) {
                currentFrequency = targetFrequency;
                isGliding = false;
            } else {
                // Interpolate curve factor: 0.0 = Linear, 1.0 = Exponential
                float expFactor = std::exp(-5.0f * ratio);
                float linFactor = 1.0f - ratio;
                float factor = (1.0f - glideCurve) * linFactor + glideCurve * expFactor;
                currentFrequency = targetFrequency + (sourceFrequency - targetFrequency) * factor;
            }
        } else {
            currentFrequency = targetFrequency;
        }
        return currentFrequency;
    }

    float getCurrentFrequency() const { return currentFrequency; }
    int getActiveMidiNote() const { return activeMidiNote; }
    bool isAnyNoteHeld() const { return !noteStack.empty(); }

private:
    double sampleRate = 44100.0;
    float glideTimeSec = 0.05f;
    float glideSamplesTotal = 0.0f;
    float glideSamplesElapsed = 0.0f;
    float glideCurve = 1.0f; // 0.0 = Linear, 1.0 = Exponential

    std::vector<int> noteStack;
    float currentFrequency = 0.0f;
    float targetFrequency = 0.0f;
    float sourceFrequency = 0.0f;
    bool isGliding = false;
    int activeMidiNote = -1;
};
