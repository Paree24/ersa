#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Parameters.h"

// Readable value text for a parameter id (Hz, s/ms, dB, cents, %).
static juce::String ersaFormat(const juce::String& id, float v)
{
    auto hz = [](float f) { return f >= 1000.0f ? juce::String(f / 1000.0f, f >= 10000.0f ? 1 : 2) + " kHz"
                                                : juce::String(f, f < 10.0f ? 2 : 0) + " Hz"; };
    auto secs = [](float s) { return s < 1.0f ? juce::String(juce::roundToInt(s * 1000.0f)) + " ms"
                                              : juce::String(s, 2) + " s"; };
    auto sgn = [](float x, int dp) { return (x > 0.0f ? "+" : "") + juce::String(x, dp); };
    if (id == "cutoff") return hz(v);
    if (id == "oscmix") return juce::String(juce::roundToInt((1.0f - v) * 100.0f)) + ":" + juce::String(juce::roundToInt(v * 100.0f));
    if (id == "lforate" || id == "chrate" || id == "phrate") return juce::String(v, 2) + " Hz";
    if (id == "fa" || id == "fd" || id == "fr" || id == "aa" || id == "ad" || id == "ar" || id == "lfodelay")
        return secs(v);
    if (id == "dltime") return juce::String(juce::roundToInt(v)) + " ms";
    if (id == "vco2tune" || id == "tune") return sgn(std::round(v), 0) + " ct";
    if (id == "eqlow" || id == "eqhigh") return sgn(v, 1) + " dB";
    if (id == "fenv" || id == "envvco") return sgn(std::round(v * 100.0f), 0) + "%";
    if (id == "volume") return v <= 0.0001f ? juce::String("-inf") : sgn(juce::Decibels::gainToDecibels(v), 1) + " dB";
    if (id == "uidet") return juce::String(juce::roundToInt(v * 100.0f)) + " ct";
    if (id == "reso") return juce::String(juce::roundToInt(v / 0.95f * 100.0f)) + "%";
    if (id == "porta") return v < 0.005f ? juce::String("Off") : juce::String(juce::roundToInt(v * 100.0f)) + "%";
    return juce::String(juce::roundToInt(v * 100.0f)) + "%";
}


