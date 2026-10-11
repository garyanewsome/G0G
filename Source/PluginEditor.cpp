#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace {
constexpr int kBaseWidth = 1260;
constexpr int kBaseHeight = 470;
constexpr int kHeaderHeight = 58;
constexpr int kVisualizerHeight = 162;
constexpr int kKeyboardHeight = 62;

juce::String timeText(double ms) {
    return ms < 1000.0 ? juce::String(juce::roundToInt(ms)) + " ms"
                       : juce::String(ms / 1000.0, 2) + " s";
}
double leadingNumber(const juce::String &t) {
    return t.retainCharacters("+-.0123456789").getDoubleValue();
}
double timeFromText(const juce::String &t) {
    const double n = leadingNumber(t);
    return (t.containsIgnoreCase("s") && !t.containsIgnoreCase("ms")) ? n * 1000.0 : n;
}
juce::String hzText(double hz) {
    return hz < 1000.0 ? juce::String(juce::roundToInt(hz)) + " Hz"
                       : juce::String(hz / 1000.0, 2) + " kHz";
}
double hzFromText(const juce::String &t) {
    const double n = leadingNumber(t);
    return t.containsIgnoreCase("k") ? n * 1000.0 : n;
}
juce::String percentText(double v) { return juce::String(juce::roundToInt(v * 100.0)) + " %"; }
double percentFromText(const juce::String &t) { return leadingNumber(t) / 100.0; }
} // namespace

