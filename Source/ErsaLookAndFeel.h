#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// ERSA-8 interface design language (see mockup.png):
// near-black charcoal, thin burnt-orange panel borders, solid orange section
// bars with dark titles, needle knobs, geometric Barlow titles + Inter body text.
struct ErsaColors
{
    static inline const juce::Colour bg        { 0xff141518 };
    static inline const juce::Colour header    { 0xff17191d };
    static inline const juce::Colour panel     { 0xff1c2125 };
    static inline const juce::Colour border    { 0xffe28960 }; // thin panel outline
    static inline const juce::Colour bar       { 0xffe28960 }; // solid section bar
    static inline const juce::Colour barText   { 0xff241a12 }; // dark text on orange
    static inline const juce::Colour orange    { 0xffe28960 }; // accents, arcs, needles
    static inline const juce::Colour orangeHi  { 0xffffa67c }; // LED / hot states
    static inline const juce::Colour red       { 0xffe04a3a }; // panic / destructive
    // section accents
    static inline const juce::Colour accentGreen  { 0xff45d47a };
    static inline const juce::Colour accentYellow { 0xffd4b145 };
    static inline const juce::Colour accentPink   { 0xffffacce };
    static inline const juce::Colour accentBlue   { 0xff45d4bd };
    static inline const juce::Colour accentWhite  { 0xfff2f2f2 };
    static inline const juce::Colour text      { 0xffd9d5d0 };
    static inline const juce::Colour textDim   { 0xff8f8b85 };
    static inline const juce::Colour track     { 0xff33363c };
    static inline const juce::Colour comboBg   { 0xff232529 };
};

class ErsaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ErsaLookAndFeel();

    juce::Font uiFont(float h) const;    // Lato Regular, body/labels
    juce::Font titleFont(float h) const; // Lato Bold, bars/buttons
    juce::Font logoFont(float h) const;  // Lato Black, wordmark
    static juce::Colour accentFor(juce::Component& c); // section accent via parent group

    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h,
                          float pos, float min, float max,
                          const juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                          bool highlighted, bool down) override;
    void drawComboBox(juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh,
                      juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    void drawGroupComponentOutline(juce::Graphics&, int w, int h, const juce::String&,
                            const juce::Justification&, juce::GroupComponent&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                              bool isMouseOverButton, bool isButtonDown) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    void drawPopupMenuBackground(juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                           bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColour) override;
    juce::Font getPopupMenuFont() override;
    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator, int standardMenuItemHeight,
                                   int& idealWidth, int& idealHeight) override;
    juce::Font getLabelFont(juce::Label&) override;

private:
    juce::Typeface::Ptr latoReg, latoBold, latoBlack;
};
