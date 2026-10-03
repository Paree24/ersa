#include "ErsaLookAndFeel.h"
#include "ErsaAssets.h"

ErsaLookAndFeel::ErsaLookAndFeel()
{
    latoReg   = juce::Typeface::createSystemTypefaceFor(ErsaAssets::LatoRegular_ttf,
                                                         ErsaAssets::LatoRegular_ttfSize);
    latoBold  = juce::Typeface::createSystemTypefaceFor(ErsaAssets::LatoBold_ttf,
                                                         ErsaAssets::LatoBold_ttfSize);
    latoBlack = juce::Typeface::createSystemTypefaceFor(ErsaAssets::LatoBlack_ttf,
                                                         ErsaAssets::LatoBlack_ttfSize);
}

juce::Font ErsaLookAndFeel::uiFont(float h) const
{
    if (latoReg != nullptr) return juce::Font(latoReg).withHeight(h);
    return juce::Font(h);
}

juce::Font ErsaLookAndFeel::titleFont(float h) const
{
    if (latoBold != nullptr) return juce::Font(latoBold).withHeight(h);
    return juce::Font(juce::FontOptions(h, juce::Font::bold));
}

juce::Font ErsaLookAndFeel::logoFont(float h) const
{
    if (latoBlack != nullptr) return juce::Font(latoBlack).withHeight(h);
    return titleFont(h);
}

// Controls inherit their section accent from the enclosing group
// (groups carry an "accent" ARGB property; default = orange).
juce::Colour ErsaLookAndFeel::accentFor(juce::Component& c)
{
    juce::Component* start = dynamic_cast<juce::GroupComponent*>(&c) != nullptr
                             ? &c : c.getParentComponent();
    for (juce::Component* p = start; p != nullptr; p = p->getParentComponent())
    {
        if (auto* g = dynamic_cast<juce::GroupComponent*>(p))
        {
            auto v = g->getProperties()["accent"];
            if (v.isInt())
                return juce::Colour((juce::uint32)(int)v);
            return ErsaColors::orange;
        }
    }
    return ErsaColors::orange;
}

juce::Font ErsaLookAndFeel::getLabelFont(juce::Label&) { return uiFont(12.5f); }
juce::Font ErsaLookAndFeel::getComboBoxFont(juce::ComboBox&) { return uiFont(13.0f); }
juce::Font ErsaLookAndFeel::getPopupMenuFont() { return uiFont(13.5f); }
juce::Font ErsaLookAndFeel::getTextButtonFont(juce::TextButton&, int) { return titleFont(12.5f); }

// ---------------- flat needle knob ----------------
void ErsaLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                       float pos, float start, float end, juce::Slider& slider)
{
    float cx = x + w * 0.5f, cy = y + h * 0.5f;
    float r = juce::jmin(w, h) * 0.5f - 1.0f;
    float a = start + pos * (end - start);

    // flat body + hairline edge
    float br = r - 5.0f;
    g.setColour(juce::Colour(0xff26292f));
    g.fillEllipse(cx - br, cy - br, br * 2, br * 2);
    g.setColour(juce::Colour(0xff3a3d43));
    g.drawEllipse(cx - br + 0.5f, cy - br + 0.5f, br * 2 - 1, br * 2 - 1, 1.0f);
    // track + value arcs
    float arcR = r - 1.5f;
    g.setColour(ErsaColors::track);
    juce::Path track;
    track.addArc(cx - arcR, cy - arcR, arcR * 2, arcR * 2, start, end, true);
    g.strokePath(track, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::butt));
    if (pos > 0.001f)
    {
        g.setColour(ErsaLookAndFeel::accentFor(slider));
        juce::Path val;
        val.addArc(cx - arcR, cy - arcR, arcR * 2, arcR * 2, start, a, true);
        g.strokePath(val, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::butt));
    }
    // end ticks
    g.setColour(ErsaColors::textDim.withAlpha(0.6f));
    for (float ta : { start, end })
    {
        float tx = cx + std::cos(ta - juce::MathConstants<float>::halfPi) * (arcR + 2.5f);
        float ty = cy + std::sin(ta - juce::MathConstants<float>::halfPi) * (arcR + 2.5f);
        g.fillEllipse(tx - 1.0f, ty - 1.0f, 2.0f, 2.0f);
    }
    // needle
    float dx = std::cos(a - juce::MathConstants<float>::halfPi);
    float dy = std::sin(a - juce::MathConstants<float>::halfPi);
    g.setColour(ErsaLookAndFeel::accentFor(slider));
    g.drawLine(cx + dx * 2.0f, cy + dy * 2.0f,
               cx + dx * (br - 2.0f), cy + dy * (br - 2.0f), 2.5f);
}

