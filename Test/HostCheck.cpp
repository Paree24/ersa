// VST3 host check (dev only, never shipped): loads the BUILT .vst3 through
// JUCE's VST3 hosting, like a DAW would: buses, params, programs, audio, state.
//   HostCheck <path/to/ERSA.vst3>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <cstdio>

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { printf("usage: HostCheck <ERSA.vst3>\n"); return 1; }
    int fails = 0;
    auto check = [&](bool ok, const juce::String& what)
    { printf("%s  %s\n", ok ? "  ok " : "FAIL ", what.toRawUTF8()); if (!ok) ++fails; };
    juce::VST3PluginFormat fmt;
    juce::OwnedArray<juce::PluginDescription> descs;
    fmt.findAllTypesForFile(descs, juce::File(argv[1]).getFullPathName());
    check(descs.size() == 1, "VST3 scanned: " + juce::String(descs.size()) + " plugin(s)");
    if (descs.isEmpty()) return 1;
    auto& d = *descs[0];
    check(d.isInstrument, "registered as instrument: " + d.name + " / " + d.category);
    juce::String err;
    auto inst = fmt.createInstanceFromDescription(d, 48000.0, 512, err);
    check(inst != nullptr, "instantiated " + err);
    if (!inst) return 1;
    check(inst->getBusCount(false) == 1 && inst->getTotalNumOutputChannels() == 2,
          "one stereo output bus (" + juce::String(inst->getTotalNumOutputChannels()) + " ch)");
    check(inst->getParameters().size() >= 70, "parameters: " + juce::String(inst->getParameters().size()));
    check(inst->getNumPrograms() == 256, "factory programs: " + juce::String(inst->getNumPrograms()));
    inst->setCurrentProgram(16);
    check(inst->getCurrentProgram() == 16, "program change: " + inst->getProgramName(16));
    inst->prepareToPlay(48000.0, 512);
    juce::AudioBuffer<float> buf(2, 512);
    float peak = 0.0f;
    bool finite = true;
    for (int b = 0; b < 60; ++b)
    {
        juce::MidiBuffer mb;
        if (b == 1) mb.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)110), 17);
        if (b == 40) mb.addEvent(juce::MidiMessage::noteOff(1, 60), 3);
        buf.clear();
        inst->processBlock(buf, mb);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i) finite = finite && std::isfinite(buf.getSample(ch, i));
        peak = juce::jmax(peak, buf.getMagnitude(0, 0, 512), buf.getMagnitude(1, 0, 512));
    }
    check(finite, "audio finite");
    check(peak > 0.02f && peak <= 1.0f, "note 60 plays (peak " + juce::String(peak, 3) + ")");
    juce::MemoryBlock st;
    inst->getStateInformation(st);
    check(st.getSize() > 500, "state saved (" + juce::String((int)st.getSize()) + " bytes)");
    inst->setCurrentProgram(0);
    inst->setStateInformation(st.getData(), (int)st.getSize());
    check(inst->getCurrentProgram() == 16, "state restored (program " + juce::String(inst->getCurrentProgram()) + ")");
    std::unique_ptr<juce::AudioProcessorEditor> ed(inst->createEditorIfNeeded());
    check(ed != nullptr && ed->getWidth() == 1240, "editor opens at 1240 px wide");
    ed.reset();
    inst->releaseResources();
    inst.reset();
    printf(fails == 0 ? "HOST CHECK PASSED\n" : "%d HOST CHECK(S) FAILED\n", fails);
    return fails == 0 ? 0 : 1;
}