//==============================================================================
G0GAudioProcessorEditor::G0GAudioProcessorEditor(G0GAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      keyboardComponent(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&lookAndFeel);

    // ---- Sub oscillator
    addKnob(morph, "osc_morph", "MORPH",
            "Sub waveform: sine (pure, deep), triangle (a little harmonic bite) "
            "or square (hard and buzzy). Turn between them to blend.");
    morph.slider.textFromValueFunction = [](double v) {
        static const char* n[] = {"SINE", "TRI", "SQR"};
        return juce::String(n[juce::jlimit(0, 2, (int) std::round(v))]);
    };

    addKnob(tune, "tune", "TUNE",
            "Tune: transposes the whole instrument in semitones. Use -12 or "
            "-24 to drop the bass an octave or two; +12 is an octave up.");
    tune.slider.textFromValueFunction = [](double v) {
        const int n = juce::roundToInt(v);
        return (n > 0 ? "+" : "") + juce::String(n) + " st";
    };
    addKnob(fineTune, "fine_tune", "FINE",
            "Fine tune: nudges the pitch in cents (100 cents = 1 semitone) "
            "to match a sample or another instrument.");
    fineTune.slider.textFromValueFunction = [](double v) {
        const int n = juce::roundToInt(v);
        return (n > 0 ? "+" : "") + juce::String(n) + " ct";
    };

    // ---- Glide
    addChoice(legatoMode, "legato", juce::StringArray("RETRIG", "LEGATO"),
              "Note handling: RETRIG punches on every note. LEGATO lets "
              "overlapping notes slide without re-punching, like a played "
              "808 line.");
    addKnob(glideTime, "glide_time", "TIME",
            "Glide time: how long the pitch takes to slide from the previous "
            "note to the next one. Zero jumps straight to the new note.");
    glideTime.slider.textFromValueFunction = timeText;
    glideTime.slider.valueFromTextFunction = timeFromText;
    addKnob(glideCurve, "glide_curve", "CURVE",
            "Glide shape: 0% slides at an even speed, 100% starts fast and "
            "settles slowly into the new note.");
    glideCurve.slider.textFromValueFunction = percentText;
    glideCurve.slider.valueFromTextFunction = percentFromText;

    // ---- Punch (pitch envelope)
    addKnob(pitchDepth, "pitch_depth", "DEPTH",
            "Punch depth: how many semitones above the note the pitch starts, "
            "before dropping onto it. This is the 'thump' of the 808.");
    pitchDepth.slider.textFromValueFunction = [](double v) {
        return juce::String(juce::roundToInt(v)) + " st";
    };
    addKnob(pitchDecay, "pitch_decay", "SPEED",
            "Punch speed: how quickly the pitch drops onto the note. Short is "
            "a tight click, longer is a boomy sweep.");
    pitchDecay.slider.textFromValueFunction = timeText;
    pitchDecay.slider.valueFromTextFunction = timeFromText;

    // ---- Amp envelope
    addKnob(hold, "ahd_hold", "HOLD",
            "Hold: how long the sound stays at full level before it starts to "
            "fade. Longer holds sound fuller and more sustained.");
    hold.slider.textFromValueFunction = timeText;
    hold.slider.valueFromTextFunction = timeFromText;
    addKnob(decay, "ahd_decay", "DECAY",
            "Decay: how long the tail takes to fade to silence after the hold.");
    decay.slider.textFromValueFunction = timeText;
    decay.slider.valueFromTextFunction = timeFromText;
    addKnob(decayCurve, "decay_curve", "SHAPE",
            "Decay shape: left is a straight linear fade, centre is the "
            "classic exponential tail, right is tighter and snappier.");
    decayCurve.slider.textFromValueFunction = percentText;
    decayCurve.slider.valueFromTextFunction = percentFromText;

    addChoice(gateMode, "gate_mode", juce::StringArray("ONE-SHOT", "GATE"),
              "ONE-SHOT: every note rings its full decay, like a drum "
              "machine. GATE: the note length shapes the bass - when the key "
              "is released the sound fades out over RELEASE.");
    addKnob(release, "release", "REL",
            "Release: in GATE mode, how quickly the bass fades after you "
            "let go of the key. Short is tight, long is smooth.");
    release.slider.textFromValueFunction = timeText;
    release.slider.valueFromTextFunction = timeFromText;

    // ---- Click
    addChoice(clickType, "click_type", juce::StringArray("OFF", "WHITE", "PINK"),
              "Click type: a very short burst of noise on every note for "
              "attack, so the bass is audible on small speakers.");
    addKnob(clickLevel, "click_level", "LEVEL",
            "Click level: how loud the noise burst is.");
    clickLevel.slider.textFromValueFunction = percentText;
    clickLevel.slider.valueFromTextFunction = percentFromText;

    // ---- Drive
    addChoice(driveMode, "drive_mode", juce::StringArray("BYPASS", "TUBE", "CLIP", "FOLD"),
              "Drive character: tube is warm and thick, clip is aggressive "
              "and growling, fold is harsh and digital.");
    addKnob(driveGain, "drive_gain", "AMOUNT",
            "Drive amount: how hard the signal is pushed into the saturation.");
    driveGain.slider.textFromValueFunction = [](double v) { return juce::String(v, 1) + " x"; };

    // ---- Filter
    addKnob(cutoff, "cutoff", "CUTOFF",
            "Cutoff: frequencies above this are removed. Lower is darker and "
            "rounder, higher lets the harmonics through.");
    cutoff.slider.textFromValueFunction = hzText;
    cutoff.slider.valueFromTextFunction = hzFromText;
    addKnob(filterEnvDepth, "filter_env_depth", "SWEEP",
            "Filter sweep: how far the cutoff is thrown open at the start of "
            "each note, then closes back down.");
    filterEnvDepth.slider.textFromValueFunction = hzText;
    filterEnvDepth.slider.valueFromTextFunction = hzFromText;
    addKnob(filterEnvDecay, "filter_env_decay", "SPEED",
            "Sweep speed: how long the filter takes to close after being "
            "thrown open.");
    filterEnvDecay.slider.textFromValueFunction = timeText;
    filterEnvDecay.slider.valueFromTextFunction = timeFromText;

    // ---- Output
    addKnob(masterVol, "master_volume", "VOL", "Master output level in decibels.");
    masterVol.slider.textFromValueFunction = [](double v) { return juce::String(v, 1) + " dB"; };

    // ---- Performance wheels (not attached to parameters)
    auto setupWheel = [this](Knob& k, const juce::String& title, double lo, double hi,
                             const juce::String& tip) {
        k.slider.setSliderStyle(juce::Slider::LinearVertical);
        k.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        k.slider.setRange(lo, hi, 0.0);
        k.slider.setTooltip(tip);
        k.slider.getProperties().set(G0GLookAndFeel::wheelProperty, true);
        addAndMakeVisible(k.slider);
        k.label.setText(title, juce::dontSendNotification);
        k.label.setJustificationType(juce::Justification::centred);
        k.label.setTooltip(tip);
        addAndMakeVisible(k.label);
    };
    setupWheel(pitchWheel, "PITCH", -1.0, 1.0,
               "Pitch wheel: bends the note up or down by up to 2 semitones. "
               "Springs back to centre when released. Hardware wheels work too.");
    setupWheel(modWheel, "MOD", 0.0, 1.0,
               "Mod wheel: opens the filter by up to 4 kHz for a brighter, "
               "more aggressive tone. Stays where you leave it.");
    pitchWheel.slider.setDoubleClickReturnValue(true, 0.0);
    pitchWheel.slider.onValueChange = [this] {
        audioProcessor.pitchBend.store((float) pitchWheel.slider.getValue());
    };
    pitchWheel.slider.onDragEnd = [this] { pitchWheel.slider.setValue(0.0); };
    modWheel.slider.onValueChange = [this] {
        audioProcessor.modWheel.store((float) modWheel.slider.getValue());
    };
    modWheel.slider.setValue(audioProcessor.modWheel.load(), juce::dontSendNotification);

    // Formatters were assigned after the attachments, so refresh the read-outs.
    for (auto* k : {&morph, &tune, &fineTune, &release, &glideTime, &glideCurve, &pitchDepth, &pitchDecay, &hold, &decay,
                    &decayCurve, &clickLevel, &driveGain, &cutoff, &filterEnvDepth,
                    &filterEnvDecay, &masterVol})
        k->slider.updateText();

    // ---- Sections (weights are relative widths)
    sections = {
        {"PERF",   nullptr,      {&pitchWheel, &modWheel}, 1.0f, {}},
        {"SUB",    nullptr,      {&morph, &tune, &fineTune}, 1.0f, {}},
        {"GLIDE",  &legatoMode,  {&glideTime, &glideCurve}, 1.0f, {}},
        {"PUNCH",  nullptr,      {&pitchDepth, &pitchDecay}, 1.0f, {}},
        {"AMP",    &gateMode,    {&hold, &decay, &decayCurve, &release}, 1.0f, {}},
        {"CLICK",  &clickType,   {&clickLevel}, 1.7f, {}},
        {"DRIVE",  &driveMode,   {&driveGain}, 1.7f, {}},
        {"FILTER", nullptr,      {&cutoff, &filterEnvDepth, &filterEnvDecay}, 1.0f, {}},
        {"OUT",    nullptr,      {&masterVol}, 1.0f, {}},
    };

    // ---- Preset browser
    presetBox.setTextWhenNothingSelected("- PRESET -");
    presetBox.setTooltip("Choose a factory or saved preset.");
    presetBox.onChange = [this] {
        const int id = presetBox.getSelectedId();
        const int nFactory = (int) Presets::factory().size();
        if (id >= 1 && id <= nFactory)
            Presets::applyFactory(audioProcessor, id - 1);
        else if (id > 1000 && id - 1001 < userPresetFiles.size())
            Presets::loadUser(audioProcessor, userPresetFiles[id - 1001]);
    };
    addAndMakeVisible(presetBox);

    prevPresetButton.setButtonText("<");
    nextPresetButton.setButtonText(">");
    savePresetButton.setButtonText("SAVE");
    prevPresetButton.setTooltip("Previous preset.");
    nextPresetButton.setTooltip("Next preset.");
    savePresetButton.setTooltip("Save the current sound as a preset in your user "
                                "Application Support folder.");
    prevPresetButton.onClick = [this] { stepPreset(-1); };
    nextPresetButton.onClick = [this] { stepPreset(1); };
    savePresetButton.onClick = [this] { promptSavePreset(); };
    addAndMakeVisible(prevPresetButton);
    addAndMakeVisible(nextPresetButton);
    addAndMakeVisible(savePresetButton);
    refreshPresetList();

    // ---- Keyboard
    keyboardComponent.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
                                G0GLookAndFeel::red.withAlpha(0.7f));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
                                G0GLookAndFeel::red.withAlpha(0.3f));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xff1c1214));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colours::black);
    keyboardComponent.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId,
                                G0GLookAndFeel::red.withAlpha(0.4f));
    keyboardComponent.setColour(juce::MidiKeyboardComponent::textLabelColourId, G0GLookAndFeel::dimText);
    keyboardComponent.setAvailableRange(24, 96);
    keyboardComponent.setScrollButtonsVisible(false);
    addAndMakeVisible(keyboardComponent);

    setResizable(true, true);
    setResizeLimits(kBaseWidth * 3 / 4, kBaseHeight * 3 / 4, kBaseWidth * 2, kBaseHeight * 2);
    getConstrainer()->setFixedAspectRatio((double) kBaseWidth / kBaseHeight);
    setSize(kBaseWidth, kBaseHeight);

    startTimerHz(30);
}