// ---------------- mixer-style linear slider ----------------
void ErsaLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
                                       float pos, float, float,
                                       const juce::Slider::SliderStyle, juce::Slider& s)
{
    if (s.isVertical())
    {
        float cx = x + w * 0.5f;
        float top = y + 4.0f, bot = (float)(y + h - 4);
        g.setColour(ErsaColors::track);
        g.fillRoundedRectangle(cx - 1.5f, top, 3.0f, bot - top, 1.5f);
        float fy = top + (1.0f - pos) * (bot - top);
        g.setColour(ErsaColors::orange);
        g.fillRoundedRectangle(cx - 1.5f, fy, 3.0f, bot - fy, 1.5f);
        // square thumb
        g.fillRoundedRectangle(cx - 8.0f, fy - 6.0f, 16.0f, 12.0f, 2.0f);
        g.setColour(ErsaColors::barText.withAlpha(0.85f));
        g.fillRect(cx - 4.0f, fy - 1.0f, 8.0f, 2.0f);
    }
    else
    {
        float cy = y + h * 0.5f;
        float l = x + 4.0f, rr = (float)(x + w - 4);
        g.setColour(ErsaColors::track);
        g.fillRoundedRectangle(l, cy - 1.5f, rr - l, 3.0f, 1.5f);
        float fx = l + pos * (rr - l);
        g.setColour(ErsaColors::orange);
        g.fillRoundedRectangle(l, cy - 1.5f, fx - l, 3.0f, 1.5f);
        g.fillRoundedRectangle(fx - 6.0f, cy - 8.0f, 12.0f, 16.0f, 2.0f);
    }
}

// ---------------- pill switch ----------------
void ErsaLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    bool on = b.getToggleState();
    float w = (float)b.getWidth(), h = (float)b.getHeight();
    float pw = juce::jmin(w - 12.0f, 40.0f), ph = 15.0f;
    float px = (w - pw) * 0.5f, py = 3.0f;
    g.setColour(on ? ErsaLookAndFeel::accentFor(b).withAlpha(0.35f) : ErsaColors::track);
    g.fillRoundedRectangle(px, py, pw, ph, ph * 0.5f);
    float knobR = ph - 5.0f;
    float kx = on ? px + pw - knobR - 2.5f : px + 2.5f;
    g.setColour(on ? ErsaLookAndFeel::accentFor(b) : ErsaColors::textDim);
    g.fillEllipse(kx, py + 2.5f, knobR, knobR);
    g.setColour(on ? ErsaColors::text : ErsaColors::textDim);
    g.setFont(uiFont(12.0f));
    g.drawText(b.getButtonText(), 0, py + ph + 2.0f, (int)w, (int)(h - py - ph - 2.0f),
               juce::Justification::centred, true);
}

// ---------------- dropdown ----------------
void ErsaLookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool,
                                   int, int, int, int, juce::ComboBox& box)
{
    g.setColour(ErsaColors::comboBg);
    g.fillRoundedRectangle(0, 0, w, h, 4.0f);
    g.setColour(box.hasKeyboardFocus(true) ? ErsaLookAndFeel::accentFor(box) : juce::Colour(0xff3a3d43));
    g.drawRoundedRectangle(0.5f, 0.5f, w - 1, h - 1, 4.0f, 1.0f);
    juce::Path arrow;
    arrow.addTriangle(w - 16.0f, h * 0.5f - 3, w - 8.0f, h * 0.5f - 3, w - 12.0f, h * 0.5f + 3);
    g.setColour(ErsaLookAndFeel::accentFor(box));
    g.fillPath(arrow);
    juce::ignoreUnused(box);
}

