#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "ErsaLookAndFeel.h"
#include "PresetBrowser.h"

class Ersa8Processor;

// Fixed-layout content (1240x586), hosted inside a scaling resizable editor.
class ErsaContent : public juce::Component
{
public:
    explicit ErsaContent(Ersa8Processor&);
    ~ErsaContent() override;
    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 1240, baseH = 586;

private:
    Ersa8Processor& proc;
    ErsaLookAndFeel lnf;

    juce::TextButton presetBtn { "Init Patch" };
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, initBtn { "INIT" }, panicBtn { "PANIC" };
    std::unique_ptr<PresetBrowser> browser;

    std::vector<std::unique_ptr<juce::Slider>> knobs;
    std::vector<std::unique_ptr<juce::ComboBox>> boxes;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<juce::GroupComponent>> groups;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sAtt;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cAtt;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> bAtt;

    juce::Slider* addKnob(const juce::String& param, const juce::String& label,
                          juce::Component* parent, int x, int y, int w = 64, int h = 62);
    juce::ComboBox* addChoice(const juce::String& param, juce::Component* parent, int x, int y, int w = 110, int h = 22);
    juce::ToggleButton* addToggle(const juce::String& param, const juce::String& label,
                                  juce::Component* parent, int x, int y, int w = 64, int h = 34);
    juce::GroupComponent* addGroup(const juce::String& title, juce::Component* parent, int x, int y, int w, int h,
                                   juce::Colour accent = ErsaColors::orange);
    juce::Label* addCaption(const juce::String& text, juce::Component* parent, int x, int y, int w);
    void refreshPresetName();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ErsaContent)
};

class Ersa8Editor : public juce::AudioProcessorEditor
{
public:
    explicit Ersa8Editor(Ersa8Processor&);
    ~Ersa8Editor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ErsaContent content;
    juce::ResizableCornerComponent corner;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Ersa8Editor)
};
