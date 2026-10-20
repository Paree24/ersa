#include "ErsaLookAndFeel.h"
#include "ErsaAssets.h"
#include "PastelViews.h"

ErsaLookAndFeel::ErsaLookAndFeel()
{
    latoReg   = juce::Typeface::createSystemTypefaceFor(ErsaAssets::LatoRegular_ttf,
                                                         ErsaAssets::LatoRegular_ttfSize);
    latoBold  = juce::Typeface::createSystemTypefaceFor(ErsaAssets::LatoBold_ttf,
                                                         ErsaAssets::LatoBold_ttfSize);
    latoBlack = juce::Typeface::createSystemTypefaceFor(ErsaAssets::LatoBlack_ttf,
                                                         ErsaAssets::LatoBlack_ttfSize);
    // the shared v3 views draw with ERSA's palette and Lato
    pastel::Theme::accent  = ErsaColors::orange;
    pastel::Theme::inner   = ErsaColors::inner;
    pastel::Theme::grid    = ErsaColors::grid;
    pastel::Theme::edge    = ErsaColors::edge;
    pastel::Theme::comboBg = ErsaColors::comboBg;
    pastel::Theme::text    = ErsaColors::text;
    pastel::Theme::textDim = ErsaColors::textDim;
    pastel::Theme::faint   = ErsaColors::faint;
    pastel::Theme::handle  = ErsaColors::handle;
    pastel::Theme::ink     = ErsaColors::barText;
    pastel::Theme::regular = latoReg;
    pastel::Theme::bold    = latoBold;
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

// Controls take their own "accent" property if set (e.g. a teal mod-depth knob
// inside an orange section), else the enclosing group's; default = orange.
juce::Colour ErsaLookAndFeel::accentFor(juce::Component& c)
{
    for (juce::Component* p = &c; p != nullptr; p = p->getParentComponent())
    {
        auto v = p->getProperties()["accent"];
        if (v.isInt())
            return juce::Colour((juce::uint32)(int)v);
    }
    return ErsaColors::orange;
}

// The EFX Grid power glyph: a ring broken at the top with a short stem.
void ErsaLookAndFeel::drawPower(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, bool on)
{
    const auto col = on ? c : ErsaColors::faint;
    const float cx = r.getCentreX(), cy = r.getCentreY();
    const float rad = juce::jmin(r.getWidth(), r.getHeight()) * 0.5f - 1.0f;
    g.setColour(col);
    g.drawEllipse(cx - rad, cy - rad, rad * 2, rad * 2, 1.4f);
    juce::Path arc;
    const float ir = rad * 0.52f;
    arc.addCentredArc(cx, cy, ir, ir, 0.0f, juce::degreesToRadians(35.0f), juce::degreesToRadians(325.0f), true);
    g.strokePath(arc, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.drawLine(cx, cy - ir - 1.5f, cx, cy - 0.5f, 1.4f);
}

// The Pastel family mark: four rounded squares.
void ErsaLookAndFeel::drawLogo(juce::Graphics& g, float x, float y)
{
    const juce::uint32 cols[4] = { 0xffe88a8a, 0xff8fd0c2, 0xffe9a3b8, 0xffa9d58e };
    for (int i = 0; i < 4; ++i)
    {
        g.setColour(juce::Colour(cols[i]));
        g.fillRoundedRectangle(x + (float)(i % 2) * 10.0f, y + (float)(i / 2) * 10.0f, 8.0f, 8.0f, 2.0f);
    }
}

juce::Font ErsaLookAndFeel::getLabelFont(juce::Label&) { return uiFont(12.5f); }
juce::Font ErsaLookAndFeel::getComboBoxFont(juce::ComboBox&) { return titleFont(12.0f); }
juce::Font ErsaLookAndFeel::getPopupMenuFont() { return uiFont(13.5f); }
juce::Font ErsaLookAndFeel::getTextButtonFont(juce::TextButton&, int) { return titleFont(12.0f); }

void ErsaLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(4, 0, box.getWidth() - 22, box.getHeight());
    label.setFont(getComboBoxFont(box));
}

// ---------------- arc knob: track, value arc, needle ----------------
// "bipolar" property: the arc grows from 12 o'clock (centre = off).
void ErsaLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                       float pos, float start, float end, juce::Slider& slider)
{
    const float cx = x + w * 0.5f, cy = y + h * 0.5f;
    const float r = juce::jmin(w, h) * 0.5f - 2.0f;
    const float a = start + pos * (end - start);
    const float sw = juce::jmax(2.6f, r * 0.17f);
    const float arcR = r - sw * 0.5f;
    const auto accent = ErsaLookAndFeel::accentFor(slider);
    const auto stroke = juce::PathStrokeType(sw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc(cx, cy, arcR, arcR, 0.0f, start, end, true);
    g.setColour(ErsaColors::edge);
    g.strokePath(track, stroke);

    const bool bipolar = (bool)slider.getProperties()["bipolar"];
    const float from = bipolar ? (start + end) * 0.5f : start;
    if (std::abs(a - from) > 0.01f)
    {
        juce::Path val;
        val.addCentredArc(cx, cy, arcR, arcR, 0.0f, juce::jmin(from, a), juce::jmax(from, a), true);
        g.setColour(slider.isEnabled() ? accent : ErsaColors::faint);
        g.strokePath(val, stroke);
    }
    // body + needle
    const float br = r - sw - 1.5f;
    g.setColour(juce::Colour(0xff26292f));
    g.fillEllipse(cx - br, cy - br, br * 2, br * 2);
    g.setColour(ErsaColors::edge);
    g.drawEllipse(cx - br, cy - br, br * 2, br * 2, 1.0f);
    const float dx = std::sin(a), dy = -std::cos(a);
    g.setColour(slider.isEnabled() ? ErsaColors::handle : ErsaColors::textDim);
    g.drawLine(cx + dx * br * 0.25f, cy + dy * br * 0.25f, cx + dx * (br - 2.0f), cy + dy * (br - 2.0f), 2.0f);
}

// ---------------- thin linear slider (mod depths) ----------------
void ErsaLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
                                       float pos, float, float,
                                       const juce::Slider::SliderStyle, juce::Slider& s)
{
    const auto accent = ErsaLookAndFeel::accentFor(s);
    const float cy = y + h * 0.5f;
    const float l = x + 6.0f, rr = (float)(x + w - 6);
    g.setColour(ErsaColors::track);
    g.fillRoundedRectangle(l, cy - 2.0f, rr - l, 4.0f, 2.0f);
    const float px = juce::jlimit(l, rr, pos); // JUCE passes the thumb position in pixels
    g.setColour(accent);
    g.fillRoundedRectangle(l, cy - 2.0f, px - l, 4.0f, 2.0f);
    g.setColour(ErsaColors::handle);
    g.fillEllipse(px - 6.0f, cy - 6.0f, 12.0f, 12.0f);
}

