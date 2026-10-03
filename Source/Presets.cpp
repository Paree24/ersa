#include "Presets.h"
#include "Parameters.h"
#include <juce_audio_processors/juce_audio_processors.h>


// ---------------- factory presets: data files, never code ----------------
// Shipped XMLs load from the install bundle (Resources/Factory) with dev
// fallbacks; per-user overwrites in <docs>/ERSA Presets/Factory shadow them.
// Filenames: "NNN Name.xml".
static juce::File customFactoryDir;
static bool useCustomFactoryDir = false;
static juce::File customFactoryOverrideDir;
static bool useCustomFactoryOverrideDir = false;

void setFactoryDirsForTest(const juce::File& bundleDir, const juce::File& overrideDir)
{
    customFactoryDir = bundleDir; useCustomFactoryDir = true;
    customFactoryOverrideDir = overrideDir; useCustomFactoryOverrideDir = true;
}

static juce::File factoryBundleDir()
{
    if (useCustomFactoryDir) return customFactoryDir;
    // VST3 bundle layout: <bundle>/Contents/<arch>/ERSA.so -> ../Resources/Factory
    auto exe = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    auto res = exe.getParentDirectory().getParentDirectory().getChildFile("Resources/Factory");
    if (res.isDirectory()) return res;
    // dev fallbacks (running from repo or build tree without installed bundle)
    const char* cands[] = { "./Source/Factory", "../Source/Factory", "../../Source/Factory" };
    for (auto c : cands)
    {
        juce::File d(c);
        if (d.isDirectory()) return d;
    }
    return juce::File();
}

static juce::File factoryOverrideDir()
{
    if (useCustomFactoryOverrideDir) return customFactoryOverrideDir;
    return getUserPresetDir().getChildFile("Factory");
}

static int bankIndexFor(const juce::String& tag)
{
    static const char* names[] = { "Bass", "Lead", "Pad", "Orch", "Keys", "Arp", "FX", "Classics" };
    for (int i = 0; i < 8; ++i)
        if (tag.equalsIgnoreCase(names[i]) || (i == 7 && tag.equalsIgnoreCase("Clsc")))
            return i + 1;
    return 0;
}

// "NNN Name [Bank].xml" — bank tag optional; untagged files fall back to
// index ranges (bank 1 layout). Returns bank 1..8.
static bool parseFactoryFileName(const juce::String& fn, int& indexOut, juce::String& nameOut, int& bankOut)
{
    if (!fn.endsWithIgnoreCase(".xml") || fn.length() < 8) return false;
    if (!juce::CharacterFunctions::isDigit(fn[0]) || !juce::CharacterFunctions::isDigit(fn[1])
        || !juce::CharacterFunctions::isDigit(fn[2]) || fn[3] != ' ') return false;
    indexOut = fn.substring(0, 3).getIntValue();
    juce::String rest = fn.substring(4, fn.length() - 4);
    bankOut = 0;
    int ob = rest.lastIndexOfChar('[');
    if (ob > 0 && rest.endsWithChar(']'))
    {
        juce::String tag = rest.substring(ob + 1, rest.length() - 1).trim();
        int b = bankIndexFor(tag);
        if (b > 0) { bankOut = b; rest = rest.substring(0, ob).trim(); }
    }
    nameOut = rest;
    if (bankOut == 0)
        bankOut = 1 + juce::jlimit(0, 7, indexOut / 16);
    return indexOut >= 0 && indexOut < 1000 && nameOut.isNotEmpty();
}

static std::vector<FactoryPreset> factory;
static bool factoryScanned = false;

void rescanFactoryPresets()
{
    factory.clear();
    std::map<int, FactoryPreset> byIndex;
    auto collect = [&](const juce::File& dir, bool shadowed)
    {
        if (!dir.isDirectory()) return;
        for (auto& f : dir.findChildFiles(juce::File::findFiles, false, "*.xml"))
        {
            int idx, bank; juce::String nm;
            if (!parseFactoryFileName(f.getFileName(), idx, nm, bank)) continue;
            byIndex[idx] = { nm, f, idx, shadowed, bank };
        }
    };
    collect(factoryBundleDir(), false);
    collect(factoryOverrideDir(), true); // user overwrites shadow shipped files
    for (auto& kv : byIndex) factory.push_back(kv.second);
    factoryScanned = true;
}

const std::vector<FactoryPreset>& getFactoryPresets()
{
    if (!factoryScanned) rescanFactoryPresets();
    return factory;
}

