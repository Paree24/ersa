#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// ERSA-8 interface design language (v3, after Pastel EFX Grid; see mockup/redesign_v3.png):
// near-black charcoal, thin neutral panel edges, dot + title headers, arc knobs with
// value readouts, live views you drag. Orange is the sound path, teal is modulation,
// rose is the effects; nothing else is coloured.
struct ErsaColors
{
    static inline const juce::Colour bg        { 0xff141518 };
    static inline const juce::Colour header    { 0xff141518 };
    static inline const juce::Colour panel     { 0xff1c1f24 };
    static inline const juce::Colour inner     { 0xff16181c }; // view wells
    static inline const juce::Colour grid      { 0xff24272c };
    static inline const juce::Colour border    { 0xff33363c }; // thin panel outline
    static inline const juce::Colour edge      { 0xff3a3d43 }; // control outline
    static inline const juce::Colour bar       { 0xfff5a05a }; // browser header bar
    static inline const juce::Colour barText   { 0xff1b1d22 }; // dark text on a filled accent
    static inline const juce::Colour orange    { 0xfff5a05a }; // sound path
    static inline const juce::Colour orangeHi  { 0xffffc08a };
    static inline const juce::Colour teal      { 0xff8fd0c2 }; // modulation
    static inline const juce::Colour rose      { 0xffe9a3b8 }; // effects
    static inline const juce::Colour red       { 0xffe88a8a }; // panic / clip
    static inline const juce::Colour handle    { 0xfff4f1ec }; // drag handles, needles
    static inline const juce::Colour text      { 0xffd9d5d0 };
    static inline const juce::Colour textDim   { 0xff8f8b85 };
    static inline const juce::Colour faint     { 0xff5c5a56 };
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
    // section accent: the component's own "accent" property, else the enclosing group's
    static juce::Colour accentFor(juce::Component& c);
    static void drawPower(juce::Graphics&, juce::Rectangle<float> r, juce::Colour c, bool on);
    static void drawLogo(juce::Graphics&, float x, float y);

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
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
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
