#include "PluginProcessor.h"
#include "PluginEditor.h"

// ---------------- parameter layout ----------------
static juce::StringArray choiceLabels(const std::vector<const char*>& v)
{
    juce::StringArray s; for (auto c : v) s.add(c); return s;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto af = [&](const char* id, const char* name, float lo, float hi, float def)
    { p.push_back(std::make_unique<juce::AudioParameterFloat>(id, name, lo, hi, def)); };
    auto cf = [&](const char* id, const char* name, std::vector<const char*> labels, int def)
    { p.push_back(std::make_unique<juce::AudioParameterChoice>(id, name, choiceLabels(labels), def)); };
    auto bf = [&](const char* id, const char* name, bool def)
    { p.push_back(std::make_unique<juce::AudioParameterBool>(id, name, def)); };

    // VCO1
    cf(P::VCO1_WAVE, "VCO1 Wave", {"Triangle","Saw","Pulse","Square"}, 1);
    cf(P::VCO1_RANGE, "VCO1 Octave", {"-2","-1","0","+1","+2"}, 2);
    af(P::VCO1_LEVEL, "VCO1 Level", 0.0f, 1.0f, 0.8f);
    af(P::VCO1_PW, "VCO1 PW", 0.05f, 0.95f, 0.5f);
    af(P::VCO1_PWM, "VCO1 PWM", 0.0f, 1.0f, 0.0f);
    // VCO2
    cf(P::VCO2_WAVE, "VCO2 Wave", {"Saw","Pulse","Square","Sine","Noise"}, 0);
    cf(P::VCO2_RANGE, "VCO2 Octave", {"-2","-1","0","+1","+2"}, 2);
    af(P::VCO2_LEVEL, "VCO2 Level", 0.0f, 1.0f, 0.8f);
    af(P::VCO2_PW, "VCO2 PW", 0.05f, 0.95f, 0.5f);
    af(P::VCO2_PWM, "VCO2 PWM", 0.0f, 1.0f, 0.0f);
    af(P::VCO2_TUNE, "VCO2 Tune", -100.0f, 100.0f, 4.0f);
    cf(P::U1, "VCO1 Unison", {"1","2","3","4"}, 0);
    cf(P::U2, "VCO2 Unison", {"1","2","3","4"}, 0);
    af(P::MIXC, "Osc Mix", 0.0f, 1.0f, 0.5f);
    bf(P::SYNC, "Sync", false);
    af(P::XMOD, "X-Mod", 0.0f, 1.0f, 0.0f);
    // HPF / VCF
    cf(P::HPF, "HPF", {"Off","1","2","3"}, 0);
    af(P::CUTOFF, "Cutoff", 40.0f, 18000.0f, 3500.0f);
    af(P::RESO, "Resonance", 0.0f, 0.95f, 0.25f);
    af(P::FENV, "Filter Env", -1.0f, 1.0f, 0.6f);
    af(P::FLFO, "Filter LFO", 0.0f, 1.0f, 0.0f);
    af(P::KEYTRK, "Key Track", 0.0f, 1.0f, 0.5f);
    cf(P::SLOPE, "Slope", {"12dB","24dB"}, 1);
    // Envelopes
    af(P::FA, "Filter Attack", 0.001f, 4.0f, 0.01f);
    af(P::FD, "Filter Decay", 0.01f, 8.0f, 0.35f);
    af(P::FS, "Filter Sustain", 0.0f, 1.0f, 0.5f);
    af(P::FR, "Filter Release", 0.01f, 10.0f, 0.4f);
    af(P::AA, "Amp Attack", 0.001f, 4.0f, 0.01f);
    af(P::AD, "Amp Decay", 0.01f, 8.0f, 0.15f);
    af(P::AS, "Amp Sustain", 0.0f, 1.0f, 1.0f);
    af(P::AR, "Amp Release", 0.01f, 10.0f, 0.25f);
    // LFO
    af(P::LFO_RATE, "LFO Rate", 0.05f, 30.0f, 4.0f);
    cf(P::LFO_WAVE, "LFO Wave", {"Triangle","Sine","Saw","Square","S&H"}, 0);
    af(P::LFO_DELAY, "LFO Delay", 0.0f, 3.0f, 0.0f);
    af(P::LFO_VCO1, "LFO>VCO1", 0.0f, 1.0f, 0.0f);
    af(P::LFO_VCO2, "LFO>VCO2", 0.0f, 1.0f, 0.0f);
    af(P::LFO_VCF, "LFO>VCF", 0.0f, 1.0f, 0.0f);
    // Master
    cf(P::VCA_MODE, "VCA Mode", {"Gate","Env"}, 1);
    af(P::VOLUME, "Volume", 0.0f, 1.2f, 0.8f);
    af(P::TUNE, "Master Tune", -100.0f, 100.0f, 0.0f);
    cf(P::BEND, "Bend Range", {"Off","2st","7st","12st"}, 1);
    af(P::PORTA, "Portamento", 0.0f, 1.0f, 0.0f);
    cf(P::VOICEMODE, "Voice Mode", {"Poly","Mono","Legato"}, 0);
    af(P::UIDET, "Unison Detune", 0.0f, 0.6f, 0.15f);
    af(P::VELSENS, "Velocity", 0.0f, 1.0f, 0.5f);
    // Arp
    bf(P::ARP_ON, "Arp On", false);
    cf(P::ARP_MODE, "Arp Mode", {"Up","Down","UpDown","Random"}, 0);
    cf(P::ARP_DIV, "Arp Division", {"1/2D","1/2","1/4D","1/2T","1/4","1/8D","1/4T","1/8","1/16D","1/8T","1/16","1/16T","1/32"}, 10);
    cf(P::ARP_OCT, "Arp Octave", {"1","2","3","4"}, 1);
    bf(P::ARP_HOLD, "Arp Hold", false);
    // Chorus
    bf(P::CH_ON, "Chorus On", true);
    cf(P::CH_MODE, "Chorus Mode", {"Manual","Vintage I","Vintage II","Wide"}, 1);
    af(P::CH_RATE, "Chorus Rate", 0.05f, 8.0f, 0.6f);
    af(P::CH_DEPTH, "Chorus Depth", 0.0f, 1.0f, 0.5f);
    af(P::CH_MIX, "Chorus Mix", 0.0f, 1.0f, 0.4f);
    // Phaser
    bf(P::PH_ON, "Phaser On", false);
    af(P::PH_RATE, "Phaser Rate", 0.05f, 8.0f, 0.5f);
    af(P::PH_DEPTH, "Phaser Depth", 0.0f, 1.0f, 0.5f);
    af(P::PH_FB, "Phaser FB", 0.0f, 0.9f, 0.4f);
    af(P::PH_MIX, "Phaser Mix", 0.0f, 1.0f, 0.4f);
    // Delay
    bf(P::DL_ON, "Delay On", false);
    af(P::DL_TIME, "Delay Time", 20.0f, 1500.0f, 350.0f);
    af(P::DL_FB, "Delay FB", 0.0f, 0.85f, 0.35f);
    af(P::DL_MIX, "Delay Mix", 0.0f, 1.0f, 0.25f);
    cf(P::DL_SYNC, "Delay Sync", {"Free","1/8","1/8D","1/4","1/2"}, 0);
    // Saturator (master glue)
    bf(P::ST_ON, "Saturator On", false);
    af(P::ST_AMT, "Sat Amount", 0.0f, 1.0f, 0.3f);
    cf(P::ST_MODE, "Sat Mode", {"Tape","Tube"}, 0);
    // Reverb
    bf(P::RV_ON, "Reverb On", false);
    af(P::RV_SIZE, "Reverb Size", 0.0f, 1.0f, 0.5f);
    af(P::RV_DAMP, "Reverb Damp", 0.0f, 1.0f, 0.5f);
    af(P::RV_MIX, "Reverb Mix", 0.0f, 1.0f, 0.25f);
    // Drive / EQ
    af(P::DRIVE, "Drive", 0.0f, 1.0f, 0.0f);
    af(P::EQLOW, "EQ Low", -12.0f, 12.0f, 0.0f);
    af(P::EQHIGH, "EQ High", -12.0f, 12.0f, 0.0f);
    bf(P::FXBYPASS, "FX Bypass", false);

    return { p.begin(), p.end() };
}