ErsaContent::ErsaContent(Ersa8Processor& p)
    : proc(p)
{
    setLookAndFeel(&lnf);
    pastel::Theme::format = ersaFormat;

    // header: preset name button opens the full browser
    addAndMakeVisible(presetBtn);
    presetBtn.setLookAndFeel(&lnf);
    presetBtn.setColour(juce::TextButton::buttonColourId, ErsaColors::comboBg);
    presetBtn.setColour(juce::TextButton::textColourOnId, ErsaColors::text);
    presetBtn.setColour(juce::TextButton::textColourOffId, ErsaColors::text);
    presetBtn.onClick = [&] { browser->setVisible(true); };
    refreshPresetName();
    prevBtn.onClick = [&] { int n = juce::jmax(1, proc.getNumPrograms()); proc.loadPreset((proc.getCurrentProgram() + n - 1) % n); refreshPresetName(); };
    nextBtn.onClick = [&] { int n = juce::jmax(1, proc.getNumPrograms()); proc.loadPreset((proc.getCurrentProgram() + 1) % n); refreshPresetName(); };
    initBtn.onClick = [&] { proc.loadInit(); refreshPresetName(); };
    panicBtn.onClick = [&] { proc.panic(); };
    for (auto* b : { &prevBtn, &nextBtn, &initBtn })
    {
        addAndMakeVisible(b);
        b->setColour(juce::TextButton::buttonColourId, ErsaColors::comboBg);
        b->setColour(juce::TextButton::textColourOnId, ErsaColors::text);
        b->setColour(juce::TextButton::textColourOffId, ErsaColors::text);
    }
    addAndMakeVisible(panicBtn);
    panicBtn.setColour(juce::TextButton::buttonColourId, ErsaColors::comboBg);
    panicBtn.setColour(juce::TextButton::textColourOnId, ErsaColors::red);
    panicBtn.setColour(juce::TextButton::textColourOffId, ErsaColors::red);
    // edit history + randomize
    for (auto* b : { &undoBtn, &redoBtn, &randBtn, &mutateBtn, &locksBtn })
    {
        addAndMakeVisible(b);
        b->setColour(juce::TextButton::buttonColourId, ErsaColors::comboBg);
        b->setColour(juce::TextButton::textColourOnId, ErsaColors::text);
        b->setColour(juce::TextButton::textColourOffId, ErsaColors::text);
    }
    undoBtn.onClick = [&] { proc.undoEdit(); refreshPresetName(); };
    redoBtn.onClick = [&] { proc.redoEdit(); refreshPresetName(); };
    randBtn.onClick = [&] { proc.randomize(); presetBtn.setButtonText("Random"); };
    mutateBtn.onClick = [&] { proc.mutate(); };
    locksBtn.onClick = [&]
    {
        juce::PopupMenu m; // ticked = locked: RAND / MUTATE leave that section alone
        m.addSectionHeader("Locked sections");
        const auto names = Ersa8Processor::randGroupNames();
        for (int g = 0; g < Ersa8Processor::RG_COUNT; ++g) m.addItem(g + 1, names[g], true, proc.isGroupLocked(g));
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&locksBtn),
                        [this](int id) { if (id > 0) proc.setGroupLocked(id - 1, !proc.isGroupLocked(id - 1)); });
    };
    undoBtn.setTooltip("Undo the last change (Ctrl+Z)");
    redoBtn.setTooltip("Redo (Ctrl+Shift+Z)");
    randBtn.setTooltip("Random patch inside musical limits (unlocked sections only, one undo step)");
    mutateBtn.setTooltip("Nudge the unlocked knobs a little (one undo step)");
    locksBtn.setTooltip("Choose which sections RAND / MUTATE leave alone");
    startTimerHz(8);

    using pastel::Wave;
    const auto teal = ErsaColors::teal;

    // ===== top row: sound path =====
    auto* gVco1 = addGroup("VCO-1", this, 8, 64, 172, 250, ErsaColors::orange);
    auto* gVco2 = addGroup("VCO-2", this, 186, 64, 196, 250, ErsaColors::orange);
    auto* gMix  = addGroup("Mix", this, 388, 64, 150, 250, ErsaColors::orange);
    auto* gVcf  = addGroup("VCF", this, 544, 64, 210, 250, ErsaColors::orange);
    auto* gLfo  = addGroup("LFO", this, 760, 64, 132, 250, teal);
    auto* gEnv  = addGroup("Envelopes", this, 898, 64, 334, 250, teal);

    // VCO-1: shape (drag the pulse width), wave, octave, unison, level, PWM
    addView<pastel::WaveView>(gVco1, { 12, 36, 148, 60 }, proc.apvts, P::VCO1_WAVE, P::VCO1_PW,
                            std::vector<Wave> { Wave::tri, Wave::saw, Wave::pulse, Wave::square });
    addSeg(P::VCO1_WAVE, gVco1, { 12, 102, 148, 22 }, {}, { Wave::tri, Wave::saw, Wave::pulse, Wave::square });
    addSeg(P::VCO1_RANGE, gVco1, { 12, 130, 148, 20 })
        ->setTooltip("Oscillator octave: -2/-1 below, 0 = concert pitch, +1/+2 above");
    addCaption("UNISON", gVco1, 12, 156, 44);
    addSeg(P::U1, gVco1, { 60, 156, 100, 18 });
    addKnob(P::VCO1_LEVEL, "Level", gVco1, 46, 202, 16);
    addKnob(P::VCO1_PWM, "PWM", gVco1, 126, 202, 16, teal);

    // VCO-2
    addView<pastel::WaveView>(gVco2, { 12, 36, 172, 60 }, proc.apvts, P::VCO2_WAVE, P::VCO2_PW,
                            std::vector<Wave> { Wave::saw, Wave::pulse, Wave::square, Wave::sine, Wave::noise });
    addSeg(P::VCO2_WAVE, gVco2, { 12, 102, 172, 22 }, {}, { Wave::saw, Wave::pulse, Wave::square, Wave::sine, Wave::noise });
    addSeg(P::VCO2_RANGE, gVco2, { 12, 130, 172, 20 }, { "-2", "-1", "0", "+1", "+2", "LO" })
        ->setTooltip("Oscillator octave; LO = sub-audio rate set by Tune (for X-Mod / sync)");
    addCaption("UNISON", gVco2, 12, 156, 44);
    addSeg(P::U2, gVco2, { 60, 156, 124, 18 });
    addKnob(P::VCO2_LEVEL, "Level", gVco2, 38, 202, 16);
    addKnob(P::VCO2_PWM, "PWM", gVco2, 98, 202, 16, teal);
    addKnob(P::VCO2_TUNE, "Tune", gVco2, 158, 202, 16, {}, true);

    // Mix / sync
    addKnob(P::MIXC, "VCO 1 : 2", gMix, 75, 82, 30);
    addToggle(P::SYNC, "Sync", gMix, 12, 146, 58, 22)->setTooltip("Hard sync VCO-2 to VCO-1");
    addKnob(P::XMOD, "X-Mod", gMix, 112, 156, 15);
    addCaption("HPF", gMix, 12, 216, 30);
    addSeg(P::HPF, gMix, { 42, 216, 96, 22 }, { "OFF", "1", "2", "3" });

    // VCF: drag the response (cutoff sideways, resonance up/down)
    addView<pastel::FilterView>(gVcf, { 12, 36, 186, 112 }, proc.apvts, P::CUTOFF, P::RESO, P::SLOPE);
    addSeg(P::SLOPE, gVcf, { 12, 154, 186, 22 }, { "12 dB", "24 dB" });
    addKnob(P::FENV, "Env", gVcf, 60, 200, 16, teal, true);
    addKnob(P::KEYTRK, "Key Trk", gVcf, 150, 200, 16);

    // LFO: shape with its delayed fade-in, rate, depths
    addView<pastel::LfoView>(gLfo, { 10, 36, 112, 54 }, proc.apvts, P::LFO_WAVE, P::LFO_DELAY,
                          std::vector<Wave> { Wave::tri, Wave::sine, Wave::saw, Wave::square, Wave::snh });
    addSeg(P::LFO_WAVE, gLfo, { 10, 96, 112, 22 }, {}, { Wave::tri, Wave::sine, Wave::saw, Wave::square, Wave::snh });
    addKnob(P::LFO_RATE, "Rate", gLfo, 66, 143, 15);
    addDepth(P::LFO_VCO1, "VCO1", gLfo, 10, 190, 112);
    addDepth(P::LFO_VCO2, "VCO2", gLfo, 10, 207, 112);
    addDepth(P::LFO_VCF, "VCF", gLfo, 10, 224, 112);

    // Envelopes: drag the points; VCA mode in the header
    addView<pastel::EnvView>(gEnv, { 12, 34, 310, 100 }, proc.apvts,
                           juce::StringArray { P::FA, P::FD, P::FS, P::FR }, "ENV-1 > VCF");
    addView<pastel::EnvView>(gEnv, { 12, 140, 310, 100 }, proc.apvts,
                           juce::StringArray { P::AA, P::AD, P::AS, P::AR }, "ENV-2 > VCA");
    addCaption("VCA", gEnv, 196, 8, 30);
    addSeg(P::VCA_MODE, gEnv, { 226, 8, 96, 20 })->setTooltip("Gate: the VCA follows the key; Env: ENV-2 shapes it");

    // ===== second row: arp + voice, effects, master =====
    auto* gArp = addGroup("Arp / Voice", this, 8, 322, 240, 246, teal);
    auto* gCh  = addGroup("Chorus", this, 254, 322, 160, 246, ErsaColors::rose);
    auto* gPh  = addGroup("Phaser", this, 420, 322, 160, 246, ErsaColors::rose);
    auto* gDl  = addGroup("Delay", this, 586, 322, 176, 246, ErsaColors::rose);
    auto* gRv  = addGroup("Reverb", this, 768, 322, 164, 246, ErsaColors::rose);
    auto* gMst = addGroup("Master", this, 938, 322, 294, 246, ErsaColors::orange);

    addPower(P::ARP_ON, gArp)->setTooltip("Arpeggiator on / off");
    addSeg(P::ARP_MODE, gArp, { 12, 36, 216, 22 }, { "UP", "DOWN", "UP/DN", "RND" });
    addChoice(P::ARP_DIV, gArp, 12, 64, 100, 22)->setTooltip("Arp rate, locked to the host tempo");
    addSeg(P::ARP_OCT, gArp, { 118, 64, 110, 22 });
    addToggle(P::ARP_HOLD, "Hold", gArp, 12, 94, 56, 20);
    addCaption("VOICE", gArp, 12, 124, 60);
    addSeg(P::VOICEMODE, gArp, { 12, 140, 216, 22 });
    addKnob(P::UIDET, "Detune", gArp, 42, 192, 15);
    addKnob(P::PORTA, "Porta", gArp, 120, 192, 15);
    addKnob(P::VELSENS, "Velocity", gArp, 198, 192, 15);

    // effects: drag the picture (two parameters each); click a bypassed one to switch it on
    addPower(P::CH_ON, gCh);
    addView<pastel::FxView>(gCh, { 10, 36, 140, 90 }, proc.apvts, pastel::FxView::Style::chorus, P::CH_RATE, P::CH_DEPTH, P::CH_ON)
        ->setTooltip("Drag: rate sideways, depth up/down");
    addSeg(P::CH_MODE, gCh, { 10, 132, 140, 22 }, { "MAN", "I", "II", "WIDE" });
    addKnob(P::CH_MIX, "Mix", gCh, 80, 192, 20);

    addPower(P::PH_ON, gPh);
    addView<pastel::FxView>(gPh, { 10, 36, 140, 90 }, proc.apvts, pastel::FxView::Style::phaser, P::PH_RATE, P::PH_DEPTH, P::PH_ON)
        ->setTooltip("Drag: rate sideways, depth up/down");
    addKnob(P::PH_FB, "Feedback", gPh, 46, 172, 18);
    addKnob(P::PH_MIX, "Mix", gPh, 114, 172, 18);

    addPower(P::DL_ON, gDl);
    addView<pastel::FxView>(gDl, { 10, 36, 156, 90 }, proc.apvts, pastel::FxView::Style::delay, P::DL_TIME, P::DL_FB, P::DL_ON, P::DL_SYNC)
        ->setTooltip("Drag: time sideways (when not synced), feedback up/down");
    addChoice(P::DL_SYNC, gDl, 10, 132, 156, 22)->setTooltip("Free time, or a note value locked to the host tempo");
    addKnob(P::DL_MIX, "Mix", gDl, 88, 192, 20);

    addPower(P::RV_ON, gRv);
    addView<pastel::FxView>(gRv, { 10, 36, 144, 90 }, proc.apvts, pastel::FxView::Style::reverb, P::RV_SIZE, P::RV_DAMP, P::RV_ON,
                          juce::String(), juce::String(), "SIZE", "DAMP")
        ->setTooltip("Drag: size sideways, damping up/down");
    addKnob(P::RV_MIX, "Mix", gRv, 82, 180, 24);

    // master: EQ you drag, drive, saturator, FX bypass
    addView<pastel::EqView>(gMst, { 12, 36, 170, 92 }, proc.apvts, P::EQLOW, P::EQHIGH)
        ->setTooltip("Drag the left / right half: low / high shelf (double-click = flat)");
    addKnob(P::DRIVE, "Drive", gMst, 238, 70, 18);
    addCaption("SATURATOR", gMst, 12, 140, 90);
    {
        auto* sat = addToggle(P::ST_ON, "", gMst, 96, 137, 22, 22);
        sat->getProperties().set("power", true);
        sat->setTooltip("Saturator on / off");
    }
    addSeg(P::ST_MODE, gMst, { 12, 166, 120, 22 });
    addKnob(P::ST_AMT, "Amount", gMst, 172, 178, 17);
    addToggle(P::FXBYPASS, "FX Bypass", gMst, 204, 136, 80, 22)->setTooltip("Bypass the whole effects chain");

    // ===== third row: the JP-8 modulation extras, arp groove, output =====
    auto* gMod = addGroup("VCO Mod", this, 8, 574, 380, 132, ErsaColors::orange);
    auto* gLev = addGroup("LFO Sync / Mod Lever", this, 394, 574, 240, 132, teal);
    auto* gGrv = addGroup("Arp Groove", this, 640, 574, 160, 132, teal);
    auto* gOut = addGroup("Output", this, 806, 574, 426, 132, ErsaColors::orange);
    addKnob(P::SUBLEVEL, "Sub", gMod, 40, 72, 17)->setTooltip("Sine sub one octave under VCO-1");
    addSeg(P::PWMSRC, gMod, { 84, 52, 100, 22 }, { "LFO", "ENV-1" });
    addCaption("PWM SOURCE", gMod, 84, 80, 100, juce::Justification::centred);
    addKnob(P::ENVVCO, "ENV-1 > VCO", gMod, 232, 72, 17, teal, true)->setTooltip("ENV-1 sweeps the pitch (centre = off)");
    addSeg(P::ENVVCODST, gMod, { 272, 52, 100, 22 }, { "VCO-1", "VCO-2", "BOTH" });
    addCaption("TARGET", gMod, 272, 80, 100, juce::Justification::centred);

    addChoice(P::LFO_SYNC, gLev, 12, 52, 96, 22)->setTooltip("Lock the LFO rate to the host tempo (Off = the Rate knob)");
    addCaption("LFO SYNC", gLev, 12, 80, 96, juce::Justification::centred);
    addKnob(P::MOD_VCO, "Lever>VCO", gLev, 144, 72, 17)->setTooltip("Mod wheel adds LFO vibrato, this deep");
    addKnob(P::MOD_VCF, "Lever>VCF", gLev, 208, 72, 17)->setTooltip("Mod wheel adds LFO filter wobble, this deep");
    addKnob(P::ARP_GATE, "Gate", gGrv, 44, 72, 17)->setTooltip("Arp note length (full = runs into the next step)");
    addKnob(P::ARP_SWING, "Swing", gGrv, 116, 72, 17)->setTooltip("Every second arp step comes late");

    scope = std::make_unique<OutputScope>(proc);
    gOut->addAndMakeVisible(*scope);
    scope->setBounds(12, 36, 250, 86);
    addKnob(P::VOLUME, "Volume", gOut, 300, 72, 20);
    addKnob(P::TUNE, "Tune", gOut, 372, 72, 17, {}, true);
    addCaption("BEND", gOut, 270, 8, 40);
    addSeg(P::BEND, gOut, { 306, 8, 108, 20 }, { "OFF", "2", "7", "12" })->setTooltip("Pitch-bend range in semitones");

    // preset browser overlay (created last: paints above everything)
    browser = std::make_unique<PresetBrowser>(proc, lnf);
    addAndMakeVisible(browser.get());
    browser->setVisible(false);
    browser->onClose = [&] { browser->setVisible(false); };
    browser->currentName = [&]
    {
        auto t = presetBtn.getButtonText(); // "042 Night Pad" -> "Night Pad"
        if (t.length() > 4 && t.substring(0, 3).containsOnly("0123456789") && t[3] == ' ')
            t = t.substring(4);
        return t == "Init Patch" ? juce::String() : t;
    };
    browser->onPresetChanged = [&]
    {
        if (browser->lastLoadedIsUser)
            presetBtn.setButtonText(browser->lastLoadedName);
        else
            refreshPresetName();
    };
}

