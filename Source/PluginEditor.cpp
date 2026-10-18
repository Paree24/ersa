#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Parameters.h"

ErsaContent::ErsaContent(Ersa8Processor& p)
    : proc(p)
{
    setLookAndFeel(&lnf);

    // header: preset name button opens the full browser
    addAndMakeVisible(presetBtn);
    presetBtn.setLookAndFeel(&lnf);
    presetBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2c2c33));
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
        b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2c2c33));
        b->setColour(juce::TextButton::textColourOnId, ErsaColors::orange);
        b->setColour(juce::TextButton::textColourOffId, ErsaColors::orange);
    }
    addAndMakeVisible(panicBtn);
    panicBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2c2c33));
    panicBtn.setColour(juce::TextButton::textColourOnId, ErsaColors::red);
    panicBtn.setColour(juce::TextButton::textColourOffId, ErsaColors::red);
    // edit history + randomize
    for (auto* b : { &undoBtn, &redoBtn, &randBtn, &mutateBtn, &locksBtn })
    {
        addAndMakeVisible(b);
        b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2c2c33));
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

    // ===== top row groups =====
    auto* gLfo   = addGroup("LFO", this, 760, 64, 132, 250, ErsaColors::accentYellow);
    auto* gVco1  = addGroup("VCO-1", this, 8, 64, 172, 250, ErsaColors::orange);
    auto* gVco2  = addGroup("VCO-2", this, 186, 64, 196, 250, ErsaColors::orange);
    auto* gMix   = addGroup("MIX / SYNC", this, 388, 64, 150, 250, ErsaColors::orange);
    auto* gVcf   = addGroup("VCF", this, 544, 64, 210, 250, ErsaColors::accentGreen);
    auto* gEnv   = addGroup("ENVELOPES / VCA", this, 898, 64, 334, 250, ErsaColors::accentYellow);

    // Layout grid (measured, not eyeballed): top-row knob centres sit on
    // rows y=53 / 138 / 202, bottom-row FX knobs on 128 / 200. Combos are
    // centred on the row they share with knobs; every caption sits BELOW its
    // control on the same baseline as the knob labels; columns are symmetric.

    // LFO (narrow group: small knobs so nothing overlaps)
    addKnob(P::LFO_RATE, "Rate", gLfo, 6, 32, 58, 58);
    addKnob(P::LFO_DELAY, "Delay", gLfo, 68, 32, 58, 58);
    addChoice(P::LFO_WAVE, gLfo, 11, 92, 110);
    addKnob(P::LFO_VCO1, "VCO1", gLfo, 6, 117, 58, 58);
    addKnob(P::LFO_VCO2, "VCO2", gLfo, 68, 117, 58, 58);
    addKnob(P::LFO_VCF, "VCF", gLfo, 37, 184, 58, 52);

    // VCO-1
    addChoice(P::VCO1_WAVE, gVco1, 31, 30, 110);
    auto* range1 = addChoice(P::VCO1_RANGE, gVco1, 31, 58, 110);
    range1->setTooltip("Oscillator octave: -2/-1 below, 0 = concert pitch, +1/+2 above");
    addCaption("Octave", gVco1, 31, 82, 110);
    addKnob(P::VCO1_LEVEL, "Level", gVco1, 8, 115);
    addKnob(P::VCO1_PW, "PW", gVco1, 100, 115);
    addKnob(P::VCO1_PWM, "PWM", gVco1, 8, 185, 64, 50);
    addKnob(P::U1, "Uni", gVco1, 100, 185, 64, 50);

    // VCO-2 (three small knobs: columns mirrored around the group centre)
    addChoice(P::VCO2_WAVE, gVco2, 43, 30, 110);
    auto* range2 = addChoice(P::VCO2_RANGE, gVco2, 43, 58, 110);
    range2->setTooltip("Oscillator octave: -2/-1 below, 0 = concert pitch, +1/+2 above; Low = sub-audio rate set by Tune (for X-Mod / sync)");
    addCaption("Octave", gVco2, 43, 82, 110);
    addKnob(P::VCO2_LEVEL, "Level", gVco2, 8, 115);
    addKnob(P::VCO2_PW, "PW", gVco2, 124, 115);
    addKnob(P::VCO2_PWM, "PWM", gVco2, 8, 184, 60, 52);
    addKnob(P::VCO2_TUNE, "Tune", gVco2, 68, 184, 60, 52);
    addKnob(P::U2, "Uni", gVco2, 128, 184, 60, 52);

    // Mix / sync (toggle pill and HPF combo share one centre line)
    addKnob(P::MIXC, "Mix", gMix, 39, 30, 72, 72);
    addToggle(P::SYNC, "Sync", gMix, 6, 127, 52, 34);
    addChoice(P::HPF, gMix, 74, 127, 64);
    addCaption("HPF", gMix, 74, 147, 64);
    addKnob(P::XMOD, "XMod", gMix, 43, 182, 64, 56);

    // VCF (columns at 40 / 105 / 170)
    addKnob(P::CUTOFF, "Cutoff", gVcf, 8, 30, 64, 62);
    addKnob(P::RESO, "Reso", gVcf, 73, 30, 64, 62);
    addKnob(P::FENV, "Env", gVcf, 140, 30, 60, 62);
    addKnob(P::KEYTRK, "KeyTrk", gVcf, 8, 115, 64, 62);
    addChoice(P::SLOPE, gVcf, 75, 127, 60);
    addCaption("Slope", gVcf, 75, 161, 60);
    addKnob(P::VELSENS, "Veloc", gVcf, 140, 115, 60, 62);
    addKnob(P::PORTA, "Porta", gVcf, 10, 182, 60, 56);
    addChoice(P::VCA_MODE, gVcf, 75, 191, 60);
    addCaption("VCA", gVcf, 75, 222, 60);
    addKnob(P::VOLUME, "Volume", gVcf, 139, 182, 62, 56);

    // Envelopes (four columns centred in the group)
    const char* fIds[4] = { P::FA, P::FD, P::FS, P::FR };
    const char* aIds[4] = { P::AA, P::AD, P::AS, P::AR };
    const char* fLbl[4] = { "F-Atk", "F-Dec", "F-Sus", "F-Rel" };
    const char* aLbl[4] = { "A-Atk", "A-Dec", "A-Sus", "A-Rel" };
    for (int i = 0; i < 4; ++i)
    {
        addKnob(fIds[i], fLbl[i], gEnv, 11 + i * 80, 30, 72, 62);
        addKnob(aIds[i], aLbl[i], gEnv, 11 + i * 80, 115, 72, 62);
    }
    addKnob(P::TUNE, "Tune", gEnv, 95, 184, 64, 52);
    addChoice(P::BEND, gEnv, 197, 191, 100);
    addCaption("Bend", gEnv, 197, 220, 100);

    // ===== bottom row =====
    // bottom row 4 px shorter: the window resize grip must not cover MASTER's corner
    auto* gArp   = addGroup("ARPEGGIATOR / VOICE", this, 8, 322, 240, 246, ErsaColors::accentPink);
    auto* gCh    = addGroup("CHORUS", this, 254, 322, 160, 246, ErsaColors::accentBlue);
    auto* gPh    = addGroup("PHASER", this, 420, 322, 160, 246, ErsaColors::accentBlue);
    auto* gDl    = addGroup("DELAY", this, 586, 322, 176, 246, ErsaColors::accentBlue);
    auto* gRv    = addGroup("REVERB", this, 768, 322, 164, 246, ErsaColors::accentBlue);
    auto* gEq    = addGroup("MASTER", this, 938, 322, 294, 246, ErsaColors::accentWhite);

    // Arpeggiator + voice (host-synced divisions)
    addToggle(P::ARP_ON, "On", gArp, 8, 45, 60, 34);
    addToggle(P::ARP_HOLD, "Hold", gArp, 70, 45, 60, 34);
    addChoice(P::ARP_MODE, gArp, 135, 44, 95);
    addChoice(P::ARP_DIV, gArp, 8, 110, 140);
    addChoice(P::ARP_OCT, gArp, 156, 110, 76);
    addCaption("Rate", gArp, 8, 134, 140);
    addCaption("Oct", gArp, 156, 134, 76);
    addChoice(P::VOICEMODE, gArp, 8, 187, 140);
    addCaption("Voice", gArp, 8, 220, 140);
    addKnob(P::UIDET, "Detune", gArp, 156, 176, 68, 60);

    // FX: On toggle pill centred on the mode combo; knob pairs at +/-36 px
    // around each group's centre, Rate/Size row 128, Mix row 200
    addToggle(P::CH_ON, "On", gCh, 8, 45, 60, 34);
    addChoice(P::CH_MODE, gCh, 72, 44, 80);
    addKnob(P::CH_RATE, "Rate", gCh, 10, 105, 68, 62);
    addKnob(P::CH_DEPTH, "Depth", gCh, 82, 105, 68, 62);
    addKnob(P::CH_MIX, "Mix", gCh, 46, 180, 68, 56);

    addToggle(P::PH_ON, "On", gPh, 8, 45, 60, 34);
    addKnob(P::PH_RATE, "Rate", gPh, 10, 105, 68, 62);
    addKnob(P::PH_DEPTH, "Depth", gPh, 82, 105, 68, 62);
    addKnob(P::PH_FB, "FB", gPh, 10, 180, 68, 56);
    addKnob(P::PH_MIX, "Mix", gPh, 82, 180, 68, 56);

    addToggle(P::DL_ON, "On", gDl, 8, 45, 60, 34);
    addChoice(P::DL_SYNC, gDl, 72, 44, 96);
    addKnob(P::DL_TIME, "Time", gDl, 24, 105, 56, 62);
    addKnob(P::DL_FB, "FB", gDl, 96, 105, 56, 62);
    addKnob(P::DL_MIX, "Mix", gDl, 60, 180, 56, 56);

    addToggle(P::RV_ON, "On", gRv, 8, 45, 60, 34);
    addKnob(P::RV_SIZE, "Size", gRv, 18, 105, 56, 62);
    addKnob(P::RV_DAMP, "Damp", gRv, 90, 105, 56, 62);
    addKnob(P::RV_MIX, "Mix", gRv, 54, 180, 56, 56);

    // Master: four big knobs centred, then three equal columns of switches
    addKnob(P::EQLOW, "Low", gEq, 9, 64, 60, 78);
    addKnob(P::EQHIGH, "High", gEq, 81, 64, 60, 78);
    addKnob(P::ST_AMT, "Sat", gEq, 153, 64, 60, 78);
    addKnob(P::DRIVE, "Drive", gEq, 225, 64, 60, 78);
    addToggle(P::FXBYPASS, "FX Bypass", gEq, 4, 180, 90, 34);
    addToggle(P::ST_ON, "Sat On", gEq, 117, 180, 60, 34);
    addChoice(P::ST_MODE, gEq, 203, 179, 84);

    // ===== third row: the JP-8 modulation extras, arp groove, output =====
    auto* gMod  = addGroup("VCO MOD", this, 8, 574, 380, 132, ErsaColors::orange);
    auto* gLev  = addGroup("LFO SYNC / MOD LEVER", this, 394, 574, 240, 132, ErsaColors::accentYellow);
    auto* gGrv  = addGroup("ARP GROOVE", this, 640, 574, 160, 132, ErsaColors::accentPink);
    auto* gOut  = addGroup("OUTPUT", this, 806, 574, 426, 132, ErsaColors::accentWhite);
    addKnob(P::SUBLEVEL, "Sub", gMod, 8, 34, 64, 62)->setTooltip("Sine sub one octave under VCO-1");
    addChoice(P::PWMSRC, gMod, 84, 48, 84);
    addCaption("PWM Src", gMod, 84, 76, 84);
    addKnob(P::ENVVCO, "ENV-1>VCO", gMod, 182, 34, 84, 62)->setTooltip("ENV-1 sweeps the pitch (centre = off)");
    addChoice(P::ENVVCODST, gMod, 276, 48, 92);
    addCaption("Target", gMod, 276, 76, 92);
    addChoice(P::LFO_SYNC, gLev, 10, 48, 84)->setTooltip("Lock the LFO rate to the host tempo (Off = the Rate knob)");
    addCaption("LFO Sync", gLev, 10, 76, 84);
    addKnob(P::MOD_VCO, "Lever>VCO", gLev, 100, 34, 66, 62)->setTooltip("Mod wheel adds LFO vibrato, this deep");
    addKnob(P::MOD_VCF, "Lever>VCF", gLev, 168, 34, 66, 62)->setTooltip("Mod wheel adds LFO filter wobble, this deep");
    addKnob(P::ARP_GATE, "Gate", gGrv, 10, 34, 66, 62)->setTooltip("Arp note length (full = runs into the next step)");
    addKnob(P::ARP_SWING, "Swing", gGrv, 84, 34, 66, 62)->setTooltip("Every second arp step comes late");
    scope = std::make_unique<OutputScope>(proc);
    gOut->addAndMakeVisible(*scope);
    scope->setBounds(10, 30, 406, 92);

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