// ---------------- processor ----------------
Ersa8Processor::Ersa8Processor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "ERSA", createParameterLayout())
{
    rng.setSeedRandomly();
    loadPreset(0); // boot to preset 000, not the init patch (state restore overrides after)
}

Ersa8Processor::~Ersa8Processor() {}

void Ersa8Processor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    cachedSr = sampleRate > 0.0 ? sampleRate : 44100.0;
    spec.sampleRate = cachedSr;
    spec.maximumBlockSize = (juce::uint32)samplesPerBlock;
    spec.numChannels = 2;
    for (auto& v : voices) v.prepare(sampleRate);
    lfo.setSampleRate(sampleRate);
    chorus.prepare(sampleRate, samplesPerBlock);
    phaser.prepare(sampleRate);
    delay.prepare(sampleRate);
    reverb.prepare(sampleRate);
    sat.prepare(sampleRate);
    lowShelf.coefficients  = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, 220.0f, 0.7f, 1.0f);
    highShelf.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 5200.0f, 0.7f, 1.0f);
    lowShelf.prepare(spec); highShelf.prepare(spec);
    subHP.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 20.0f, 0.7071f);
    subHP.prepare(spec);
    subHP2.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 20.0f, 0.7071f);
    subHP2.prepare(spec);
    lfoSm = 0.0f;
    // snap smoothers to current values so the first block is already settled
    SynthParams d0 = collectParams();
    smCut = d0.cutoff; smRes = d0.reso; smL1 = d0.vco1level; smL2 = d0.vco2level;
    smPw1 = d0.vco1pw; smPw2 = d0.vco2pw; smVol = d0.volume; smT2 = d0.vco2tune;
    smMix1 = d0.mix1; smMix2 = d0.mix2; smXmod = d0.xmod; smDrive = d0.drive;
    limGain = 1.0f; limPeak = 0.0f;
    bendSm = bendSemis;
}

