#include "PluginProcessor.h"
#include "PluginEditor.h"

G0GAudioProcessorEditor::G0GAudioProcessorEditor(G0GAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      keyboardComponent(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    // Helper to configure rotary sliders
    auto initKnob = [this](juce::Slider& s, const juce::String& suffix) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 55, 15);
        s.setTextValueSuffix(suffix);
        s.setLookAndFeel(&customLookAndFeel);
        addAndMakeVisible(s);
    };

    // Helper to configure ComboBoxes
    auto initCombo = [this](juce::ComboBox& c, const juce::StringArray& items) {
        c.setLookAndFeel(&customLookAndFeel);
        c.addItemList(items, 1);
        addAndMakeVisible(c);
    };

    // 1. Envelope & Glide controls
    initKnob(glideTimeSlider, " ms");
    glideTimeAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "glide_time", glideTimeSlider);

    initKnob(glideCurveSlider, "");
    glideCurveAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "glide_curve", glideCurveSlider);

    initKnob(holdSlider, " ms");
    holdAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "ahd_hold", holdSlider);

    initKnob(decaySlider, " ms");
    decayAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "ahd_decay", decaySlider);

    initKnob(decayCurveSlider, "");
    decayCurveAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "decay_curve", decayCurveSlider);

    initKnob(pitchDepthSlider, " st");
    pitchDepthAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "pitch_depth", pitchDepthSlider);

    initKnob(pitchDecaySlider, " ms");
    pitchDecayAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "pitch_decay", pitchDecaySlider);

    // Sub Waveform morph
    initKnob(oscMorphSlider, "");
    oscMorphAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "osc_morph", oscMorphSlider);

    // 2. Transient Click controls
    initCombo(clickTypeCombo, juce::StringArray("Click Off", "White Noise", "Pink Noise"));
    clickTypeAttach = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, "click_type", clickTypeCombo);

    initKnob(clickLevelSlider, "");
    clickLevelAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "click_level", clickLevelSlider);

    // 3. Saturation controls
    initCombo(driveModeCombo, juce::StringArray("Bypass", "Tube Mode", "Clip Mode", "Foldback"));
    driveModeAttach = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, "drive_mode", driveModeCombo);

    initKnob(driveGainSlider, "x");
    driveGainAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "drive_gain", driveGainSlider);

    // 4. Filter & Volume controls
    initKnob(cutoffSlider, " Hz");
    cutoffAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "cutoff", cutoffSlider);

    initKnob(filterEnvDecaySlider, " ms");
    filterEnvDecayAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "filter_env_decay", filterEnvDecaySlider);

    initKnob(filterEnvDepthSlider, " Hz");
    filterEnvDepthAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "filter_env_depth", filterEnvDepthSlider);

    initKnob(masterVolSlider, " dB");
    masterVolAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "master_volume", masterVolSlider);

    // 5. Setup visual frames (Group Components)
    envelopeGroup.setText("SUB BASS SYNTH & GLIDE");
    addAndMakeVisible(envelopeGroup);

    clickGroup.setText("CLICK");
    addAndMakeVisible(clickGroup);

    saturationGroup.setText("HARMONICS");
    addAndMakeVisible(saturationGroup);

    filterGroup.setText("FILTER / MASTER");
    addAndMakeVisible(filterGroup);

    // Keyboard hooks
    keyboardComponent.setLookAndFeel(&customLookAndFeel);
    addAndMakeVisible(keyboardComponent);

    // Height expanded to 530 for 14 controls
    setSize(800, 530);

    // Start 60 FPS repaints for visualizer playhead tracking
    startTimerHz(60);
}