juce::Label* ErsaContent::addCaption(const juce::String& text, juce::Component* parent,
                                     int x, int y, int w)
{
    auto* l = new juce::Label(text, text);
    labels.emplace_back(l);
    parent->addAndMakeVisible(l);
    l->setBounds(x, y, w, 14);
    l->setJustificationType(juce::Justification::centred);
    l->setColour(juce::Label::textColourId, ErsaColors::textDim);
    l->setFont(lnf.uiFont(12.5f));
    return l;
}

juce::Slider* ErsaContent::addKnob(const juce::String& param, const juce::String& label,
                                   juce::Component* parent, int x, int y, int w, int h)
{
    auto* s = new juce::Slider();
    knobs.emplace_back(s);
    parent->addAndMakeVisible(s);
    s->setBounds(x, y, w, h - 16);
    s->setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s->setLookAndFeel(&lnf);
    if (auto* par = proc.apvts.getParameter(param))
    {
        sAtt.add(new juce::AudioProcessorValueTreeState::SliderAttachment(proc.apvts, param, *s));
        // live value bubble while hovering / dragging; double-click resets
        s->setPopupDisplayEnabled(true, true, this);
        s->setDoubleClickReturnValue(true, par->convertFrom0to1(par->getDefaultValue()));
    }
    auto* l = new juce::Label(param + "_l", label);
    labels.emplace_back(l);
    parent->addAndMakeVisible(l);
    l->setBounds(x, y + h - 16, w, 14);
    l->setJustificationType(juce::Justification::centred);
    l->setColour(juce::Label::textColourId, ErsaColors::textDim);
    l->setFont(lnf.uiFont(12.5f));
    return s;
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
    g.setColour(juce::Colour(0xff15161a));
    g.fillRoundedRectangle(r, 4.0f);
    // meters: two bars on the left, 0 dBFS at the top, -48 dB at the bottom
    auto mr = r.removeFromLeft(26.0f).reduced(4.0f, 6.0f);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto bar = mr.withWidth(7.0f).translated(ch * 10.0f, 0.0f);
        g.setColour(juce::Colour(0xff2c2c33));
        g.fillRect(bar);
        const float db = juce::Decibels::gainToDecibels(proc.getMeter(ch), -48.0f);
        const float h = bar.getHeight() * (db + 48.0f) / 48.0f;
        g.setColour(db > -1.0f ? ErsaColors::red : ErsaColors::accentGreen);
        g.fillRect(bar.withTop(bar.getBottom() - h));
    }
    // scope: the newest 512 samples, started at a rising zero crossing so a
    // steady note stands still
    float tmp[Ersa8Processor::kScopeSize];
    proc.copyScope(tmp, Ersa8Processor::kScopeSize);
    int start = 0;
    for (int i = 1; i < Ersa8Processor::kScopeSize - 512; ++i)
        if (tmp[i - 1] <= 0.0f && tmp[i] > 0.0f) { start = i; break; }
    auto sr = r.reduced(6.0f, 6.0f);
    g.setColour(juce::Colour(0xff2c2c33));
    g.drawHorizontalLine((int)sr.getCentreY(), sr.getX(), sr.getRight());
    juce::Path path;
    for (int i = 0; i < 512; ++i)
    {
        const float x = sr.getX() + sr.getWidth() * (float)i / 511.0f;
        const float y = sr.getCentreY() - juce::jlimit(-1.0f, 1.0f, tmp[start + i]) * sr.getHeight() * 0.5f;
        if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
    }
    g.setColour(ErsaColors::accentWhite.withAlpha(0.85f));
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
    // header strip
    g.setColour(ErsaColors::header);
    g.fillRect(0, 0, getWidth(), 56);
    g.setColour(ErsaColors::orange);
    g.fillRect(0, 56, getWidth(), 2);
    g.setColour(ErsaColors::text);
    g.setFont(lnf.logoFont(28.0f));
    g.drawText("E R S A", 16, 6, 220, 32, juce::Justification::centredLeft, false);
    g.setColour(ErsaColors::textDim);
    g.setFont(lnf.uiFont(11.5f));
    g.drawText("DUAL-VCO POLYSYNTH", 18, 34, 220, 14, juce::Justification::centredLeft, false);
}

void ErsaContent::resized()
{
    presetBtn.setBounds(940, 14, 196, 28);
    prevBtn.setBounds(900, 14, 32, 28);
    nextBtn.setBounds(1144, 14, 32, 28);
    initBtn.setBounds(852, 14, 40, 28);
    panicBtn.setBounds(1184, 14, 48, 28);
    undoBtn.setBounds(250, 14, 52, 28);
    redoBtn.setBounds(306, 14, 52, 28);
    randBtn.setBounds(640, 14, 54, 28);
    mutateBtn.setBounds(698, 14, 70, 28);
    locksBtn.setBounds(772, 14, 60, 28);
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
