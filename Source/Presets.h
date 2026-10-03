#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <map>

// A factory preset = name + data file. Presets live as data (XML files), never
// as compiled code: shipped files load from the install, per-user overwrites
// in Documents shadow them. Filenames: "NNN Name.xml".
struct FactoryPreset
{
    juce::String name;
    juce::File file;
    int index = -1;
    bool shadowed = false; // user-overwritten copy shadows the shipped file
    int bank = 0;          // effective bank 1..8 (tag or index fallback)
};

const std::vector<FactoryPreset>& getFactoryPresets(); // sorted by index
void rescanFactoryPresets();
void setFactoryDirsForTest(const juce::File& bundleDir, const juce::File& overrideDir);
void applyPreset(class juce::AudioProcessorValueTreeState& apvts, int index);
juce::String presetName(int index);
bool overwriteFactoryPreset(int index, class juce::AudioProcessorValueTreeState& apvts);
bool deleteFactoryShadow(int index); // restores the shipped file (true if one existed)

// ---- user presets (XML files on disk) ----
struct UserPreset { juce::String name; juce::File file; };
void setUserPresetDirForTest(const juce::File& dir); // harness override, else Documents
juce::File getUserPresetDir();
std::vector<UserPreset> scanUserPresets();
juce::String sanitisePresetName(juce::String name);
bool saveUserPreset(const juce::String& name, class juce::AudioProcessorValueTreeState& apvts);
bool loadUserPreset(const juce::File& file, class juce::AudioProcessorValueTreeState& apvts);
bool deleteUserPreset(const juce::File& file);
