#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// ============================================================
// ERSA-8 : parameter IDs (dual-VCO analog signal flow + extended FX)
// ============================================================
namespace P
{
    // VCO-1
    static constexpr const char* VCO1_WAVE  = "vco1wave";   // 0 tri 1 saw 2 pulse 3 square
    static constexpr const char* VCO1_RANGE = "vco1range";  // octave -2..+2 (0 = concert pitch)
    static constexpr const char* VCO1_LEVEL = "vco1level";
    static constexpr const char* VCO1_PW    = "vco1pw";
    static constexpr const char* VCO1_PWM   = "vco1pwm";    // LFO->PW amount
    // VCO-2
    static constexpr const char* VCO2_WAVE  = "vco2wave";   // 0 saw 1 pulse 2 square 3 sine 4 noise
    static constexpr const char* VCO2_RANGE = "vco2range";
    static constexpr const char* VCO2_LEVEL = "vco2level";
    static constexpr const char* VCO2_PW    = "vco2pw";
    static constexpr const char* VCO2_PWM   = "vco2pwm";
    static constexpr const char* VCO2_TUNE  = "vco2tune";   // -100..+100 cents (coarse via range)
    static constexpr const char* U1         = "u1";          // VCO-1 unison taps 1..4
    static constexpr const char* U2         = "u2";          // VCO-2 unison taps 1..4
    static constexpr const char* MIXC       = "oscmix";      // VCO1<->VCO2 balance 0..1
    static constexpr const char* SYNC       = "sync";
    static constexpr const char* XMOD       = "xmod";       // VCO2 -> VCO1 FM amount
    // Mixer / HPF
    static constexpr const char* HPF        = "hpf";        // 0 off 1..3
    // VCF
    static constexpr const char* CUTOFF     = "cutoff";
    static constexpr const char* RESO       = "reso";
    static constexpr const char* FENV       = "fenv";       // -1..+1
    static constexpr const char* FLFO       = "flfo";
    static constexpr const char* KEYTRK     = "keytrk";
    static constexpr const char* SLOPE       = "slope";      // 0:12dB 1:24dB
    // Envelopes
    static constexpr const char* FA = "fa"; static constexpr const char* FD = "fd";
    static constexpr const char* FS = "fs"; static constexpr const char* FR = "fr";
    static constexpr const char* AA = "aa"; static constexpr const char* AD = "ad";
    static constexpr const char* AS = "as"; static constexpr const char* AR = "ar";
    // LFO
    static constexpr const char* LFO_RATE  = "lforate";
    static constexpr const char* LFO_WAVE  = "lfowave";  // 0 tri 1 sine 2 saw 3 square 4 snh
    static constexpr const char* LFO_DELAY = "lfodelay";
    static constexpr const char* LFO_VCO1  = "lfovco1";
    static constexpr const char* LFO_VCO2  = "lfovco2";
    static constexpr const char* LFO_VCF   = "lfovcf";
    // VCA / master
    static constexpr const char* VCA_MODE   = "vcamode";   // 0 gate 1 env
    static constexpr const char* VOLUME     = "volume";
    static constexpr const char* TUNE       = "tune";      // master -100..+100c
    static constexpr const char* BEND       = "bend";      // 0,2,12 semitones choice
    static constexpr const char* PORTA      = "porta";
    static constexpr const char* VOICEMODE  = "voicemode"; // 0 poly 1 mono 2 legato
    static constexpr const char* UIDET      = "uidet";
    static constexpr const char* VELSENS    = "velsens";
    // Arp
    static constexpr const char* ARP_ON    = "arpon";
    static constexpr const char* ARP_MODE  = "arpmode";  // 0 up 1 down 2 updown 3 random
    static constexpr const char* ARP_DIV   = "arpdiv";   // host-synced division 0..7
    static constexpr const char* ARP_OCT   = "arpoct";   // 1..4
    static constexpr const char* ARP_HOLD  = "arphold";
    // FX — chorus
    static constexpr const char* CH_ON    = "chon";
    static constexpr const char* CH_MODE  = "chmode";   // 0 manual 1:vintage I 2:vintage II 3:extended
    static constexpr const char* CH_RATE  = "chrate";
    static constexpr const char* CH_DEPTH = "chdepth";
    static constexpr const char* CH_MIX   = "chmix";
    // FX — phaser
    static constexpr const char* PH_ON   = "phon";
    static constexpr const char* PH_RATE = "phrate";
    static constexpr const char* PH_DEPTH= "phdepth";
    static constexpr const char* PH_FB   = "phfb";
    static constexpr const char* PH_MIX  = "phmix";
    // FX — delay
    static constexpr const char* DL_ON   = "dlon";
    static constexpr const char* DL_TIME = "dltime";  // ms
    static constexpr const char* DL_FB   = "dlfb";
    static constexpr const char* DL_MIX  = "dlmix";
    static constexpr const char* DL_SYNC = "dlsync";   // 0 free 1 sync (1/8d etc via choice->time computed? keep simple: sync divides)
    // FX — saturator (master glue, post-reverb)
    static constexpr const char* ST_ON   = "ston";
    static constexpr const char* ST_AMT  = "stamt";
    static constexpr const char* ST_MODE = "stmode";   // 0 tape 1 tube
    // FX — reverb
    static constexpr const char* RV_ON   = "rvon";
    static constexpr const char* RV_SIZE = "rvsize";
    static constexpr const char* RV_DAMP = "rvdamp";
    static constexpr const char* RV_MIX  = "rvmix";
    // FX — drive / EQ
    static constexpr const char* DRIVE  = "drive";
    static constexpr const char* EQLOW   = "eqlow";
    static constexpr const char* EQHIGH  = "eqhigh";
    static constexpr const char* FXBYPASS= "fxbypass";
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
