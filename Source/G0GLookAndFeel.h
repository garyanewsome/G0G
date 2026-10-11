#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// G0G look and feel: near-black with hot red, metal knobs, ribbed wheels.
//==============================================================================
class G0GLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static inline const juce::Colour background { 0xff060608 };
    static inline const juce::Colour panel      { 0xff100b0d };
    static inline const juce::Colour red        { 0xffff1f3d };   // primary
    static inline const juce::Colour darkRed    { 0xff7a0e1e };
    static inline const juce::Colour amber      { 0xffff8a2b };   // filter envelope
    static inline const juce::Colour dimText    { 0xff9a8a8e };

    // Sliders carrying this property are drawn as performance wheels.
    static inline const juce::Identifier wheelProperty { "g0gWheel" };

    G0GLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, background);
        setColour (juce::Label::textColourId, juce::Colour (0xffe8dcdf));
        setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xffffe3e7));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha (0.45f));
        setColour (juce::Slider::textBoxOutlineColourId, red.withAlpha (0.22f));
        setColour (juce::Slider::textBoxHighlightColourId, red.withAlpha (0.5f));
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf0140a0d));
        setColour (juce::TooltipWindow::textColourId, juce::Colours::white);
        setColour (juce::TooltipWindow::outlineColourId, red);
        setColour (juce::TextButton::buttonColourId, panel);
        setColour (juce::TextButton::textColourOffId, red);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::ComboBox::backgroundColourId, panel);
        setColour (juce::ComboBox::outlineColourId, red.withAlpha (0.6f));
        setColour (juce::ComboBox::textColourId, juce::Colour (0xffffe3e7));
        setColour (juce::ComboBox::arrowColourId, red);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff120a0c));
        setColour (juce::PopupMenu::textColourId, juce::Colour (0xffffe3e7));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, red.withAlpha (0.35f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    }

    static juce::Font monoFont (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions ("Menlo", height,
                                              bold ? juce::Font::bold : juce::Font::plain));
    }

    juce::Font getLabelFont (juce::Label& l) override
    {
        const bool isSliderText = dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr;
        return monoFont (isSliderText ? 11.0f : 12.0f, true);
    }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return monoFont (12.0f, true); }
    juce::Font getComboBoxFont (juce::ComboBox&) override          { return monoFont (12.0f, true); }
    juce::Font getPopupMenuFont() override                          { return monoFont (13.0f, true); }

    //==========================================================================
    // Metal knob: tick ring, glowing value arc, shaded cap with a pointer.
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        const bool active = slider.isMouseOverOrDragging();
        const auto accent = active ? juce::Colour (0xffff5a70) : red;
        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;

        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (3.0f);
        const float d = juce::jmin (b.getWidth(), b.getHeight());
        const auto c = b.withSizeKeepingCentre (d, d).getCentre();
        const float R = d * 0.5f;

        // Tick ring
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
            const bool major = (i % 5 == 0);
            const float r0 = R - R * (major ? 0.15f : 0.09f), r1 = R;
            g.setColour (dimText.withAlpha (major ? 0.85f : 0.4f));
            g.drawLine (c.x + r0 * std::sin (a), c.y - r0 * std::cos (a),
                        c.x + r1 * std::sin (a), c.y - r1 * std::cos (a), major ? 1.4f : 1.0f);
        }

        const float arcR = R * 0.80f;
        const float trackW = juce::jmax(2.0f, R * 0.09f);
        const float angle = startAngle + sliderPos * (endAngle - startAngle);
        const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;

        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (darkRed.withAlpha (0.45f));
        g.strokePath (track, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (slider.isEnabled() && std::abs (angle - from) > 0.002f)
        {
            juce::Path v;
            v.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
            g.setColour (accent.withAlpha (0.16f));
            g.strokePath (v, juce::PathStrokeType (trackW * 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (accent.withAlpha (0.35f));
            g.strokePath (v, juce::PathStrokeType (trackW * 1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (accent);
            g.strokePath (v, juce::PathStrokeType (trackW * 0.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Cap
        const float capR = arcR * 0.80f;
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillEllipse (c.x - capR - 1.5f, c.y - capR + 1.0f, (capR + 1.5f) * 2.0f, (capR + 1.5f) * 2.0f);

        juce::ColourGradient cap (juce::Colour (0xff5a4a4e), c.x - capR * 0.5f, c.y - capR * 0.6f,
                                  juce::Colour (0xff120c0e), c.x + capR * 0.7f, c.y + capR * 0.8f, true);
        g.setGradientFill (cap);
        g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
        g.setColour (accent.withAlpha (active ? 0.9f : 0.55f));
        g.drawEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f, 1.2f);

        // Pointer
        const float p0 = capR * 0.25f, p1 = capR * 0.92f;
        g.setColour (juce::Colours::white);
        g.drawLine (c.x + p0 * std::sin (angle), c.y - p0 * std::cos (angle),
                    c.x + p1 * std::sin (angle), c.y - p1 * std::cos (angle), juce::jmax (1.8f, R * 0.07f));
    }

    //==========================================================================
    // Ribbed thumb-wheel in a recessed housing; ribs scroll with the value.
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float minPos, float maxPos, juce::Slider::SliderStyle style,
                           juce::Slider& slider) override
    {
        if (! slider.getProperties()[wheelProperty])
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minPos, maxPos, style, slider);
            return;
        }

        const bool active = slider.isMouseOverOrDragging();
        const auto accent = active ? juce::Colour (0xffff5a70) : red;
        const bool bipolar = slider.getMinimum() < 0.0;
        const float norm = (float) slider.valueToProportionOfLength (slider.getValue());

        const float w = juce::jmin (40.0f, (float) width - 8.0f);
        const float cx = (float) x + (float) width * 0.5f;
        const juce::Rectangle<float> housing (cx - w * 0.5f, (float) y + 4.0f, w, (float) height - 14.0f);

        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (housing.expanded (4.0f, 3.0f), 8.0f);
        g.setColour (accent.withAlpha (0.28f));
        g.drawRoundedRectangle (housing.expanded (4.0f, 3.0f), 8.0f, 2.0f);

        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip;
            clip.addRoundedRectangle (housing, 6.0f);
            g.reduceClipRegion (clip);

            juce::ColourGradient cyl (juce::Colour (0xff060406), housing.getX(), 0,
                                      juce::Colour (0xff060406), housing.getRight(), 0, false);
            cyl.addColour (0.25, juce::Colour (0xff3a2a2e));
            cyl.addColour (0.5, juce::Colour (0xff62484e));
            cyl.addColour (0.75, juce::Colour (0xff3a2a2e));
            g.setGradientFill (cyl);
            g.fillRect (housing);

            const float pitch = 9.0f;
            const float travel = norm * pitch * 14.0f;
            for (float ry = housing.getY() - pitch + std::fmod (travel, pitch); ry < housing.getBottom(); ry += pitch)
            {
                g.setColour (juce::Colours::black.withAlpha (0.55f));
                g.fillRect (housing.getX(), ry, housing.getWidth(), 2.0f);
                g.setColour (accent.withAlpha (0.28f));
                g.fillRect (housing.getX(), ry + 2.0f, housing.getWidth(), 1.0f);
            }

            juce::ColourGradient f1 (juce::Colours::black.withAlpha (0.85f), 0, housing.getY(),
                                     juce::Colours::transparentBlack, 0, housing.getY() + housing.getHeight() * 0.28f, false);
            g.setGradientFill (f1);
            g.fillRect (housing);
            juce::ColourGradient f2 (juce::Colours::black.withAlpha (0.85f), 0, housing.getBottom(),
                                     juce::Colours::transparentBlack, 0, housing.getBottom() - housing.getHeight() * 0.28f, false);
            g.setGradientFill (f2);
            g.fillRect (housing);
        }

        const float py = housing.getBottom() - 8.0f - norm * (housing.getHeight() - 16.0f);
        g.setColour (accent.withAlpha (0.3f));
        g.fillRoundedRectangle (cx - w * 0.5f - 6.0f, py - 3.0f, w + 12.0f, 6.0f, 3.0f);
        g.setColour (accent);
        g.fillRect (cx - w * 0.5f - 6.0f, py - 1.0f, 8.0f, 2.0f);
        g.fillRect (cx + w * 0.5f - 2.0f, py - 1.0f, 8.0f, 2.0f);

        if (bipolar)
        {
            const float my = housing.getCentreY();
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            juce::Path tri;
            tri.addTriangle (housing.getX() - 9.0f, my - 4.0f, housing.getX() - 9.0f, my + 4.0f, housing.getX() - 2.0f, my);
            tri.addTriangle (housing.getRight() + 9.0f, my - 4.0f, housing.getRight() + 9.0f, my + 4.0f, housing.getRight() + 2.0f, my);
            g.fillPath (tri);
        }
    }

    //==========================================================================
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const auto c = down ? juce::Colours::white : red;
        g.setColour (c.withAlpha (highlighted || down ? 0.25f : 0.10f));
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (c);
        g.drawRoundedRectangle (r, 2.0f, 1.2f);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int,
                       juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
        g.setColour (panel);
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (red.withAlpha (box.isMouseOver() ? 1.0f : 0.6f));
        g.drawRoundedRectangle (r, 2.0f, 1.2f);

        juce::Path arrow;
        const float ax = (float) width - 14.0f, ay = (float) height * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (red);
        g.fillPath (arrow);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (4, 1, box.getWidth() - 24, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height);
        g.setColour (juce::Colour (0xf0140a0d));
        g.fillRect (r);
        g.setColour (red);
        g.drawRect (r, 1.0f);
        g.fillRect (r.removeFromLeft (3.0f));
        g.setColour (juce::Colours::white);
        g.setFont (monoFont (12.0f));
        g.drawFittedText (text, r.toNearestInt().reduced (8, 4), juce::Justification::centredLeft, 5);
    }

    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override
    {
        const int w = juce::jmin (270, parentArea.getWidth() - 10);
        juce::AttributedString s;
        s.append (tipText, monoFont (12.0f));
        juce::TextLayout tl;
        tl.createLayout (s, (float) (w - 16));
        const int h = (int) tl.getHeight() + 12;
        return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - w - 12 : screenPos.x + 24,
                                     screenPos.y > parentArea.getCentreY() ? screenPos.y - h - 6 : screenPos.y + 6, w, h)
                   .constrainedWithin (parentArea);
    }
};
