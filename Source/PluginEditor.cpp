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

    // ===== top row groups =====
    auto* gLfo   = addGroup("LFO", this, 760, 64, 132, 250, ErsaColors::accentYellow);
    auto* gVco1  = addGroup("VCO-1", this, 8, 64, 172, 250, ErsaColors::orange);
    auto* gVco2  = addGroup("VCO-2", this, 186, 64, 196, 250, ErsaColors::orange);
    auto* gMix   = addGroup("MIX / SYNC", this, 388, 64, 150, 250, ErsaColors::orange);
    auto* gVcf   = addGroup("VCF", this, 544, 64, 210, 250, ErsaColors::accentGreen);
    auto* gEnv   = addGroup("ENVELOPES / VCA", this, 898, 64, 334, 250, ErsaColors::accentYellow);

    // LFO (narrow group: small knobs so nothing overlaps)
    addKnob(P::LFO_RATE, "Rate", gLfo, 6, 28, 58, 58);
    addKnob(P::LFO_DELAY, "Delay", gLfo, 68, 28, 58, 58);
    addChoice(P::LFO_WAVE, gLfo, 11, 92, 110);
    addKnob(P::LFO_VCO1, "VCO1", gLfo, 6, 122, 58, 58);
    addKnob(P::LFO_VCO2, "VCO2", gLfo, 68, 122, 58, 58);
    addKnob(P::LFO_VCF, "VCF", gLfo, 37, 186, 58, 52);

    // VCO-1
    addChoice(P::VCO1_WAVE, gVco1, 31, 30, 110);
    auto* range1 = addChoice(P::VCO1_RANGE, gVco1, 31, 58, 110);
    range1->setTooltip("Oscillator octave: -2/-1 below, 0 = concert pitch, +1/+2 above");
    addCaption("Octave", gVco1, 31, 80, 110);
    addKnob(P::VCO1_LEVEL, "Level", gVco1, 8, 100);
    addKnob(P::VCO1_PW, "PW", gVco1, 100, 100);
    addKnob(P::VCO1_PWM, "PWM", gVco1, 8, 168, 64, 50);
    addKnob(P::U1, "Uni", gVco1, 100, 168, 64, 50);

    // VCO-2
    addChoice(P::VCO2_WAVE, gVco2, 43, 30, 110);
    auto* range2 = addChoice(P::VCO2_RANGE, gVco2, 43, 58, 110);
    range2->setTooltip("Oscillator octave: -2/-1 below, 0 = concert pitch, +1/+2 above");
    addCaption("Octave", gVco2, 43, 80, 110);
    addKnob(P::VCO2_LEVEL, "Level", gVco2, 8, 100);
    addKnob(P::VCO2_PW, "PW", gVco2, 124, 100);
    addKnob(P::VCO2_PWM, "PWM", gVco2, 8, 168, 60, 52);
    addKnob(P::VCO2_TUNE, "Tune", gVco2, 72, 168, 60, 52);
    addKnob(P::U2, "Uni", gVco2, 132, 168, 60, 52);

    // Mix / sync
    addKnob(P::MIXC, "Mix", gMix, 39, 30, 72, 72);
    addToggle(P::SYNC, "Sync", gMix, 8, 110, 64, 34);
    addCaption("HPF", gMix, 78, 104, 64);
    addChoice(P::HPF, gMix, 78, 120, 64);
    addKnob(P::XMOD, "XMod", gMix, 43, 168, 64, 56);

    // VCF
    addKnob(P::CUTOFF, "Cutoff", gVcf, 8, 30, 64, 62);
    addKnob(P::RESO, "Reso", gVcf, 76, 30, 64, 62);
    addKnob(P::FENV, "Env", gVcf, 142, 30, 60, 62);
    addKnob(P::KEYTRK, "KeyTrk", gVcf, 8, 110, 64, 62);
    addChoice(P::SLOPE, gVcf, 80, 116, 60);
    addKnob(P::VELSENS, "Veloc", gVcf, 142, 110, 60, 62);
    addKnob(P::PORTA, "Porta", gVcf, 8, 180, 60, 56);
    addChoice(P::VCA_MODE, gVcf, 76, 190, 60);
    addKnob(P::VOLUME, "Volume", gVcf, 140, 180, 62, 56);

    // Envelopes
    const char* fIds[4] = { P::FA, P::FD, P::FS, P::FR };
    const char* aIds[4] = { P::AA, P::AD, P::AS, P::AR };
    const char* fLbl[4] = { "F-Atk", "F-Dec", "F-Sus", "F-Rel" };
    const char* aLbl[4] = { "A-Atk", "A-Dec", "A-Sus", "A-Rel" };
    for (int i = 0; i < 4; ++i)
    {
        addKnob(fIds[i], fLbl[i], gEnv, 8 + i * 80, 30, 72, 62);
        addKnob(aIds[i], aLbl[i], gEnv, 8 + i * 80, 120, 72, 62);
    }
    addKnob(P::TUNE, "Tune", gEnv, 77, 190, 64, 52);
    addChoice(P::BEND, gEnv, 157, 196, 100);

    // ===== bottom row =====
    auto* gArp   = addGroup("ARPEGGIATOR / VOICE", this, 8, 322, 240, 250, ErsaColors::accentPink);
    auto* gCh    = addGroup("CHORUS", this, 254, 322, 160, 250, ErsaColors::accentBlue);
    auto* gPh    = addGroup("PHASER", this, 420, 322, 160, 250, ErsaColors::accentBlue);
    auto* gDl    = addGroup("DELAY", this, 586, 322, 176, 250, ErsaColors::accentBlue);
    auto* gRv    = addGroup("REVERB", this, 768, 322, 164, 250, ErsaColors::accentBlue);
    auto* gEq    = addGroup("MASTER", this, 938, 322, 294, 250, ErsaColors::accentWhite);

    // Arpeggiator + voice (host-synced divisions)
    addToggle(P::ARP_ON, "On", gArp, 8, 40, 60, 34);
    addToggle(P::ARP_HOLD, "Hold", gArp, 70, 40, 60, 34);
    addChoice(P::ARP_MODE, gArp, 135, 44, 95);
    addCaption("Rate", gArp, 8, 100, 140);
    addCaption("Oct", gArp, 156, 100, 76);
    addChoice(P::ARP_DIV, gArp, 8, 116, 140);
    addChoice(P::ARP_OCT, gArp, 156, 116, 76);
    addCaption("Voice", gArp, 8, 176, 140);
    addChoice(P::VOICEMODE, gArp, 8, 190, 140);
    addKnob(P::UIDET, "Detune", gArp, 156, 176, 68, 60);

    addToggle(P::CH_ON, "On", gCh, 8, 40, 60, 34);
    addChoice(P::CH_MODE, gCh, 72, 44, 80);
    addKnob(P::CH_RATE, "Rate", gCh, 8, 110, 68, 62);
    addKnob(P::CH_DEPTH, "Depth", gCh, 82, 110, 68, 62);
    addKnob(P::CH_MIX, "Mix", gCh, 45, 180, 68, 56);

    addToggle(P::PH_ON, "On", gPh, 8, 40, 60, 34);
    addKnob(P::PH_RATE, "Rate", gPh, 8, 110, 68, 62);
    addKnob(P::PH_DEPTH, "Depth", gPh, 82, 110, 68, 62);
    addKnob(P::PH_FB, "FB", gPh, 8, 180, 68, 56);
    addKnob(P::PH_MIX, "Mix", gPh, 82, 180, 68, 56);

    addToggle(P::DL_ON, "On", gDl, 8, 40, 60, 34);
    addChoice(P::DL_SYNC, gDl, 72, 44, 96);
    addKnob(P::DL_TIME, "Time", gDl, 28, 100, 56, 62);
    addKnob(P::DL_FB, "FB", gDl, 92, 100, 56, 62);
    addKnob(P::DL_MIX, "Mix", gDl, 60, 180, 56, 56);

    addToggle(P::RV_ON, "On", gRv, 8, 40, 60, 34);
    addKnob(P::RV_SIZE, "Size", gRv, 22, 100, 56, 62);
    addKnob(P::RV_DAMP, "Damp", gRv, 86, 100, 56, 62);
    addKnob(P::RV_MIX, "Mix", gRv, 54, 180, 56, 56);

    addKnob(P::EQLOW, "Low", gEq, 12, 46, 60, 78);
    addKnob(P::EQHIGH, "High", gEq, 84, 46, 60, 78);
    addKnob(P::ST_AMT, "Sat", gEq, 156, 46, 60, 78);
    addKnob(P::DRIVE, "Drive", gEq, 228, 46, 60, 78);
    addToggle(P::FXBYPASS, "FX Bypass", gEq, 12, 140, 90, 34);
    addToggle(P::ST_ON, "Sat On", gEq, 110, 140, 60, 34);
    addChoice(P::ST_MODE, gEq, 178, 144, 100);

    // preset browser overlay (created last: paints above everything)
    browser = std::make_unique<PresetBrowser>(proc, lnf);
    addAndMakeVisible(browser.get());
    browser->setVisible(false);
    browser->onClose = [&] { browser->setVisible(false); };
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
        sAtt.add(new juce::AudioProcessorValueTreeState::SliderAttachment(proc.apvts, param, *s));
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
    if (browser != nullptr)
        browser->setBounds(0, 0, baseW, baseH);
}

// ---------------- resizable host editor ----------------
Ersa8Editor::Ersa8Editor(Ersa8Processor& p)
    : juce::AudioProcessorEditor(p), content(p), corner(this, getConstrainer())
{
    setSize(ErsaContent::baseW, ErsaContent::baseH);
    setResizable(true, true);
    setResizeLimits(744, 352, 1984, 938); // 0.6x .. 1.6x
    addAndMakeVisible(content);
    addAndMakeVisible(corner);
}

Ersa8Editor::~Ersa8Editor() {}

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