G0GAudioProcessorEditor::~G0GAudioProcessorEditor() {
    // Release custom LookAndFeel pointers to prevent dangling memory
    glideTimeSlider.setLookAndFeel(nullptr);
    glideCurveSlider.setLookAndFeel(nullptr);
    holdSlider.setLookAndFeel(nullptr);
    decaySlider.setLookAndFeel(nullptr);
    decayCurveSlider.setLookAndFeel(nullptr);
    pitchDepthSlider.setLookAndFeel(nullptr);
    pitchDecaySlider.setLookAndFeel(nullptr);
    oscMorphSlider.setLookAndFeel(nullptr);

    clickTypeCombo.setLookAndFeel(nullptr);
    clickLevelSlider.setLookAndFeel(nullptr);

    driveModeCombo.setLookAndFeel(nullptr);
    driveGainSlider.setLookAndFeel(nullptr);

    cutoffSlider.setLookAndFeel(nullptr);
    filterEnvDecaySlider.setLookAndFeel(nullptr);
    filterEnvDepthSlider.setLookAndFeel(nullptr);
    masterVolSlider.setLookAndFeel(nullptr);

    keyboardComponent.setLookAndFeel(nullptr);
}

void G0GAudioProcessorEditor::paint(juce::Graphics& g) {
    // 1. Dark Slate Premium Background
    g.fillAll(juce::Colour(0xFF0C0E12));

    // 2. Glowing Aqua Header Text
    g.setFont(juce::Font("Outfit", 18.0f, juce::Font::bold));
    g.setColour(juce::Colour(0xFF00F0FF));
    g.drawText("G0G", 20, 15, 100, 25, juce::Justification::left);

    g.setFont(juce::Font("Inter", 11.0f, juce::Font::bold));
    g.setColour(juce::Colour(0xFF5C6F84));
    g.drawText("// DYNAMIC 808 SYNTHESIZER", 65, 17, 300, 25, juce::Justification::left);

    g.setColour(juce::Colour(0xFF3C4B5E));
    g.drawText("EXPANDED ENGINE", 620, 17, 160, 25, juce::Justification::right);

    // 3. Draw central AHD / Filter Envelope Visualizer
    juce::Rectangle<int> visArea(20, 50, 760, 145);
    drawEnvelopeVisualizer(g, visArea);

    // 4. Panel labels (Knob title text overlays)
    g.setFont(juce::Font("Inter", 9.5f, juce::Font::bold));
    g.setColour(juce::Colour(0xFF8C9FB4));

    // Sub-bass panel labels - Row 1
    g.drawText("MORPH", 35, 317, 70, 12, juce::Justification::centred);
    g.drawText("GLIDE", 120, 317, 70, 12, juce::Justification::centred);
    g.drawText("HOLD", 205, 317, 70, 12, juce::Justification::centred);
    g.drawText("DECAY", 290, 317, 70, 12, juce::Justification::centred);

    // Sub-bass panel labels - Row 2
    g.drawText("D. CURVE", 35, 417, 70, 12, juce::Justification::centred);
    g.drawText("G. CURVE", 120, 417, 70, 12, juce::Justification::centred);
    g.drawText("P. DEPTH", 205, 417, 70, 12, juce::Justification::centred);
    g.drawText("P. DECAY", 290, 417, 70, 12, juce::Justification::centred);

    // Click section labels
    g.drawText("CLICK VOL", 420, 377, 75, 12, juce::Justification::centred);

    // Saturation section labels
    g.drawText("DRIVE", 525, 377, 75, 12, juce::Justification::centred);

    // Filter & Master section labels - Row 1
    g.drawText("LPF CUTOFF", 625, 317, 70, 12, juce::Justification::centred);
    g.drawText("FE. DEPTH", 700, 317, 70, 12, juce::Justification::centred);

    // Filter & Master section labels - Row 2
    g.drawText("FE. DECAY", 625, 417, 70, 12, juce::Justification::centred);
    g.drawText("MASTER", 700, 417, 70, 12, juce::Justification::centred);
}