G0GAudioProcessorEditor::~G0GAudioProcessorEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void G0GAudioProcessorEditor::addKnob(Knob& k, const juce::String& paramId,
                                      const juce::String& title, const juce::String& tooltip) {
    k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                                 juce::MathConstants<float>::pi * 2.8f, true);
    k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    k.slider.setTooltip(tooltip);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, paramId, k.slider);
    addAndMakeVisible(k.slider);

    k.label.setText(title, juce::dontSendNotification);
    k.label.setJustificationType(juce::Justification::centred);
    k.label.setTooltip(tooltip);
    addAndMakeVisible(k.label);
}

void G0GAudioProcessorEditor::addChoice(Choice& c, const juce::String& paramId,
                                        const juce::StringArray& items, const juce::String& tooltip) {
    c.box.addItemList(items, 1);
    c.box.setTooltip(tooltip);
    c.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, paramId, c.box);
    addAndMakeVisible(c.box);
}

//==============================================================================
void G0GAudioProcessorEditor::refreshPresetList(const juce::String& selectName) {
    presetBox.clear(juce::dontSendNotification);
    int id = 1;
    for (const auto& p : Presets::factory())
        presetBox.addItem(p.name, id++);

    userPresetFiles = Presets::userPresetFiles();
    if (userPresetFiles.size() > 0) {
        presetBox.addSeparator();
        presetBox.addSectionHeading("USER");
        int uid = 1001;
        for (const auto& f : userPresetFiles) {
            presetBox.addItem(f.getFileNameWithoutExtension(), uid);
            if (selectName.isNotEmpty() &&
                f.getFileNameWithoutExtension() == juce::File::createLegalFileName(selectName))
                presetBox.setSelectedId(uid, juce::dontSendNotification);
            ++uid;
        }
    }
}