void Ersa8Processor::releaseResources() {}

SynthParams Ersa8Processor::collectParams()
{
    SynthParams p;
    auto getF = [&](const char* id){ return apvts.getRawParameterValue(id)->load(); };
    auto getI = [&](const char* id){ return (int)std::round(apvts.getRawParameterValue(id)->load()); };
    auto getB = [&](const char* id){ return apvts.getRawParameterValue(id)->load() > 0.5f; };
    p.vco1wave=getI(P::VCO1_WAVE); p.vco1range=getI(P::VCO1_RANGE);
    p.vco1level=getF(P::VCO1_LEVEL); p.vco1pw=getF(P::VCO1_PW); p.vco1pwm=getF(P::VCO1_PWM);
    p.vco2wave=getI(P::VCO2_WAVE); p.vco2range=getI(P::VCO2_RANGE);
    p.vco2level=getF(P::VCO2_LEVEL); p.vco2pw=getF(P::VCO2_PW); p.vco2pwm=getF(P::VCO2_PWM);
    p.vco2tune=getF(P::VCO2_TUNE); p.sync=getB(P::SYNC); p.xmod=getF(P::XMOD);
    p.u1 = juce::jlimit(1, 4, getI(P::U1) + 1);
    p.u2 = juce::jlimit(1, 4, getI(P::U2) + 1);
    { // equal-power osc balance + makeup
        float m = juce::jlimit(0.0f, 1.0f, getF(P::MIXC));
        p.mix1 = std::cos(m * juce::MathConstants<float>::halfPi) * 1.3f;
        p.mix2 = std::sin(m * juce::MathConstants<float>::halfPi) * 1.3f;
        // per-tap unison detune multipliers, each osc centered on its OWN tap
        // count (linear cents approx, inaudible error) — osc2 mirrored
        p.tapN = juce::jmax(p.u1, p.u2);
        float spread = 4.0f + getF(P::UIDET) * 46.0f;
        for (int u = 0; u < 4; ++u)
        {
            float pos1 = (p.u1 <= 1) ? 0.5f : (float)u / (float)(p.u1 - 1);
            float pos2 = (p.u2 <= 1) ? 0.5f : (float)u / (float)(p.u2 - 1);
            float du1 = (pos1 - 0.5f) * 2.0f * spread;
            float du2 = (pos2 - 0.5f) * 2.0f * spread;
            p.tapM1[u] = 1.0f + du1 * 0.0005776f;
            p.tapM2[u] = 1.0f - du2 * 0.0005776f;
        }
    }
    p.hpf=getI(P::HPF);
    p.cutoff=getF(P::CUTOFF); p.reso=getF(P::RESO); p.fenv=getF(P::FENV);
    p.flfo=getF(P::LFO_VCF); p.keytrk=getF(P::KEYTRK); p.slope=getI(P::SLOPE);
    p.fa=getF(P::FA); p.fd=getF(P::FD); p.fs=getF(P::FS); p.fr=getF(P::FR);
    p.aa=getF(P::AA); p.ad=getF(P::AD); p.as=getF(P::AS); p.ar=getF(P::AR);
    p.lfoRate=getF(P::LFO_RATE); p.lfoWave=getI(P::LFO_WAVE); p.lfoDelay=getF(P::LFO_DELAY);
    p.lfoVco1=getF(P::LFO_VCO1); p.lfoVco2=getF(P::LFO_VCO2); p.lfoVcf=getF(P::LFO_VCF);
    p.vcaMode=getI(P::VCA_MODE);
    p.volume=getF(P::VOLUME); p.tune=getF(P::TUNE);
    { int b=getI(P::BEND); p.bendRange = (b==0?0:(b==1?2:(b==2?7:12))); }
    p.porta=getF(P::PORTA); p.voiceMode=getI(P::VOICEMODE);
    p.uidet=getF(P::UIDET); p.velsens=getF(P::VELSENS);
    p.chOn=getB(P::CH_ON); p.chMode=getI(P::CH_MODE); p.chRate=getF(P::CH_RATE);
    p.chDepth=getF(P::CH_DEPTH); p.chMix=getF(P::CH_MIX);
    p.phOn=getB(P::PH_ON); p.phRate=getF(P::PH_RATE); p.phDepth=getF(P::PH_DEPTH);
    p.phFb=getF(P::PH_FB); p.phMix=getF(P::PH_MIX);
    p.dlOn=getB(P::DL_ON); p.dlTime=getF(P::DL_TIME); p.dlFb=getF(P::DL_FB); p.dlMix=getF(P::DL_MIX);
    p.dlSync=getI(P::DL_SYNC);
    p.rvOn=getB(P::RV_ON); p.rvSize=getF(P::RV_SIZE); p.rvDamp=getF(P::RV_DAMP); p.rvMix=getF(P::RV_MIX);
    p.stOn=getB(P::ST_ON); p.stAmt=getF(P::ST_AMT); p.stMode=getI(P::ST_MODE);
    p.drive=getF(P::DRIVE); p.eqLow=getF(P::EQLOW); p.eqHigh=getF(P::EQHIGH);
    p.fxBypass=getB(P::FXBYPASS);
    p.pitchBend = bendSemis;
    return p;
}

