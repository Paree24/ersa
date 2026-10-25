#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Parameters.h"
#include "DSPEngine.h"
#include "Presets.h"
#include "SnapshotUndo.h"
#include "MidiLearn.h"

class Ersa8Processor : public juce::AudioProcessor
{
public:
    Ersa8Processor();
    ~Ersa8Processor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ERSA"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.5; }

    int getNumPrograms() override { return juce::jmax(1, (int)getFactoryPresets().size()); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    pastel::MidiLearn midiLearn { *this, "ERSA" }; // global CC map + preset knob
    void loadPreset(int index);
    void loadInit(); // neutral starting point (all defaults), keeps program number
    void panic();    // all-silence: kills voices, latch, FX tails (applied at the next audio block)
    juce::String getPresetName(int i) const;

    // ---- undo / redo (message thread) ----
    // Knob / switch gestures are one step each; preset loads, Init and
    // randomize / mutate wrap their parameter writes in undoStep().
    std::unique_ptr<SnapshotUndo::Transaction> undoStep()
    {
        return undo != nullptr ? std::make_unique<SnapshotUndo::Transaction>(*undo) : nullptr;
    }
    bool canUndo() const { return undo != nullptr && undo->canUndo(); }
    bool canRedo() const { return undo != nullptr && undo->canRedo(); }
    void undoEdit() { if (undo != nullptr) undo->undo(); }
    void redoEdit() { if (undo != nullptr) undo->redo(); }

    // ---- randomize / mutate with section locks (one undo step each) ----
    // Only knob positions move: the signal path never changes. Volume,
    // tune, bend, portamento, voice mode, VCA mode and FX bypass are never touched.
    enum RandGroup { RG_VCO, RG_VCF, RG_ENV, RG_LFO, RG_ARP, RG_FX, RG_COUNT };
    static juce::StringArray randGroupNames() { return { "VCOs + Mix", "VCF", "Envelopes", "LFO + Mod", "Arp", "Effects" }; }
    static int paramGroup(const juce::String& id); // -1 = never randomized
    bool isGroupLocked(int g) const { return ((int)apvts.state.getProperty("randLocks", 1 << RG_ARP) >> g) & 1; }
    void setGroupLocked(int g, bool locked);
    void randomize();
    void mutate(float amount = 0.12f);

    // ---- output meter + scope (written by the audio thread, read by the UI) ----
    static constexpr int kScopeSize = 1024;
    float getMeter(int ch) const { return meter[ch & 1].load(); }
    void copyScope(float* dest, int n) const; // newest n samples, oldest first

private:
    SynthParams collectParams();
    void handleMidi(const juce::MidiMessage& msg);
    int allocateVoice(int note, bool monoLike);
    void arpStep(double stepSamples);
    void arpRelease();
    void updateHeld(int note, bool on);
    void releaseNote(int note, const SynthParams& tmp);
    void clearKeyState();
    static void voicePanGains(int note, float& gL, float& gR)
    {
        float pan = juce::jlimit(-1.0f, 1.0f, ((float)note - 60.0f) / 36.0f) * 0.35f;
        gL = std::cos((pan + 1.0f) * 0.78539818f);
        gR = std::sin((pan + 1.0f) * 0.78539818f);
    }
    // freeze the given voice's current output so a pitch snapping to a new
    // note crossfades from it (~3 ms) instead of stepping
    void freezeVoice(int idx)
    {
        float oL, oR;
        voicePanGains(voices[(size_t)idx].note, oL, oR);
        stealBufL[idx] = voices[(size_t)idx].lastOut() * oL * 0.7f;
        stealBufR[idx] = voices[(size_t)idx].lastOut() * oR * 0.7f;
        stealLeft[idx] = 140;
    }

    static constexpr int kVoices = 8;
    std::array<SynthVoice, kVoices> voices;
    GlobalLFO lfo;
    juce::Random rng;

    // FX
    ChorusFX chorus;
    PhaserFX phaser;
    DelayFX delay;
    ModVerb reverb;
    SatFX sat;
    juce::dsp::IIR::Filter<float> lowShelf, highShelf;
    juce::dsp::IIR::Filter<float> subHP, subHP2; // 20 Hz cleanup: kills DC + subsonics
    juce::dsp::ProcessSpec spec {};
    double cachedSr = 44100.0;
    float lfoSm = 0.0f; // tiny slew on LFO output (removes saw/square wrap thumps)
    float limGain = 1.0f, limPeak = 0.0f; // master transient limiter state
    int idleSamples = 0;   // samples since last active voice (for idle gate)
    float gateG = 1.0f;    // idle gate gain (1 open, ramps to 0 after long silence)
    // per-sample smoother states (kill zipper noise / preset-change bursts)
    float smCut = 3500.0f, smRes = 0.25f, smL1 = 0.8f, smL2 = 0.8f;
    float smPw1 = 0.5f, smPw2 = 0.5f, smVol = 0.8f, smT2 = 4.0f;
    float smMix1 = 0.92f, smMix2 = 0.92f, smXmod = 0.0f, smDrive = 0.0f;
    bool lastArp = false, lastHold = false;

    // Arp / performance state
    std::vector<int> heldNotes;      // unique, in key-down order
    std::vector<int> arpNotes;       // expanded w/ octaves
    int arpStepIdx = 0;
    double arpSamplesToNext = 0.0;
    int arpCurrentNote = -1;
    bool arpNoteActive = false;
    int arpVoice = 0;          // voice the current arp step sounds on
    double arpGateLeft = 0.0;  // samples until the step's gate ends (0 = runs to the next step)
    bool arpOddStep = false;   // swing: odd steps are the short ones
    // host sync: while the transport plays, steps come from the DAW's beat
    // position (slot = step number counted from the phrase anchor), not a free counter
    juce::int64 arpSlot = 0;
    bool arpSynced = false;      // the previous block ran on the host grid
    int arpSlotDiv = -1;         // division the slot numbers were counted in
    double arpAnchor = 0.0;      // host beat the pattern's step 0 sits on (phrase start)
    double arpExpectPpq = 0.0;   // where this block should start if the transport ran on
    bool arpPhraseStart = false; // first key of a phrase: (re)start the pattern
    float heldVel[128] = {};   // key velocities (arp plays them)
    int nextVoice = 0;         // rotating voice assignment
    int lastPolyVoice = -1;    // poly portamento source: the voice of the last note played
    // steal crossfade state per voice slot
    float stealBufL[kVoices] = {};
    float stealBufR[kVoices] = {};
    int stealLeft[kVoices] = {};
    double hostBpm = 120.0;
    // performance controls: hold pedal (CC64) and the MOD lever (CC1)
    bool keyDown[128] = {};    // physical key state
    bool sustained[128] = {};  // key up while the pedal was down: still sounding
    bool sustainDown = false;
    float modLever = 0.0f, modLeverSm = 0.0f; // 0..1, smoothed in the render loop
    float bendSemis = 0.0f;
    float bendSm = 0.0f; // smoothed bend for held voices (note-ons use raw = instant)
    int transposeOct = 0;

    int currentPreset = 0;
    int lastVoiceMode = -1;
    // audio-thread scratch + deferred UI requests
    std::vector<float> mixL, mixR;
    float lastEqLow = 1.0e9f, lastEqHigh = 1.0e9f;
    std::atomic<bool> panicRequest { false }, initRequest { false };
    void doPanic();
    void doInitReset();
    std::unique_ptr<SnapshotUndo> undo;
    std::atomic<float> meter[2] {};
    float scopeBuf[kScopeSize] = {};
    std::atomic<int> scopePos { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Ersa8Processor)
};