ErsaContent::~ErsaContent() { setLookAndFeel(nullptr); }

juce::GroupComponent* ErsaContent::addGroup(const juce::String& title, juce::Component* parent,
                                            int x, int y, int w, int h, juce::Colour accent)
{
    auto* g = new juce::GroupComponent(title, title);
    groups.emplace_back(g);
    parent->addAndMakeVisible(g);
    g->setBounds(x, y, w, h);
    g->setLookAndFeel(&lnf);
    g->getProperties().set("accent", (int)accent.getARGB());
    return g;
}

juce::ToggleButton* ErsaContent::addPower(const juce::String& param, juce::GroupComponent* group)
{
    auto* t = addToggle(param, "", group, group->getWidth() - 30, 6, 24, 24);
    t->getProperties().set("power", true);
    return t;
}

juce::Label* ErsaContent::addCaption(const juce::String& text, juce::Component* parent,
                                     int x, int y, int w, juce::Justification just)
{
    auto* l = new juce::Label(text, text);
    labels.emplace_back(l);
    parent->addAndMakeVisible(l);
    l->setBounds(x, y, w, 18);
    l->setJustificationType(just);
    l->setBorderSize({});
    l->setColour(juce::Label::textColourId, ErsaColors::textDim);
    l->setFont(lnf.titleFont(9.5f));
    l->setInterceptsMouseClicks(false, false);
    return l;
}

