#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Parameters.h"
#include "DSPEngine.h"
#include "Presets.h"

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
    void loadPreset(int index);
    void loadInit(); // neutral starting point (all defaults), keeps program number
    void panic();    // immediate all-silence: kills voices, latch, FX tails
    juce::String getPresetName(int i) const;

private:
    SynthParams collectParams();
    void handleMidi(const juce::MidiMessage& msg);
    int allocateVoice(int note, bool monoLike);
    void arpStep();
    void updateHeld(int note, bool on);
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
    std::vector<int> heldNotes;      // sorted unique
    std::vector<int> arpNotes;       // expanded w/ octaves
    int arpStepIdx = 0;
    int arpSamplesToNext = 0;
    int arpCurrentNote = -1;
    bool arpNoteActive = false;
    // steal crossfade state per voice slot
    float stealBufL[kVoices] = {};
    float stealBufR[kVoices] = {};
    int stealLeft[kVoices] = {};
    double hostBpm = 120.0;
    float bendSemis = 0.0f;
    float bendSm = 0.0f; // smoothed bend for held voices (note-ons use raw = instant)
    int transposeOct = 0;

    int currentPreset = 0;
    int lastVoiceMode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Ersa8Processor)
};