void Ersa8Processor::updateHeld(int note, bool on)
{
    if (on) { if (std::find(heldNotes.begin(), heldNotes.end(), note) == heldNotes.end()) heldNotes.push_back(note); }
    else { heldNotes.erase(std::remove(heldNotes.begin(), heldNotes.end(), note), heldNotes.end()); }
    std::sort(heldNotes.begin(), heldNotes.end());
}

int Ersa8Processor::allocateVoice(int note, bool monoLike)
{
    if (monoLike)
    {
        if (voices[0].active) return 0;
        voices[0].active = true; return 0;
    }
    // existing voice with same note? retrigger
    for (int i = 0; i < kVoices; ++i)
        if (voices[i].active && voices[i].note == note) return i;
    // free voice
    for (int i = 0; i < kVoices; ++i)
        if (!voices[i].active) return i;
    // steal the quietest voice (prefer already-released ones): smallest output step
    int best = 0; float bestLevel = 1e9f;
    for (int i = 0; i < kVoices; ++i)
    {
        float l = voices[i].ampLevel();
        if (!voices[i].isReleasing()) l += 2.0f;
        if (l < bestLevel) { bestLevel = l; best = i; }
    }
    return best;
}

void Ersa8Processor::handleMidi(const juce::MidiMessage& msg)
{
    SynthParams tmp = collectParams();
    bool monoLike = (tmp.voiceMode != 0);

    if (msg.isNoteOn())
    {
        int n = msg.getNoteNumber();
        float vel = msg.getFloatVelocity();
        bool arpOn = apvts.getRawParameterValue(P::ARP_ON)->load() > 0.5f;
        if (arpOn) { updateHeld(n, true); }
        else if (tmp.voiceMode == 2) // legato
        {
            bool anyHeld = !heldNotes.empty();
            updateHeld(n, true);
            bool wasActive = voices[0].active;
            // glide when sliding between held notes; always re-articulate a fresh attack
            voices[0].noteOn(n, vel, tmp, wasActive, !anyHeld || !wasActive);
        }
        else if (monoLike)
        {
            // true legato only when another key is physically held; staccato
            // notes always re-articulate (pitch may still glide via portamento)
            bool trueLegato = !heldNotes.empty();
            updateHeld(n, true);
            bool wasActive = voices[0].active;
            bool glide = wasActive && tmp.porta > 0.003f;
            if (wasActive && !glide) freezeVoice(0); // snapping pitch: blend, don't step
            voices[0].noteOn(n, vel, tmp, glide, !trueLegato || !wasActive);
        }
        else
        {
            int idx = allocateVoice(n, false);
            if (voices[idx].active) freezeVoice(idx); // stealing: blend from old sound
            else stealLeft[idx] = 0;
            voices[idx].age = 0;
            voices[idx].noteOn(n, vel, tmp, false, true);
            for (auto& v : voices) v.age++;
        }
    }
    else if (msg.isNoteOff())
    {
        int n = msg.getNoteNumber();
        bool arpOn = apvts.getRawParameterValue(P::ARP_ON)->load() > 0.5f;
        bool hold = apvts.getRawParameterValue(P::ARP_HOLD)->load() > 0.5f;
        if (arpOn) { if (!hold) updateHeld(n, false); }
        else if (tmp.voiceMode == 2 || monoLike)
        {
            updateHeld(n, false);
            if (!heldNotes.empty())
            {
                int last = heldNotes.back();
                voices[0].note = last;
                voices[0].setTarget((float)last + bendSemis + tmp.tune / 100.0f);
            }
            else voices[0].noteOff();
        }
        else
        {
            updateHeld(n, false);
            for (auto& v : voices) if (v.active && v.note == n) v.noteOff();
        }
    }
    else if (msg.isPitchWheel())
    {
        int range = tmp.bendRange;
        if (range == 0) { bendSemis = 0.0f; return; }
        int v = msg.getPitchWheelValue();
        // center deadband: worn/noisy wheels hover a few LSBs off 8192; without
        // this the synth sits micro-detuned, and each jitter event used to snap
        // voice pitch instantly (= laser-zap). Inside the band we pin exact 0.
        if (std::abs(v - 8192) <= 48) v = 8192;
        bendSemis = ((float)v - 8192.0f) / 8192.0f * (float)range;
        // NOTE: no immediate setTarget here on purpose. Held voices pick the
        // bend up via the smoothed bendSm path in the render loop (~8 ms glide),
        // so wheel jitter can never step the pitch discontinuously.
    }
    else if (msg.isAllNotesOff() || msg.isAllSoundOff())
    {
        heldNotes.clear();
        for (auto& v : voices) v.noteOff();
        if (arpNoteActive) { voices[0].noteOff(); arpNoteActive = false; }
    }
}