juce::Slider* ErsaContent::addKnob(const juce::String& param, const juce::String& label,
                                   juce::Component* parent, int cx, int cy, int r,
                                   juce::Colour accent, bool bipolar)
{
    auto* s = new juce::Slider();
    knobs.emplace_back(s);
    parent->addAndMakeVisible(s);
    s->setBounds(cx - r - 3, cy - r - 3, 2 * r + 6, 2 * r + 6);
    s->setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s->setLookAndFeel(&lnf);
    if (! accent.isTransparent()) s->getProperties().set("accent", (int)accent.getARGB());
    if (bipolar) s->getProperties().set("bipolar", true);
    auto* name = new juce::Label(param + "_l", label.toUpperCase());
    labels.emplace_back(name);
    parent->addAndMakeVisible(name);
    name->setBounds(cx - 44, cy + r + 4, 88, 12);
    name->setJustificationType(juce::Justification::centred);
    name->setBorderSize({});
    name->setColour(juce::Label::textColourId, ErsaColors::textDim);
    name->setFont(lnf.titleFont(9.5f));
    name->setInterceptsMouseClicks(false, false);
    auto* value = new juce::Label(param + "_v", {});
    labels.emplace_back(value);
    parent->addAndMakeVisible(value);
    value->setBounds(cx - 44, cy + r + 16, 88, 14);
    value->setJustificationType(juce::Justification::centred);
    value->setBorderSize({});
    value->setColour(juce::Label::textColourId, ErsaColors::text);
    value->setFont(lnf.titleFont(11.0f));
    value->setInterceptsMouseClicks(false, false);
    if (auto* par = proc.apvts.getParameter(param))
    {
        sAtt.add(new juce::AudioProcessorValueTreeState::SliderAttachment(proc.apvts, param, *s));
        // live value bubble while hovering / dragging; double-click resets
        s->setPopupDisplayEnabled(true, true, this);
        s->setDoubleClickReturnValue(true, par->convertFrom0to1(par->getDefaultValue()));
    }
    // after the attachment: it installs the parameter's own (raw) text function
    s->textFromValueFunction = [param](double v) { return ersaFormat(param, (float)v); };
    s->onValueChange = [s, value] { value->setText(s->getTextFromValue(s->getValue()), juce::dontSendNotification); };
    s->onValueChange();
    return s;
}