static bool applyStateFile(const juce::File& file, juce::AudioProcessorValueTreeState& apvts)
{
    auto xml = juce::parseXML(file);
    if (xml == nullptr) return false;
    auto vt = juce::ValueTree::fromXml(*xml);
    if (!vt.isValid()) return false;
    // reset to defaults, then apply every stored param (unknown ids ignored)
    for (auto* param : apvts.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
            ranged->setValueNotifyingHost(ranged->getDefaultValue());
    int applied = 0;
    for (int i = 0; i < vt.getNumChildren(); ++i)
    {
        auto child = vt.getChild(i);
        if (!child.hasType("PARAM")) continue;
        juce::String id = child.getProperty("id").toString();
        if (auto* par = apvts.getParameter(id))
        {
            // JUCE 8 stores denormalised real units in the tree: convert back
            float denorm = (float)child.getProperty("value", 0.0);
            par->setValueNotifyingHost(par->convertTo0to1(denorm));
            ++applied;
        }
    }
    return applied > 0;
}

void applyPreset(juce::AudioProcessorValueTreeState& apvts, int index)
{
    const auto& all = getFactoryPresets();
    for (auto& pr : all)
        if (pr.index == index) { applyStateFile(pr.file, apvts); return; }
}

juce::String presetName(int index)
{
    const auto& all = getFactoryPresets();
    for (auto& pr : all)
        if (pr.index == index) return pr.name;
    return "Init";
}

bool overwriteFactoryPreset(int index, juce::AudioProcessorValueTreeState& apvts)
{
    const auto& all = getFactoryPresets();
    const FactoryPreset* row = nullptr;
    for (auto& pr : all)
        if (pr.index == index) row = &pr;
    if (row == nullptr) return false;
    auto dir = factoryOverrideDir();
    if (!dir.isDirectory() && !dir.createDirectory()) return false;
    auto file = dir.getChildFile(row->file.getFileName()); // same NNN Name.xml
    if (file.exists() && !file.deleteFile()) return false;
    bool ok = false;
    if (auto xml = apvts.copyState().createXml())
        ok = xml->writeTo(file);
    if (ok) rescanFactoryPresets();
    return ok;
}

bool deleteFactoryShadow(int index)
{
    const auto& all = getFactoryPresets();
    for (auto& pr : all)
    {
        if (pr.index == index && pr.shadowed)
        {
            bool ok = pr.file.deleteFile();
            rescanFactoryPresets();
            return ok;
        }
    }
    return false;
}

bool loadUserPreset(const juce::File& file, juce::AudioProcessorValueTreeState& apvts)
{
    return applyStateFile(file, apvts);
}

// ---------------- user presets ----------------
static juce::File customUserDir;
static bool useCustomUserDir = false;

void setUserPresetDirForTest(const juce::File& dir) { customUserDir = dir; useCustomUserDir = true; }

juce::File getUserPresetDir()
{
    if (useCustomUserDir) return customUserDir;
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("ERSA Presets");
}

std::vector<UserPreset> scanUserPresets()
{
    std::vector<UserPreset> out;
    auto dir = getUserPresetDir();
    if (!dir.isDirectory()) return out;
    for (auto& f : dir.findChildFiles(juce::File::findFiles, false, "*.xml"))
        out.push_back({ f.getFileNameWithoutExtension(), f });
    std::sort(out.begin(), out.end(),
              [](const UserPreset& a, const UserPreset& b)
              { return a.name.compareIgnoreCase(b.name) < 0; });
    return out;
}

juce::String sanitisePresetName(juce::String name)
{
    name = name.trim().substring(0, 64);
    name = name.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-");
    name = name.trim();
    return name.isEmpty() ? juce::String("Untitled") : name;
}

bool saveUserPreset(const juce::String& name, juce::AudioProcessorValueTreeState& apvts)
{
    auto clean = sanitisePresetName(name);
    auto dir = getUserPresetDir();
    if (!dir.isDirectory() && !dir.createDirectory()) return false;
    auto file = dir.getChildFile(clean + ".xml");
    if (file.exists() && !file.deleteFile()) return false;
    if (auto xml = apvts.copyState().createXml())
        return xml->writeTo(file);
    return false;
}


bool deleteUserPreset(const juce::File& file)
{
    if (file.getParentDirectory() != getUserPresetDir()) return false; // safety: own dir only
    return file.existsAsFile() && file.deleteFile();
}