void Ersa8Processor::arpStep()
{
    if (heldNotes.empty()) { if (arpNoteActive) { voices[0].noteOff(); arpNoteActive = false; arpCurrentNote = -1; } return; }
    SynthParams p = collectParams();
    int octs = 1 + (int)std::round(apvts.getRawParameterValue(P::ARP_OCT)->load());
    int mode = (int)std::round(apvts.getRawParameterValue(P::ARP_MODE)->load());
    std::vector<int> seq;
    auto pushOct = [&](const std::vector<int>& base){
        for (int o = 0; o < octs; ++o)
            for (int n : base) seq.push_back(std::min(127, n + o * 12));
    };
    if (mode == 0) pushOct(heldNotes);
    else if (mode == 1) { std::vector<int> d = heldNotes; std::reverse(d.begin(), d.end()); pushOct(d); }
    else if (mode == 2)
    {
        std::vector<int> ud = heldNotes;
        for (int i = (int)heldNotes.size() - 2; i > 0; --i) ud.push_back(heldNotes[(size_t)i]);
        pushOct(ud);
    }
    else pushOct(heldNotes); // random handled below
    if (seq.empty()) return;
    int idx;
    if (mode == 3) idx = (int)(rng.nextFloat() * (float)seq.size());
    else idx = arpStepIdx++ % (int)seq.size();
    int n = seq[(size_t)idx];
    bool wasActive = voices[0].active;
    if (arpNoteActive) voices[0].noteOff();
    // every arp step is a fresh articulation (pitch may glide when portamento set)
    bool glide = arpNoteActive && p.porta > 0.003f;
    if (wasActive && !glide) freezeVoice(0);
    voices[0].noteOn(n, 0.9f, p, glide, true);
    voices[0].note = n;
    voices[0].setTarget((float)n + bendSemis + p.tune / 100.0f);
    arpNoteActive = true; arpCurrentNote = n;
}