juce::Slider* ErsaContent::addDepth(const juce::String& param, const juce::String& label,
                                    juce::Component* parent, int x, int y, int w)
{
    addCaption(label, parent, x, y - 1, 34);
    auto* s = new juce::Slider();
    knobs.emplace_back(s);
    parent->addAndMakeVisible(s);
    s->setBounds(x + 34, y, w - 34, 16);
    s->setSliderStyle(juce::Slider::LinearHorizontal);
    s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s->setLookAndFeel(&lnf);
    if (auto* par = proc.apvts.getParameter(param))
    {
        sAtt.add(new juce::AudioProcessorValueTreeState::SliderAttachment(proc.apvts, param, *s));
        s->setPopupDisplayEnabled(true, true, this);
        s->setDoubleClickReturnValue(true, par->convertFrom0to1(par->getDefaultValue()));
    }
    s->textFromValueFunction = [param](double v) { return ersaFormat(param, (float)v); };
    return s;
}

pastel::SegChoice* ErsaContent::addSeg(const juce::String& param, juce::Component* parent, juce::Rectangle<int> bounds,
                                     juce::StringArray labels, std::vector<pastel::Wave> icons)
{
    return addView<pastel::SegChoice>(parent, bounds, proc.apvts, param, labels, std::move(icons));
}