void G0GAudioProcessorEditor::stepPreset(int delta) {
    const int count = presetBox.getNumItems();
    if (count == 0)
        return;
    int index = presetBox.getSelectedItemIndex();
    for (int n = 0; n < count; ++n) {
        index = (index + delta + count) % count;
        if (presetBox.getItemId(index) != 0) {
            presetBox.setSelectedItemIndex(index, juce::sendNotificationSync);
            return;
        }
    }
}

void G0GAudioProcessorEditor::promptSavePreset() {
    auto* w = new juce::AlertWindow("Save preset", "Name this sound:",
                                    juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor("name", "My 808");
    w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->enterModalState(
        true,
        juce::ModalCallbackFunction::create([this, w](int result) {
            if (result == 1) {
                const auto name = w->getTextEditorContents("name").trim();
                if (name.isNotEmpty() && Presets::saveUser(audioProcessor, name))
                    refreshPresetList(name);
            }
        }),
        true);
}

void G0GAudioProcessorEditor::timerCallback() {
    // Mirror hardware wheel movement; keep the visualizer playheads moving.
    if (!modWheel.slider.isMouseButtonDown())
        modWheel.slider.setValue(audioProcessor.modWheel.load(), juce::dontSendNotification);
    if (!pitchWheel.slider.isMouseButtonDown())
        pitchWheel.slider.setValue(audioProcessor.pitchBend.load(), juce::dontSendNotification);
    repaint(visualizerArea);
}

//==============================================================================
void G0GAudioProcessorEditor::paint(juce::Graphics& g) {
    uiScale = (float) getWidth() / (float) kBaseWidth;
    const auto red = G0GLookAndFeel::red;

    g.fillAll(G0GLookAndFeel::background);

    // Faint red grid.
    g.setColour(red.withAlpha(0.035f));
    const float step = 24.0f * uiScale;
    for (float x = 0; x < (float) getWidth(); x += step)
        g.drawVerticalLine((int) x, 0.0f, (float) getHeight());
    for (float y = 0; y < (float) getHeight(); y += step)
        g.drawHorizontalLine((int) y, 0.0f, (float) getWidth());

    // Title
    const auto titleArea = juce::Rectangle<int>((int) (20 * uiScale), (int) (6 * uiScale),
                                                (int) (150 * uiScale), (int) (44 * uiScale));
    g.setFont(G0GLookAndFeel::monoFont(36.0f * uiScale, true));
    g.setColour(red.withAlpha(0.35f));
    g.drawText("G0G", titleArea.translated(2, 2), juce::Justification::centredLeft, false);
    g.setColour(red);
    g.drawText("G0G", titleArea, juce::Justification::centredLeft, false);
    g.setFont(G0GLookAndFeel::monoFont(11.0f * uiScale, true));
    g.setColour(G0GLookAndFeel::dimText);
    g.drawText("// DYNAMIC 808", juce::Rectangle<int>((int) (150 * uiScale), (int) (24 * uiScale),
                                                       (int) (200 * uiScale), (int) (18 * uiScale)),
               juce::Justification::left, false);

    // Section panels: chamfered corners, scanlines, header tag.
    const float ch = 10.0f * uiScale;
    for (const auto& s : sections) {
        const auto r = s.bounds;
        juce::Path p;
        p.startNewSubPath(r.getX() + ch, r.getY());
        p.lineTo(r.getRight(), r.getY());
        p.lineTo(r.getRight(), r.getBottom() - ch);
        p.lineTo(r.getRight() - ch, r.getBottom());
        p.lineTo(r.getX(), r.getBottom());
        p.lineTo(r.getX(), r.getY() + ch);
        p.closeSubPath();

        g.setColour(G0GLookAndFeel::panel);
        g.fillPath(p);
        g.setColour(red.withAlpha(0.12f));
        g.strokePath(p, juce::PathStrokeType(4.0f * uiScale));
        g.setColour(red.withAlpha(0.7f));
        g.strokePath(p, juce::PathStrokeType(1.0f));

        g.setColour(juce::Colours::black.withAlpha(0.2f));
        for (float y = r.getY() + 2; y < r.getBottom(); y += 3.0f)
            g.drawHorizontalLine((int) y, r.getX() + 1, r.getRight() - 1);

        g.setColour(red);
        g.setFont(G0GLookAndFeel::monoFont(11.0f * uiScale, true));
        g.drawText(s.title, r.withHeight(20.0f * uiScale).reduced(12.0f * uiScale, 0),
                   juce::Justification::centredLeft, false);
    }

    // Visualizer is drawn in base coordinates and scaled with the window.
    {
        juce::Graphics::ScopedSaveState ss(g);
        g.addTransform(juce::AffineTransform::scale(uiScale));
        drawEnvelopeVisualizer(g, juce::Rectangle<int>(
            12, kHeaderHeight + 4, kBaseWidth - 24, kVisualizerHeight));
    }
}

void G0GAudioProcessorEditor::resized() {
    uiScale = (float) getWidth() / (float) kBaseWidth;
    const float s = uiScale;
    auto area = getLocalBounds();

    keyboardComponent.setBounds(area.removeFromBottom((int) (kKeyboardHeight * s)).reduced((int) (12 * s), (int) (6 * s)));
    keyboardComponent.setKeyWidth((float) keyboardComponent.getWidth() / 43.0f); // C1..C7 = 43 white keys

    auto header = area.removeFromTop((int) (kHeaderHeight * s));
    const int padY = (int) (13 * s);
    savePresetButton.setBounds(header.removeFromRight((int) (72 * s)).reduced((int) (3 * s), padY));
    nextPresetButton.setBounds(header.removeFromRight((int) (32 * s)).reduced((int) (2 * s), padY));
    presetBox.setBounds(header.removeFromRight((int) (230 * s)).reduced((int) (2 * s), padY));
    prevPresetButton.setBounds(header.removeFromRight((int) (32 * s)).reduced((int) (2 * s), padY));

    visualizerArea = juce::Rectangle<int>((int) (12 * s), (int) ((kHeaderHeight + 4) * s),
                                          (int) ((kBaseWidth - 24) * s), (int) (kVisualizerHeight * s));
    area.removeFromTop((int) ((kVisualizerHeight + 8) * s));

    auto body = area.reduced((int) (12 * s), (int) (4 * s));
    const int gap = (int) (8 * s);

    float totalWeight = 0.0f;
    for (const auto& sec : sections)
        totalWeight += sec.weight * (float) sec.knobs.size();
    const float unit = (float) (body.getWidth() - gap * ((int) sections.size() - 1)) / totalWeight;

    float x = (float) body.getX();
    for (auto& sec : sections) {
        const float w = unit * sec.weight * (float) sec.knobs.size();
        sec.bounds = {x, (float) body.getY(), w, (float) body.getHeight()};

        auto inner = sec.bounds.toNearestInt().reduced((int) (6 * s), 0);
        inner.removeFromTop((int) (22 * s));
        inner.removeFromBottom((int) (6 * s));

        if (sec.choice != nullptr)
            sec.choice->box.setBounds(inner.removeFromTop((int) (24 * s)).reduced((int) (2 * s), 0));

        const int cw = inner.getWidth() / (int) sec.knobs.size();
        for (auto* k : sec.knobs) {
            auto col = inner.removeFromLeft(cw);
            k->label.setBounds(col.removeFromTop((int) (18 * s)));
            k->label.setFont(G0GLookAndFeel::monoFont(11.0f * s, true));
            k->slider.setBounds(col);
            if (k->slider.getTextBoxPosition() != juce::Slider::NoTextBox)
                k->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false,
                                          juce::jmin(cw - 2, (int) (76 * s)), (int) (18 * s));
        }
        x += w + (float) gap;
    }
}