void Ersa8Processor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    SynthParams p = collectParams();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (pos->getBpm()) hostBpm = *pos->getBpm();

    bool arpOn = apvts.getRawParameterValue(P::ARP_ON)->load() > 0.5f;
    bool holdNow = apvts.getRawParameterValue(P::ARP_HOLD)->load() > 0.5f;
    // host-synced divisions: steps per beat (straight, triplet, dotted),
    // sorted slow -> fast. NOTE: this order was resorted after release;
    // factory XMLs were migrated old->new, see commit.
    static constexpr float divMult[13] = { 0.3333f, 0.5f, 0.6667f, 0.75f, 1.0f, 1.3333f, 1.5f,
                                           2.0f, 2.6667f, 3.0f, 4.0f, 6.0f, 8.0f };
    int arpDiv = juce::jlimit(0, 12, (int)std::round(apvts.getRawParameterValue(P::ARP_DIV)->load()));

    // unlatch: releasing HOLD (or switching arp off) clears latched notes
    if (lastArp && !arpOn)
    {
        heldNotes.clear();
        if (arpNoteActive) { voices[0].noteOff(); arpNoteActive = false; arpCurrentNote = -1; }
    }
    else if (arpOn && lastHold && !holdNow)
    {
        heldNotes.clear();
        if (arpNoteActive) { voices[0].noteOff(); arpNoteActive = false; arpCurrentNote = -1; }
    }
    lastArp = arpOn; lastHold = holdNow;

    int numSamples = buffer.getNumSamples();
    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    // --- collect midi events with sample positions ---
    struct Ev { int pos; juce::MidiMessage msg; };
    std::vector<Ev> evs;
    for (auto m : midi) evs.push_back({ m.samplePosition, m.getMessage() });

    bool monoLike = (p.voiceMode != 0);
    // voice-mode switch with hanging notes: release everything so no voice
    // gets stuck in a routing that no longer plays it
    if (p.voiceMode != lastVoiceMode)
    {
        lastVoiceMode = p.voiceMode;
        heldNotes.clear();
        for (auto& v : voices) v.noteOff();
        if (arpNoteActive) { voices[0].noteOff(); arpNoteActive = false; arpCurrentNote = -1; }
    }

    size_t evIdx = 0;
    int arpCountdown = arpSamplesToNext;

    // per-sample smoothing coeffs (~8 ms) for click/zipper-prone params
    float smK = 1.0f - std::exp(-1.0f / (0.008f * (float)cachedSr));
    // LFO edge slew (~4 ms): keeps character, removes wrap discontinuities.
    // Slow squares still articulate (that's their job) but lose the hard edge.
    float lfoK = 1.0f - std::exp(-1.0f / (0.004f * (float)cachedSr));

    std::vector<float> mixL(numSamples, 0.0f), mixR(numSamples, 0.0f);

    // per-block smoother targets (snapshot BEFORE the sample loop: the loop
    // writes the smoothed values back into p.*, so reading p.* per-sample
    // would see (sm-sm)=0 and freeze the slew after one step per block).
    const float tCut = p.cutoff, tRes = p.reso;
    const float tL1 = p.vco1level, tL2 = p.vco2level;
    const float tPw1 = p.vco1pw, tPw2 = p.vco2pw;
    const float tVol = p.volume, tT2 = p.vco2tune;
    const float tMix1 = p.mix1, tMix2 = p.mix2;
    const float tXmod = p.xmod, tDrive = p.drive;

    for (int s = 0; s < numSamples; ++s)
    {
        while (evIdx < evs.size() && evs[evIdx].pos == s) handleMidi(evs[evIdx++].msg);

        if (arpOn)
        {
            if (arpCountdown <= 0)
            {
                arpStep();
                float stepsPerSec = (float)(hostBpm / 60.0) * divMult[arpDiv];
                arpCountdown = (int)(cachedSr / std::max(0.1f, stepsPerSec));
            }
            arpCountdown--;
        }

        // smooth continuous params toward their targets (preset/knob jumps glide).
        // NOTE: targets come from the per-block snapshot above, NOT from p.*
        // (the loop writes smoothed values back into p.*, so reading p.* here
        // would freeze every slew after a single sample-step per block — the
        // old seconds-long "laser" glides after every preset/knob change).
        smCut += (tCut - smCut) * smK;     p.cutoff = smCut;
        smRes += (tRes - smRes) * smK;        p.reso = smRes;
        smL1 += (tL1 - smL1) * smK;     p.vco1level = smL1;
        smL2 += (tL2 - smL2) * smK;     p.vco2level = smL2;
        smPw1 += (tPw1 - smPw1) * smK;      p.vco1pw = smPw1;
        smPw2 += (tPw2 - smPw2) * smK;      p.vco2pw = smPw2;
        smVol += (tVol - smVol) * smK;      p.volume = smVol;
        smT2 += (tT2 - smT2) * smK;      p.vco2tune = smT2;
        smMix1 += (tMix1 - smMix1) * smK;      p.mix1 = smMix1;
        smMix2 += (tMix2 - smMix2) * smK;      p.mix2 = smMix2;
        smXmod += (tXmod - smXmod) * smK;      p.xmod = smXmod;
        smDrive += (tDrive - smDrive) * smK;   p.drive = smDrive;
        bendSm += (bendSemis - bendSm) * smK;   p.pitchBend = bendSm;

        float lfoVal = lfo.next(p.lfoRate, p.lfoWave, p.lfoDelay, rng);
        lfoSm += (lfoVal - lfoSm) * lfoK;
        lfoVal = lfoSm;
        for (int vi = 0; vi < kVoices; ++vi)
        {
            auto& v = voices[(size_t)vi];
            if (!v.active) continue;
            float dummy = 0.0f;
            float vs = v.render(p, lfoVal, dummy) * 0.7f; // voice-sum compensation
            // subtle key-tracked spread: low notes left, high notes right
            float gL, gR;
            voicePanGains(v.note, gL, gR);
            // steal crossfade: blend from frozen old output instead of stepping
            float st = stealLeft[vi] > 0 ? (float)stealLeft[vi] / 140.0f : 0.0f;
            if (stealLeft[vi] > 0) stealLeft[vi]--;
            mixL[(size_t)s] += vs * gL * (1.0f - st) + stealBufL[vi] * st;
            mixR[(size_t)s] += vs * gR * (1.0f - st) + stealBufR[vi] * st;
        }
        // refresh per-sample-varying params cheaply
        for (auto& v : voices) if (v.active) v.setTarget((float)v.note + bendSm + p.tune / 100.0f);
    }
    arpSamplesToNext = arpCountdown;
    midi.clear();

    // stereo-ize + master
    for (int s = 0; s < numSamples; ++s)
    {
        float ml = mixL[(size_t)s] * p.volume * 0.85f;
        float mr = mixR[(size_t)s] * p.volume * 0.85f;
        // drive pre-FX (soft: moderate pre-gain, gentle post-trim)
        if (p.drive > 0.001f && !p.fxBypass)
        {
            ml = std::tanh(ml * (1.0f + p.drive * 4.0f)) / (1.0f + p.drive * 1.5f) * (1.0f + p.drive * 0.5f);
            mr = std::tanh(mr * (1.0f + p.drive * 4.0f)) / (1.0f + p.drive * 1.5f) * (1.0f + p.drive * 0.5f);
        }
        outL[s] = ml;
        if (outR) outR[s] = mr;
    }

    if (!p.fxBypass)
    {
        if (p.chOn) chorus.process(outL, outR ? outR : outL, numSamples, p.chRate, p.chDepth, p.chMix * 0.9f, p.chMode);
        if (p.phOn && outR) phaser.process(outL, outR, numSamples, p.phRate, p.phDepth, p.phFb, p.phMix);
        if (p.dlOn && outR)
        {
            float t = p.dlTime;
            if (p.dlSync != 0)
            {
                double beat = 60.0 / std::max(20.0, hostBpm);
                if (p.dlSync == 1) t = (float)(beat * 0.375 * 1000.0);
                else if (p.dlSync == 2) t = (float)(beat * 0.5625 * 1000.0);
                else if (p.dlSync == 3) t = (float)(beat * 0.75 * 1000.0);
                else t = (float)(beat * 1.5 * 1000.0);
            }
            delay.process(outL, outR, numSamples, t, p.dlFb, p.dlMix);
        }
        if (p.rvOn && outR)
            reverb.process(outL, outR, numSamples, p.rvSize, p.rvDamp, p.rvMix);
        if (p.stOn && outR)
            sat.process(outL, outR, numSamples, p.stAmt, 0.75f, 1.0f, p.stMode);
        // EQ post (uses rate cached from prepareToPlay — never trust getSampleRate here)
        lowShelf.coefficients  = juce::dsp::IIR::Coefficients<float>::makeLowShelf(cachedSr, 220.0f, 0.7f,
                            juce::Decibels::decibelsToGain(p.eqLow));
        highShelf.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(cachedSr, 5200.0f, 0.7f,
                            juce::Decibels::decibelsToGain(p.eqHigh));
        if (std::abs(p.eqLow) > 0.05f || std::abs(p.eqHigh) > 0.05f)
        {
            juce::dsp::AudioBlock<float> block(buffer);
            juce::dsp::ProcessContextReplacing<float> ctx(block);
            lowShelf.process(ctx); highShelf.process(ctx);
        }
        // subsonic cleanup last (pre-limiter): removes DC + rumble so the
        // limiter never folds inaudible lows into intermodulation grunge
        {
            juce::dsp::AudioBlock<float> block(buffer);
            juce::dsp::ProcessContextReplacing<float> ctx(block);
            subHP.process(ctx); subHP2.process(ctx);
        }
    }

    // safety: musical peak limiter (fast attack catches sweep bursts, slow
    // release avoids pumping) + tanh backstop so output is always bounded.
    // Stereo-linked: peak detected across channels, one shared gain trajectory.
    // Idle gate: after ~6 s with no active voice, ramp to exact digital zero
    // so idle output is bit-silent (no tail micro-ring, no denormal whisper).
    {
        float att = 1.0f - std::exp(-1.0f / (0.0003f * (float)cachedSr));
        float rel = 1.0f - std::exp(-1.0f / (0.080f * (float)cachedSr));
        float gateStep = 1.0f / (0.050f * (float)cachedSr);
        const float thr = 0.7f;
        bool anyActive = false;
        for (auto& v : voices) if (v.active) { anyActive = true; break; }
        if (anyActive) idleSamples = 0;
        else idleSamples += numSamples;
        bool shouldClose = idleSamples > (int)(6.0 * cachedSr);
        int nch = buffer.getNumChannels();
        for (int s = 0; s < numSamples; ++s)
        {
            if (shouldClose && gateG > 0.0f) gateG = juce::jmax(0.0f, gateG - gateStep);
            else if (!shouldClose) gateG = 1.0f;
            float a = 0.0f;
            for (int ch = 0; ch < nch; ++ch)
                a = juce::jmax(a, std::fabs(buffer.getSample(ch, s)));
            if (a > limPeak) limPeak = a;
            else limPeak += (0.0f - limPeak) * rel;
            float target = (limPeak > thr) ? thr / limPeak : 1.0f;
            limGain += (target - limGain) * (target < limGain ? att : rel);
            for (int ch = 0; ch < nch; ++ch)
                buffer.setSample(ch, s, std::tanh(buffer.getSample(ch, s) * limGain) * gateG);
        }
    }
}