juce::ComboBox* ErsaContent::addChoice(const juce::String& param, juce::Component* parent,
                                       int x, int y, int w, int h)
{
    auto* c = new juce::ComboBox();
    boxes.emplace_back(c);
    parent->addAndMakeVisible(c);
    c->setBounds(x, y, w, h);
    c->setLookAndFeel(&lnf);
    c->setColour(juce::ComboBox::textColourId, ErsaColors::text);
    if (auto* par = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter(param)))
    {
        for (int i = 0; i < par->choices.size(); ++i) c->addItem(par->choices[i], i + 1);
    }
    cAtt.add(new juce::AudioProcessorValueTreeState::ComboBoxAttachment(proc.apvts, param, *c));
    return c;
}

juce::ToggleButton* ErsaContent::addToggle(const juce::String& param, const juce::String& label,
                                          juce::Component* parent, int x, int y, int w, int h)
{
    auto* t = new juce::ToggleButton(label);
    toggles.emplace_back(t);
    parent->addAndMakeVisible(t);
    t->setBounds(x, y, w, h);
    t->setLookAndFeel(&lnf);
    bAtt.add(new juce::AudioProcessorValueTreeState::ButtonAttachment(proc.apvts, param, *t));
    return t;
}

void ErsaContent::timerCallback()
{
    undoBtn.setEnabled(proc.canUndo());
    redoBtn.setEnabled(proc.canRedo());
}