// ---------------- toggles: power glyph or pill ----------------
// "power" property: draw the EFX power glyph (section on/off), else a pill
// that fills with the accent when on.
void ErsaLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool hover, bool)
{
    const bool on = b.getToggleState();
    const auto accent = ErsaLookAndFeel::accentFor(b);
    auto r = b.getLocalBounds().toFloat();
    if ((bool)b.getProperties()["power"])
    {
        drawPower(g, r.withSizeKeepingCentre(19.0f, 19.0f), accent, on);
        return;
    }
    r = r.reduced(1.0f);
    const float rad = r.getHeight() * 0.5f;
    if (on)
    {
        g.setColour(accent);
        g.fillRoundedRectangle(r, rad);
    }
    else
    {
        g.setColour(hover ? accent.withAlpha(0.8f) : accent.withAlpha(0.55f));
        g.drawRoundedRectangle(r, rad, 1.2f);
    }
    g.setColour(on ? ErsaColors::barText : accent);
    g.setFont(titleFont(11.0f));
    g.drawText(b.getButtonText().toUpperCase(), r, juce::Justification::centred, true);
}

// ---------------- dropdown ----------------
void ErsaLookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool,
                                   int, int, int, int, juce::ComboBox& box)
{
    g.setColour(ErsaColors::comboBg);
    g.fillRoundedRectangle(0, 0, (float)w, (float)h, 5.0f);
    g.setColour(box.hasKeyboardFocus(true) ? ErsaLookAndFeel::accentFor(box) : ErsaColors::edge);
    g.drawRoundedRectangle(0.5f, 0.5f, w - 1.0f, h - 1.0f, 5.0f, 1.0f);
    juce::Path arrow;
    arrow.startNewSubPath(w - 15.0f, h * 0.5f - 2.0f);
    arrow.lineTo(w - 11.0f, h * 0.5f + 2.0f);
    arrow.lineTo(w - 7.0f, h * 0.5f - 2.0f);
    g.setColour(ErsaColors::textDim);
    g.strokePath(arrow, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ---------------- section panel: neutral edge, accent dot + title ----------------
void ErsaLookAndFeel::drawGroupComponentOutline(juce::Graphics& g, int w, int h,
                                         const juce::String& text,
                                         const juce::Justification&, juce::GroupComponent& group)
{
    g.setColour(ErsaColors::panel);
    g.fillRoundedRectangle(0, 0, (float)w, (float)h, 8.0f);
    g.setColour(ErsaColors::border);
    g.drawRoundedRectangle(0.5f, 0.5f, w - 1.0f, h - 1.0f, 8.0f, 1.0f);
    g.setColour(ErsaLookAndFeel::accentFor(group));
    g.fillEllipse(12.4f, 14.4f, 7.2f, 7.2f);
    g.setColour(ErsaColors::text);
    g.setFont(logoFont(13.0f));
    g.drawText(text.toUpperCase(), 26, 6, w - 60, 24, juce::Justification::centredLeft, true);
}

// ---------------- dark buttons ----------------
void ErsaLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b,
                                           const juce::Colour&, bool hover, bool down)
{
    const float w = (float)b.getWidth(), h = (float)b.getHeight();
    g.setColour(down ? juce::Colour(0xff2b2e34) : ErsaColors::comboBg);
    g.fillRoundedRectangle(0, 0, w, h, 5.0f);
    g.setColour(hover || b.hasKeyboardFocus(true) ? ErsaColors::orange.withAlpha(0.8f) : ErsaColors::edge);
    g.drawRoundedRectangle(0.5f, 0.5f, w - 1, h - 1, 5.0f, 1.0f);
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