void G0GAudioProcessorEditor::drawEnvelopeVisualizer(juce::Graphics& g, juce::Rectangle<int> area) {
    // Background fill
    g.setColour(juce::Colour(0xFF0A0708));
    g.fillRect(area);

    // Glowing border frame
    g.setColour(G0GLookAndFeel::red.withAlpha(0.12f));
    g.drawRect(area.toFloat(), 1.5f);

    // Draw grid lines
    g.setColour(juce::Colour(0xFF3A2228).withAlpha(0.25f));
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

        juce::ColourGradient fGrad(G0GLookAndFeel::amber.withAlpha(0.06f), leftOffset, topOffset,
                                   G0GLookAndFeel::amber.withAlpha(0.0f), leftOffset, area.getBottom() - 10, false);
        g.setGradientFill(fGrad);
        g.fillPath(filterFillPath);

        // Stroke filter path
        g.setColour(G0GLookAndFeel::amber.withAlpha(0.7f));
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

    juce::ColourGradient ampGrad(G0GLookAndFeel::red.withAlpha(0.15f), leftOffset, topOffset,
                                 G0GLookAndFeel::red.withAlpha(0.0f), leftOffset, area.getBottom() - 10, false);
    g.setGradientFill(ampGrad);
    g.fillPath(ampFillPath);

    // Glow underlay and main stroke
    g.setColour(G0GLookAndFeel::red.withAlpha(0.10f));
    g.strokePath(ampPath, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(G0GLookAndFeel::red);
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
                g.setColour(G0GLookAndFeel::amber.withAlpha(0.4f));
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

        g.setColour(G0GLookAndFeel::red.withAlpha(0.4f));
        g.fillEllipse(playheadX - 6.0f, playheadY - 6.0f, 12.0f, 12.0f);
        g.setColour(juce::Colours::white);
        g.fillEllipse(playheadX - 3.0f, playheadY - 3.0f, 6.0f, 6.0f);
    }

    // --- OVERLAY TEXT ---
    g.setFont(G0GLookAndFeel::monoFont(9.0f, true));
    g.setColour(G0GLookAndFeel::dimText);

    g.drawText("HOLD: " + juce::String(holdMs, 0) + " ms", leftOffset + 12, area.getY() + 10, 100, 12, juce::Justification::left);
    g.drawText("DECAY: " + juce::String(decayMs, 0) + " ms", leftOffset + 120, area.getY() + 10, 100, 12, juce::Justification::left);
    
    // Draw filter info in magenta if active
    if (filterEnvDepth > 0.0f && filterEnvDecayMs > 0.0f) {
        g.setColour(G0GLookAndFeel::amber);
        g.drawText("F.ENV: +" + juce::String(filterEnvDepth, 0) + "Hz (" + juce::String(filterEnvDecayMs, 0) + "ms)", leftOffset + 240, area.getY() + 10, 180, 12, juce::Justification::left);
    }

    int activeNote = audioProcessor.getActiveMidiNote();
    if (activeNote >= 0) {
        float freq = audioProcessor.getGlideFrequency();
        juce::String noteName = juce::MidiMessage::getMidiNoteName(activeNote, true, true, 3);
        g.setColour(G0GLookAndFeel::red);
        g.drawText("NOTE: " + noteName + " (" + juce::String(activeNote) + ")", area.getRight() - 250, area.getY() + 10, 110, 12, juce::Justification::right);
        g.drawText("FREQ: " + juce::String(freq, 1) + " Hz", area.getRight() - 120, area.getY() + 10, 100, 12, juce::Justification::right);
    } else {
        g.setColour(G0GLookAndFeel::dimText);
        g.drawText("VOICE: IDLE", area.getRight() - 120, area.getY() + 10, 100, 12, juce::Justification::right);
    }
}