OutputScope::OutputScope(Ersa8Processor& p) : proc(p) { startTimerHz(30); }

void OutputScope::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour(ErsaColors::inner);
    g.fillRoundedRectangle(r, 6.0f);
    // meters: two bars on the left, 0 dBFS at the top, -48 dB at the bottom
    auto mr = r.removeFromLeft(26.0f).reduced(4.0f, 6.0f);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto bar = mr.withWidth(7.0f).translated(ch * 10.0f, 0.0f);
        g.setColour(ErsaColors::track);
        g.fillRoundedRectangle(bar, 2.0f);
        const float db = juce::Decibels::gainToDecibels(proc.getMeter(ch), -48.0f);
        const float h = bar.getHeight() * (db + 48.0f) / 48.0f;
        g.setColour(db > -1.0f ? ErsaColors::red : ErsaColors::orange);
        g.fillRoundedRectangle(bar.withTop(bar.getBottom() - h), 2.0f);
    }
    // scope: the newest 512 samples, started at a rising zero crossing so a
    // steady note stands still
    float tmp[Ersa8Processor::kScopeSize];
    proc.copyScope(tmp, Ersa8Processor::kScopeSize);
    int start = 0;
    for (int i = 1; i < Ersa8Processor::kScopeSize - 512; ++i)
        if (tmp[i - 1] <= 0.0f && tmp[i] > 0.0f) { start = i; break; }
    auto sr = r.reduced(6.0f, 6.0f);
    g.setColour(ErsaColors::grid);
    g.drawHorizontalLine((int)sr.getCentreY(), sr.getX(), sr.getRight());
    juce::Path path;
    for (int i = 0; i < 512; ++i)
    {
        const float x = sr.getX() + sr.getWidth() * (float)i / 511.0f;
        const float y = sr.getCentreY() - juce::jlimit(-1.0f, 1.0f, tmp[start + i]) * sr.getHeight() * 0.5f;
        if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
    }
    g.setColour(ErsaColors::orange.withAlpha(0.9f));
    g.strokePath(path, juce::PathStrokeType(1.4f));
}