// ---------------- section panel: thin orange border + solid orange bar ----------------
void ErsaLookAndFeel::drawGroupComponentOutline(juce::Graphics& g, int w, int h,
                                         const juce::String& text,
                                         const juce::Justification&, juce::GroupComponent& group)
{
    g.setColour(ErsaColors::panel);
    g.fillRoundedRectangle(0, 0, w, h, 5.0f);
    juce::Colour accent = ErsaLookAndFeel::accentFor(group);
    g.setColour(accent);
    g.drawRoundedRectangle(0.5f, 0.5f, w - 1, h - 1, 5.0f, 1.0f);
    const float barH = 24.0f;
    g.setColour(accent);
    g.fillRoundedRectangle(1, 1, w - 2, barH, 4.0f);
    g.fillRect(1, (int)(barH - 5), w - 2, 5); // square off bar bottom
    g.setColour(ErsaColors::barText);
    g.setFont(titleFont(14.0f));
    g.drawText(text.toUpperCase(), 10, 0, w - 12, (int)barH, juce::Justification::centredLeft, false);
}

// ---------------- dark pill buttons ----------------
void ErsaLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b,
                                           const juce::Colour&, bool hover, bool down)
{
    float w = (float)b.getWidth(), h = (float)b.getHeight();
    g.setColour(down ? juce::Colour(0xff2b2e34) : ErsaColors::comboBg);
    g.fillRoundedRectangle(0, 0, w, h, 5.0f);
    g.setColour(hover || b.hasKeyboardFocus(true) ? ErsaColors::orange : juce::Colour(0xff3a3d43));
    g.drawRoundedRectangle(0.5f, 0.5f, w - 1, h - 1, 5.0f, hover ? 1.5f : 1.0f);
}

// ---------------- dark popup menu ----------------
void ErsaLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int w, int h)
{
    g.setColour(ErsaColors::panel);
    g.fillRoundedRectangle(0, 0, w, h, 5.0f);
    g.setColour(ErsaColors::border);
    g.drawRoundedRectangle(0.5f, 0.5f, w - 1, h - 1, 5.0f, 1.0f);
}

void ErsaLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                        bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                                        bool hasSubMenu, const juce::String& text,
                                        const juce::String&, const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour(ErsaColors::track);
        g.fillRect(area.getX() + 8, area.getCentreY(), area.getWidth() - 16, 1);
        return;
    }
    if (isHighlighted && isActive)
    {
        g.setColour(ErsaColors::orange.withAlpha(0.22f));
        g.fillRoundedRectangle(area.toFloat(), 3.0f);
    }
    g.setColour(!isActive ? ErsaColors::textDim.withAlpha(0.5f)
                : isHighlighted ? ErsaColors::orangeHi : ErsaColors::text);
    g.setFont(uiFont(13.0f));
    g.drawText(text, area.getX() + 12, area.getY(), area.getWidth() - 24, area.getHeight(),
               juce::Justification::centredLeft, true);
    if (isTicked)
    {
        g.setColour(ErsaColors::orange);
        float s = 6.0f, cx = (float)area.getX() + 5.0f, cy = (float)area.getCentreY();
        juce::Path tick;
        tick.startNewSubPath(cx - s * 0.4f, cy);
        tick.lineTo(cx, cy + s * 0.4f);
        tick.lineTo(cx + s * 0.6f, cy - s * 0.5f);
        g.strokePath(tick, juce::PathStrokeType(2.0f, juce::PathStrokeType::mitered,
                                                juce::PathStrokeType::rounded));
    }
    juce::ignoreUnused(hasSubMenu);
}

void ErsaLookAndFeel::getIdealPopupMenuItemSize(const juce::String&, bool, int standardMenuItemHeight,
                                                int& idealWidth, int& idealHeight)
{
    idealWidth = 240;
    idealHeight = juce::jmax(24, standardMenuItemHeight);
}