// ---------------- state / programs ----------------
void Ersa8Processor::setCurrentProgram(int index)
{
    if (index >= 0 && index < (int)getFactoryPresets().size()) loadPreset(index);
}

const juce::String Ersa8Processor::getProgramName(int index) { return presetName(index); }

void Ersa8Processor::loadPreset(int index)
{
    int n = juce::jmax(1, (int)getFactoryPresets().size());
    currentPreset = juce::jlimit(0, n - 1, index);
    // reset every parameter to its creation default so programs are deterministic
    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
            ranged->setValueNotifyingHost(ranged->getDefaultValue());
    applyPreset(apvts, currentPreset);
}

void Ersa8Processor::loadInit()
{
    for (auto* param : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
            ranged->setValueNotifyingHost(ranged->getDefaultValue());
    // stop any hanging performance state so init is truly clean
    heldNotes.clear();
    bendSemis = 0.0f;
    for (auto& v : voices) v.noteOff();
    if (arpNoteActive) { voices[0].noteOff(); arpNoteActive = false; arpCurrentNote = -1; }
    delay.clear(); reverb.clear(); sat.reset(); chorus.clear(); phaser.clear();
}

void Ersa8Processor::panic()
{
    heldNotes.clear();
    bendSemis = 0.0f; bendSm = 0.0f;
    for (auto& v : voices) v.kill();
    arpNoteActive = false; arpCurrentNote = -1; arpStepIdx = 0; arpSamplesToNext = 0;
    for (int i = 0; i < kVoices; ++i) stealLeft[i] = 0;
    delay.clear(); reverb.clear(); sat.reset(); chorus.clear(); phaser.clear();
    lowShelf.reset(); highShelf.reset(); subHP.reset(); subHP2.reset();
}

juce::String Ersa8Processor::getPresetName(int i) const { return presetName(i); }

void Ersa8Processor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("prog", currentPreset, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

void Ersa8Processor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        auto vt = juce::ValueTree::fromXml(*xml);
        if (vt.isValid())
        {
            apvts.replaceState(vt);
            currentPreset = vt.getProperty("prog", 0);
        }
    }
}

juce::AudioProcessorEditor* Ersa8Processor::createEditor() { return new Ersa8Editor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Ersa8Processor(); }