void ErsaContent::refreshPresetName()
{
    int cur = proc.getCurrentProgram();
    int n = juce::jmax(1, proc.getNumPrograms());
    if (cur >= 0 && cur < n)
        presetBtn.setButtonText(juce::String(cur).paddedLeft('0', 3) + " " + proc.getPresetName(cur));
    else
        presetBtn.setButtonText("Init Patch");
}

void ErsaContent::paint(juce::Graphics& g)
{
    g.fillAll(ErsaColors::bg);
    ErsaLookAndFeel::drawLogo(g, 18.0f, 17.0f);
    g.setColour(ErsaColors::text);
    g.setFont(lnf.logoFont(20.0f));
    g.drawText("E R S A", 44, 12, 120, 28, juce::Justification::centredLeft, false);
    g.setColour(ErsaColors::textDim);
    g.setFont(lnf.titleFont(10.0f));
    g.drawText("DUAL-VCO POLYSYNTH", 140, 12, 140, 28, juce::Justification::centredLeft, false);
    g.setColour(ErsaColors::border);
    g.fillRect(8, 52, getWidth() - 16, 1);
}

void ErsaContent::resized()
{
    undoBtn.setBounds(288, 12, 28, 28);
    redoBtn.setBounds(320, 12, 28, 28);
    prevBtn.setBounds(444, 12, 28, 28);
    presetBtn.setBounds(476, 12, 288, 28);
    nextBtn.setBounds(768, 12, 28, 28);
    randBtn.setBounds(916, 12, 64, 28);
    mutateBtn.setBounds(984, 12, 66, 28);
    locksBtn.setBounds(1054, 12, 58, 28);
    initBtn.setBounds(1116, 12, 50, 28);
    panicBtn.setBounds(1170, 12, 62, 28);
    if (browser != nullptr)
        browser->setBounds(0, 0, baseW, baseH);
}

// ---------------- resizable host editor ----------------
Ersa8Editor::Ersa8Editor(Ersa8Processor& p)
    : juce::AudioProcessorEditor(p), proc(p), content(p), corner(this, getConstrainer())
{
    setSize(ErsaContent::baseW, ErsaContent::baseH);
    setResizable(true, true);
    setResizeLimits(744, 428, 1984, 1142); // 0.6x .. 1.6x
    addAndMakeVisible(content);
    addAndMakeVisible(corner);
    setWantsKeyboardFocus(true);
}

Ersa8Editor::~Ersa8Editor() {}

bool Ersa8Editor::keyPressed(const juce::KeyPress& k)
{
    const auto m = k.getModifiers();
    if (m.isCommandDown() && (k.getKeyCode() == 'Z' || k.getKeyCode() == 'z'))
    {
        if (m.isShiftDown()) proc.redoEdit(); else proc.undoEdit();
        return true;
    }
    return false;
}

void Ersa8Editor::paint(juce::Graphics& g)
{
    g.fillAll(ErsaColors::bg); // letterbox surround
}

void Ersa8Editor::resized()
{
    float s = juce::jmin((float)getWidth() / (float)ErsaContent::baseW,
                         (float)getHeight() / (float)ErsaContent::baseH);
    s = juce::jlimit(0.4f, 3.0f, s);
    float ox = ((float)getWidth() - (float)ErsaContent::baseW * s) * 0.5f;
    float oy = ((float)getHeight() - (float)ErsaContent::baseH * s) * 0.5f;
    content.setTransform(juce::AffineTransform::scale(s).translated(ox, oy));
    content.setBounds(0, 0, ErsaContent::baseW, ErsaContent::baseH);
    corner.setBounds(getWidth() - 20, getHeight() - 20, 20, 20);
}