void G0GAudioProcessorEditor::resized() {
    // 1. Position panel boxes
    envelopeGroup.setBounds(20, 210, 370, 245);
    clickGroup.setBounds(405, 210, 100, 245);
    saturationGroup.setBounds(510, 210, 100, 245);
    filterGroup.setBounds(615, 210, 165, 245);

    // 2. Position Sub Engine knobs (2-row grid)
    // Row 1
    oscMorphSlider.setBounds(35, 240, 70, 70);
    glideTimeSlider.setBounds(120, 240, 70, 70);
    holdSlider.setBounds(205, 240, 70, 70);
    decaySlider.setBounds(290, 240, 70, 70);

    // Row 2
    decayCurveSlider.setBounds(35, 340, 70, 70);
    glideCurveSlider.setBounds(120, 340, 70, 70);
    pitchDepthSlider.setBounds(205, 340, 70, 70);
    pitchDecaySlider.setBounds(290, 340, 70, 70);

    // 3. Position Click Controls
    clickTypeCombo.setBounds(412, 240, 86, 24);
    clickLevelSlider.setBounds(420, 300, 75, 75);

    // 4. Position Saturation Controls
    driveModeCombo.setBounds(517, 240, 86, 24);
    driveGainSlider.setBounds(525, 300, 75, 75);

    // 5. Position Filter & Master Controls (2x2 grid)
    // Row 1
    cutoffSlider.setBounds(625, 240, 70, 70);
    filterEnvDepthSlider.setBounds(700, 240, 70, 70);
    // Row 2
    filterEnvDecaySlider.setBounds(625, 340, 70, 70);
    masterVolSlider.setBounds(700, 340, 70, 70);

    // 6. Keyboard Layout at bottom
    keyboardComponent.setBounds(20, 470, 760, 42);
}

void G0GAudioProcessorEditor::timerCallback() {
    // Repaint visualizer playheads smoothly at 60 FPS
    repaint(20, 50, 760, 145);
}

void G0GAudioProcessorEditor::drawEnvelopeVisualizer(juce::Graphics& g, juce::Rectangle<int> area) {
    // Background fill
    g.setColour(juce::Colour(0xFF0F1217));
    g.fillRect(area);

    // Glowing border frame
    g.setColour(juce::Colour(0xFF00F0FF).withAlpha(0.12f));
    g.drawRect(area, 1.5f);

    // Draw grid lines
    g.setColour(juce::Colour(0xFF232A35).withAlpha(0.25f));
    int gridCount = 10;
    for (int i = 1; i < gridCount; ++i) {
        float ratio = static_cast<float>(i) / static_cast<float>(gridCount);
        g.drawVerticalLine(area.getX() + ratio * area.getWidth(), area.getY(), area.getBottom());
    }

    // Retrieve active envelope parameters
    float holdMs = audioProcessor.apvts.getRawParameterValue("ahd_hold")->load();
    float decayMs = audioProcessor.apvts.getRawParameterValue("ahd_decay")->load();
    float decayShape = audioProcessor.apvts.getRawParameterValue("decay_curve")->load();
    
    float filterEnvDepth = audioProcessor.apvts.getRawParameterValue("filter_env_depth")->load();
    float filterEnvDecayMs = audioProcessor.apvts.getRawParameterValue("filter_env_decay")->load();

    float totalMs = holdMs + decayMs;
    if (totalMs < 100.0f) {
        totalMs = 100.0f;
    }

    int leftOffset = area.getX();
    int topOffset = area.getY() + 15;
    int graphHeight = area.getHeight() - 25;

    // --- DRAW FILTER ENVELOPE (Neon Magenta) ---
    if (filterEnvDepth > 0.0f && filterEnvDecayMs > 0.0f) {
        juce::Path filterPath;
        // Start filter envelope height normalized to max 5000Hz depth
        float depthRatio = filterEnvDepth / 5000.0f;
        float startHeight = graphHeight * depthRatio;
        
        filterPath.startNewSubPath(leftOffset, area.getBottom() - 10 - startHeight);

        // Exponential decay of the filter sweep
        float wFDecay = (filterEnvDecayMs / totalMs) * static_cast<float>(area.getWidth());
        int steps = 120;
        for (int j = 0; j <= steps; ++j) {
            float ratio = static_cast<float>(j) / static_cast<float>(steps);
            float amp = std::exp(-5.0f * ratio);
            
            float x = leftOffset + ratio * wFDecay;
            float y = area.getBottom() - 10.0f - startHeight * amp;
            
            // Clamp horizontal limit to visualizer boundaries
            if (x <= area.getRight()) {
                filterPath.lineTo(x, y);
            }
        }
        
        // Draw tail flat line to the right boundary
        float lastX = leftOffset + wFDecay;
        if (lastX < area.getRight()) {
            filterPath.lineTo(area.getRight(), area.getBottom() - 10);
        }

        // Fill under filter path
        juce::Path filterFillPath = filterPath;
        filterFillPath.lineTo(area.getRight(), area.getBottom() - 10);
        filterFillPath.lineTo(leftOffset, area.getBottom() - 10);
        filterFillPath.closeSubPath();

        juce::ColourGradient fGrad(juce::Colour(0xFFFF00D2).withAlpha(0.06f), leftOffset, topOffset,
                                   juce::Colour(0xFFFF00D2).withAlpha(0.0f), leftOffset, area.getBottom() - 10, false);
        g.setGradientFill(fGrad);
        g.fillPath(filterFillPath);

        // Stroke filter path
        g.setColour(juce::Colour(0xFFFF00D2).withAlpha(0.7f));
        g.strokePath(filterPath, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // --- DRAW AMPLITUDE ENVELOPE (Neon Aqua) ---
    float wHold = (holdMs / totalMs) * static_cast<float>(area.getWidth());
    float wDecay = static_cast<float>(area.getWidth()) - wHold;

    juce::Path ampPath;
    ampPath.startNewSubPath(leftOffset, area.getBottom() - 10);
    ampPath.lineTo(leftOffset, topOffset); // Instant attack
    ampPath.lineTo(leftOffset + wHold, topOffset); // Hold line

    int steps = 150;
    for (int j = 0; j <= steps; ++j) {
        float ratio = static_cast<float>(j) / static_cast<float>(steps);
        float amp = 0.0f;
        
        if (decayShape == 0.0f) {
            amp = std::exp(-5.0f * ratio);
        } else if (decayShape < 0.0f) {
            float lin = 1.0f - ratio;
            float expVal = std::exp(-5.0f * ratio);
            amp = (1.0f + decayShape) * expVal - decayShape * lin;
        } else {
            float rate = 5.0f + decayShape * 7.0f;
            amp = std::exp(-rate * ratio);
        }

        float x = leftOffset + wHold + ratio * wDecay;
        float y = area.getBottom() - 10.0f - graphHeight * amp;
        ampPath.lineTo(x, y);
    }

    // Fill under amplitude path
    juce::Path ampFillPath = ampPath;
    ampFillPath.lineTo(area.getRight(), area.getBottom() - 10);
    ampFillPath.lineTo(leftOffset, area.getBottom() - 10);
    ampFillPath.closeSubPath();

    juce::ColourGradient ampGrad(juce::Colour(0xFF00F0FF).withAlpha(0.15f), leftOffset, topOffset,
                                 juce::Colour(0xFF00F0FF).withAlpha(0.0f), leftOffset, area.getBottom() - 10, false);
    g.setGradientFill(ampGrad);
    g.fillPath(ampFillPath);

    // Glow underlay and main stroke
    g.setColour(juce::Colour(0xFF00F0FF).withAlpha(0.10f));
    g.strokePath(ampPath, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(juce::Colour(0xFF00F0FF));
    g.strokePath(ampPath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // --- DRAW ACTIVE VOICE PLAYHEADS ---
    if (audioProcessor.isVoiceActive()) {
        float pAmpHold = audioProcessor.getHoldProgress();
        float pAmpDecay = audioProcessor.getDecayProgress();
        float pFilter = audioProcessor.getFilterEnvProgress();

        // 1. Draw Filter Playhead Dot
        if (pFilter > 0.0f && pFilter < 1.0f && filterEnvDepth > 0.0f && filterEnvDecayMs > 0.0f) {
            float wFDecay = (filterEnvDecayMs / totalMs) * static_cast<float>(area.getWidth());
            float fX = leftOffset + pFilter * wFDecay;
            
            float depthRatio = filterEnvDepth / 5000.0f;
            float startHeight = graphHeight * depthRatio;
            float fY = area.getBottom() - 10.0f - startHeight * std::exp(-5.0f * pFilter);
            
            if (fX <= area.getRight()) {
                g.setColour(juce::Colour(0xFFFF00D2).withAlpha(0.4f));
                g.fillEllipse(fX - 5.0f, fY - 5.0f, 10.0f, 10.0f);
                g.setColour(juce::Colours::white);
                g.fillEllipse(fX - 2.0f, fY - 2.0f, 4.0f, 4.0f);
            }
        }

        // 2. Draw Amplitude Playhead Dot
        float playheadX = leftOffset;
        float playheadY = topOffset;

        if (pAmpHold > 0.0f && pAmpDecay == 0.0f) {
            playheadX = leftOffset + pAmpHold * wHold;
            playheadY = topOffset;
        } else if (pAmpDecay > 0.0f) {
            playheadX = leftOffset + wHold + pAmpDecay * wDecay;
            
            float ampVal = 0.0f;
            if (decayShape == 0.0f) {
                ampVal = std::exp(-5.0f * pAmpDecay);
            } else if (decayShape < 0.0f) {
                float lin = 1.0f - pAmpDecay;
                float expVal = std::exp(-5.0f * pAmpDecay);
                ampVal = (1.0f + decayShape) * expVal - decayShape * lin;
            } else {
                float rate = 5.0f + decayShape * 7.0f;
                ampVal = std::exp(-rate * pAmpDecay);
            }
            playheadY = area.getBottom() - 10.0f - graphHeight * ampVal;
        }

        g.setColour(juce::Colour(0xFF00F0FF).withAlpha(0.4f));
        g.fillEllipse(playheadX - 6.0f, playheadY - 6.0f, 12.0f, 12.0f);
        g.setColour(juce::Colours::white);
        g.fillEllipse(playheadX - 3.0f, playheadY - 3.0f, 6.0f, 6.0f);
    }

    // --- OVERLAY TEXT ---
    g.setFont(juce::Font("Inter", 9.0f, juce::Font::bold));
    g.setColour(juce::Colour(0xFF5C6F84));

    g.drawText("HOLD: " + juce::String(holdMs, 0) + " ms", leftOffset + 12, area.getY() + 10, 100, 12, juce::Justification::left);
    g.drawText("DECAY: " + juce::String(decayMs, 0) + " ms", leftOffset + 120, area.getY() + 10, 100, 12, juce::Justification::left);
    
    // Draw filter info in magenta if active
    if (filterEnvDepth > 0.0f && filterEnvDecayMs > 0.0f) {
        g.setColour(juce::Colour(0xFFFF00D2));
        g.drawText("F.ENV: +" + juce::String(filterEnvDepth, 0) + "Hz (" + juce::String(filterEnvDecayMs, 0) + "ms)", leftOffset + 240, area.getY() + 10, 180, 12, juce::Justification::left);
    }

    int activeNote = audioProcessor.getActiveMidiNote();
    if (activeNote >= 0) {
        float freq = audioProcessor.getGlideFrequency();
        juce::String noteName = juce::MidiMessage::getMidiNoteName(activeNote, true, true, 3);
        g.setColour(juce::Colour(0xFF00F0FF));
        g.drawText("NOTE: " + noteName + " (" + juce::String(activeNote) + ")", area.getRight() - 250, area.getY() + 10, 110, 12, juce::Justification::right);
        g.drawText("FREQ: " + juce::String(freq, 1) + " Hz", area.getRight() - 120, area.getY() + 10, 100, 12, juce::Justification::right);
    } else {
        g.setColour(juce::Colour(0xFF5C6F84));
        g.drawText("VOICE: IDLE", area.getRight() - 120, area.getY() + 10, 100, 12, juce::Justification::right);
    }
}
