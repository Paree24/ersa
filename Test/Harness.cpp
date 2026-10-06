// Headless smoke test: instantiates the real processor + editor,
// walks all 128 presets, renders audio with MIDI, round-trips state.
// Exit code 0 = all OK, crash/assert = bug reproduced.
#include <cstdio>
#include <cmath>
#include "PluginProcessor.h"
#include "PluginEditor.h"\n#include "PresetBrowser.h"

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); return 1; } \
    else { printf("ok: %s\n", msg); } } while (0)

static bool isFiniteBuffer(const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (!std::isfinite(b.getSample(ch, i))) return false;
    return true;
}

static float peakAbs(const juce::AudioBuffer<float>& b)
{
    float p = 0.0f;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            p = juce::jmax(p, std::abs(b.getSample(ch, i)));
    return p;
}

int main(int argc, char** argv)
{
    juce::ignoreUnused(argc, argv);
    {
        juce::File repoFactory;
        const char* cands[] = { "Source/Factory", "../Source/Factory",
                                "../../Source/Factory", "ersa/Source/Factory" };
        for (auto c : cands)
            if (juce::File(c).isDirectory()) { repoFactory = juce::File(c); break; }
        if (!repoFactory.isDirectory())
        {
            printf("FAIL: repo factory presets not found (run from repo root or build dir)\n");
            return 1;
        }
        juce::File tmpOver = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                 .getChildFile("ersa-harness-factory-override");
        tmpOver.deleteRecursively();
        setFactoryDirsForTest(repoFactory, tmpOver);
        printf("factory dir: %s\n", repoFactory.getFullPathName().toRawUTF8());
    }
    printf("--- fresh processor, fx off from boot ---\n");
    {
        std::unique_ptr<Ersa8Processor> p2(new Ersa8Processor());
        printf("getSampleRate before prepare = %f\n", p2->getSampleRate());
        p2->prepareToPlay(44100.0, 512);
        printf("getSampleRate after prepare = %f\n", p2->getSampleRate());
        if (auto* fx = p2->apvts.getParameter("fxbypass"))
            fx->setValueNotifyingHost(0.0f);
        for (auto id : { "chon", "phon", "dlon", "rvon", "ston" })
            if (auto* prm = p2->apvts.getParameter(id))
                prm->setValueNotifyingHost(0.0f);
        juce::AudioBuffer<float> b2(2, 512);
        juce::MidiBuffer m2;
        p2->processBlock(b2, m2);
        printf("fresh no-fx finite=%d s0=%f\n", (int)isFiniteBuffer(b2), b2.getSample(0, 0));
        p2->releaseResources();
    }
    printf("--- preset walk poisoning bisect ---\n");
    {
        std::unique_ptr<Ersa8Processor> p3(new Ersa8Processor());
        p3->prepareToPlay(44100.0, 512);
        if (auto* fx = p3->apvts.getParameter("fxbypass"))
            fx->setValueNotifyingHost(0.0f);
        for (auto id : { "chon", "phon", "dlon", "rvon", "ston" })
            if (auto* prm = p3->apvts.getParameter(id))
                prm->setValueNotifyingHost(0.0f);
        juce::AudioBuffer<float> b3(2, 512);
        juce::MidiBuffer m3;
        int nprog = p3->getNumPrograms();
        for (int i = 0; i < nprog; ++i)
        {
            p3->setCurrentProgram(i);
            b3.clear();
            p3->processBlock(b3, m3);
            if (!isFiniteBuffer(b3))
            {
                printf("POISON at preset %d (%s)\n", i, p3->getPresetName(i).toRawUTF8());
                break;
            }
            if (i == nprog - 1) printf("walk clean: no poisoning preset found\n");
        }
        p3->releaseResources();
    }
    printf("--- creating processor ---\n");
    printf("--- creating processor ---\n");
    std::unique_ptr<Ersa8Processor> proc(new Ersa8Processor());
    CHECK(proc->getNumPrograms() == 256, "256 programs (2 banks)");
    CHECK(std::abs(proc->apvts.getRawParameterValue("cutoff")->load() - 600.0f) < 0.01f, "boots to preset 000");

    printf("--- walking presets ---\n");
    for (int i = 0; i < proc->getNumPrograms(); ++i)
    {
        proc->setCurrentProgram(i);
        juce::String n = proc->getProgramName(i);
        if (n.isEmpty()) { printf("FAIL: preset %d has empty name\n", i); return 1; }
        // every program must leave explicit octave ranges (C3 centering)
        auto* r1 = proc->apvts.getRawParameterValue("vco1range");
        auto* r2 = proc->apvts.getRawParameterValue("vco2range");
        if (r1 == nullptr || r2 == nullptr) { printf("FAIL: range param missing\n"); return 1; }
    }
    printf("ok: all %d presets apply + named\n", proc->getNumPrograms());

    printf("--- create/delete editor ---\n");
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed(proc->createEditor());
        CHECK(ed != nullptr, "editor created");
        ed->setSize(ErsaContent::baseW, ErsaContent::baseH);
        printf("children=%d\n", ed->getNumChildComponents());
        for (int ci = 0; ci < juce::jmin(3, ed->getNumChildComponents()); ++ci)
            if (auto* c = ed->getChildComponent(ci))
                printf("child %d vis=%d %s\n", ci, (int)c->isVisible(), c->getBounds().toString().toRawUTF8());
        juce::Image img(juce::Image::RGB, 1240, 586, true);
        {
            juce::Graphics g(img);
            ed->paintEntireComponent(g, false); // full tree incl. children
        }
        printf("ok: editor paint ran\n");
        {
            // render the preset browser open as well
            ErsaLookAndFeel blnf;
            PresetBrowser pb(*proc, blnf);
            pb.setBounds(0, 0, 1240, 586);
            pb.setVisible(true);
            juce::Image bimg(juce::Image::RGB, 1240, 586, true);
            {
                juce::Graphics bg2(bimg);
                pb.paintEntireComponent(bg2, false);
            }
            juce::File bpng("/tmp/ersa-browser.png");
            bpng.deleteFile();
            {
                juce::FileOutputStream bfos(bpng);
                juce::PNGImageFormat bfmt;
                bfmt.writeImageToStream(bimg, bfos);
            }
            printf("browser screenshot: %s\n", bpng.getFullPathName().toRawUTF8());
            juce::File png("/tmp/ersa-ui.png");
            png.deleteFile();
            juce::FileOutputStream fos(png);
            juce::PNGImageFormat fmt;
            fmt.writeImageToStream(img, fos);
            printf("ui screenshot: %s\n", png.getFullPathName().toRawUTF8());
        }
    }
    printf("ok: editor deleted\n");

    printf("--- prepare + render ---\n");
    const double sr = 44100.0;
    const int block = 512;
    proc->setRateAndBufferSizeDetails((int)sr, block); // what the plugin wrapper does
    proc->prepareToPlay(sr, block);
    proc->setCurrentProgram(0);
    CHECK(proc->getSampleRate() == sr, "sample rate visible to processor");

    // determinism: preset 27 sets EQ high; preset 0 must reset it
    proc->setCurrentProgram(27);
    CHECK(std::abs(proc->apvts.getRawParameterValue("eqhigh")->load() - 4.0f) < 1e-3f, "preset 27 eqhigh set");
    proc->setCurrentProgram(0);
    printf("eqhigh after preset 0 = %f\n", proc->apvts.getRawParameterValue("eqhigh")->load());
    CHECK(std::abs(proc->apvts.getRawParameterValue("eqhigh")->load()) < 1e-3f, "preset 0 resets eqhigh");

    juce::AudioBuffer<float> buf(2, block);
    juce::MidiBuffer midi;

    // bisection: dry (FX bypassed) first
    if (auto* fx = proc->apvts.getParameter("fxbypass"))
        fx->setValueNotifyingHost(1.0f);
    buf.clear();
    proc->processBlock(buf, midi);
    printf("dry block0 finite=%d peak=%f s0=%f s1=%f\n",
           (int)isFiniteBuffer(buf), peakAbs(buf), buf.getSample(0, 0), buf.getSample(0, 1));
    if (auto* fx = proc->apvts.getParameter("fxbypass"))
        fx->setValueNotifyingHost(0.0f);
    // isolate: all FX off, bypass off
    for (auto id : { "chon", "phon", "dlon", "rvon", "ston" })
        if (auto* prm = proc->apvts.getParameter(id))
            prm->setValueNotifyingHost(0.0f);
    buf.clear();
    proc->processBlock(buf, midi);
    printf("no-fx block finite=%d peak=%f s0=%f\n",
           (int)isFiniteBuffer(buf), peakAbs(buf), buf.getSample(0, 0));
    // enable one at a time
    for (auto id : { "chon", "phon", "dlon", "rvon", "ston" })
    {
        if (auto* prm = proc->apvts.getParameter(id))
            prm->setValueNotifyingHost(1.0f);
        buf.clear();
        proc->processBlock(buf, midi);
        printf("only-%s finite=%d peak=%f s0=%f\n",
               id, (int)isFiniteBuffer(buf), peakAbs(buf), buf.getSample(0, 0));
        if (auto* prm = proc->apvts.getParameter(id))
            prm->setValueNotifyingHost(0.0f);
    }
    float maxPeak = 0.0f;
    const int totalBlocks = (int)(sr * 2.5 / block);
    for (int b = 0; b < totalBlocks; ++b)
    {
        midi.clear();
        if (b == 2)  midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9f), 0);
        if (b == 20) midi.addEvent(juce::MidiMessage::noteOn(1, 64, 0.9f), 0);
        if (b == 40) midi.addEvent(juce::MidiMessage::noteOn(1, 67, 0.9f), 0);
        if (b == 80) midi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
        buf.clear();
        proc->processBlock(buf, midi);
        if (!isFiniteBuffer(buf)) { printf("FAIL: non-finite audio at block %d\n", b); return 1; }
        maxPeak = juce::jmax(maxPeak, peakAbs(buf));
    }
    printf("ok: 2.5s render finite, peak=%.3f\n", maxPeak);
    CHECK(maxPeak > 0.001f, "synth actually produces sound");

    printf("--- worst case: 4x4 per-osc unison + all FX + arp ---\n");
    for (auto id : { "u1", "u2" })
        if (auto* up = proc->apvts.getParameter(id))
            up->setValueNotifyingHost(up->convertTo0to1(3.0f)); // 4 taps each
    for (auto id : { "chon", "phon", "dlon", "rvon", "ston" })
        if (auto* prm = proc->apvts.getParameter(id))
            prm->setValueNotifyingHost(1.0f);
    if (auto* ar = proc->apvts.getParameter("arpon"))
        ar->setValueNotifyingHost(1.0f);
    for (int b = 0; b < 60; ++b)
    {
        midi.clear();
        if (b == 1) { midi.addEvent(juce::MidiMessage::noteOn(1, 48, 1.0f), 0);
                      midi.addEvent(juce::MidiMessage::noteOn(1, 55, 1.0f), 0); }
        if (b == 50) midi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
        buf.clear();
        proc->processBlock(buf, midi);
        if (!isFiniteBuffer(buf)) { printf("FAIL: non-finite audio (fx worst case) at block %d\n", b); return 1; }
    }
    printf("ok: fx worst case finite\n");

    printf("--- analog slop: same note twice must differ slightly ---\n");
    {
        std::unique_ptr<Ersa8Processor> p4(new Ersa8Processor());
        p4->setRateAndBufferSizeDetails(44100, 512);
        p4->prepareToPlay(44100.0, 512);
        p4->setCurrentProgram(16); // saw lead
        juce::AudioBuffer<float> ba(2, 512), bb(2, 512);
        juce::MidiBuffer mm;
        mm.addEvent(juce::MidiMessage::noteOn(1, 48, 0.9f), 0);
        p4->processBlock(ba, mm);
        juce::AudioBuffer<float> first;
        first.makeCopyOf(ba); // preserve the true first attack
        for (int r = 0; r < 90; ++r) // let release fully finish (~1 s)
        {
            mm.clear();
            if (r == 0) mm.addEvent(juce::MidiMessage::noteOff(1, 48), 0);
            p4->processBlock(ba, mm);
        }
        mm.clear();
        mm.addEvent(juce::MidiMessage::noteOn(1, 48, 0.9f), 0);
        p4->processBlock(bb, mm);
        float diff = 0.0f;
        for (int i = 0; i < 512; ++i)
            diff = juce::jmax(diff, std::abs(first.getSample(0, i) - bb.getSample(0, i)));
        printf("two strikes max diff = %f\n", diff);
        CHECK(diff > 0.0001f, "no two notes sound exactly identical (slop alive)");
        p4->releaseResources();
    }
    printf("--- tuning probe: measured fundamental vs expected ---\n");
    {
        struct TP { int preset; int note; float expect; const char* why; };
        TP tests[] = {
            { 0, 48, 65.41f, "Sub 16'" }, { 5, 48, 65.41f, "Acid 16'" },
            { 16, 48, 130.81f, "Lead 8'" }, { 64, 48, 130.81f, "Keys 8'" },
            { 49, 48, 65.41f, "Cello 16'" }, { 50, 48, 261.63f, "Violin 4'" },
        };
        for (auto& t : tests)
        {
            std::unique_ptr<Ersa8Processor> tp(new Ersa8Processor());
            tp->setRateAndBufferSizeDetails(44100, 512);
            tp->prepareToPlay(44100.0, 512);
            tp->loadInit();
            if (auto* ch = tp->apvts.getParameter("chon")) ch->setValueNotifyingHost(0.0f);
            tp->setCurrentProgram(t.preset);
            juce::AudioBuffer<float> tb(2, 512);
            juce::MidiBuffer tm;
            std::vector<float> steady;
            steady.reserve(44100);
            for (int k = 0; k < 86; ++k) // ~1 s
            {
                tm.clear();
                if (k == 0) tm.addEvent(juce::MidiMessage::noteOn(1, t.note, 0.9f), 0);
                tb.clear(); tp->processBlock(tb, tm);
                if (!isFiniteBuffer(tb)) { printf("FAIL: non-finite tuning probe\n"); return 1; }
                if (k >= 30 && k < 80)
                    for (int i = 0; i < 512; ++i) steady.push_back(tb.getSample(0, i) + tb.getSample(1, i));
            }
            const int N = 8192;
            juce::dsp::FFT fft(13);
            std::vector<float> fbuf(2 * N, 0.0f);
            for (int i = 0; i < N && i < (int)steady.size(); ++i)
            {
                float w = 0.5f - 0.5f * std::cos(2.0f * 3.14159265f * (float)i / (float)N);
                fbuf[i] = steady[(size_t)i] * w; // first half = consecutive samples
            }
            fft.performFrequencyOnlyForwardTransform(fbuf.data(), true);
            // search only +/-3% around the EXPECTED fundamental (immune to sub dominance)
            float expBin = t.expect * (float)N / 44100.0f;
            int lo = juce::jmax(2, (int)(expBin * 0.97f)), hi = (int)(expBin * 1.03f) + 1;
            int peakBin = lo; float peakVal = 0.0f;
            for (int b = lo; b <= hi && b < N / 2; ++b)
                if (fbuf[b] > peakVal) { peakVal = fbuf[b]; peakBin = b; }
            float adj = 0.0f;
            if (peakBin > 1 && peakBin < N / 2 - 1)
            {
                float a = fbuf[peakBin - 1], b = fbuf[peakBin], c = fbuf[peakBin + 1];
                float dd = a - 2 * b + c;
                if (std::abs(dd) > 1e-9f) adj = 0.5f * (a - c) / dd;
            }
            float meas = ((float)peakBin + adj) * 44100.0f / (float)N;
            float bestErr = std::abs(1200.0f * std::log2(meas / t.expect));
            printf("preset %d (%s): %.1f Hz vs %.1f, err %.0f cents %s\n",
                   t.preset, t.why, meas, t.expect, bestErr, bestErr < 20.0f ? "OK" : "*** OFF ***");
            tp->releaseResources();
        }
    }
    printf("--- isolated osc tuning (dry, single osc) ---\n");
    {
        struct OT { const char* name; float vco1l; float vco2l; float expect; };
        OT tests[] = {
            { "VCO1 only", 0.8f, 0.0f, 440.0f }, { "VCO2 only", 0.0f, 0.8f, 440.0f },
            { "both raw", 0.8f, 0.8f, 440.0f },
        };
        for (auto& t : tests)
        {
            std::unique_ptr<Ersa8Processor> tp(new Ersa8Processor());
            tp->setRateAndBufferSizeDetails(44100, 512);
            tp->prepareToPlay(44100.0, 512);
            tp->loadInit();
            if (auto* ch = tp->apvts.getParameter("chon")) ch->setValueNotifyingHost(0.0f);
            auto setF = [&](const char* id, float v){
                if (auto* prm = tp->apvts.getParameter(id))
                    prm->setValueNotifyingHost(prm->convertTo0to1(v));
            };
            setF("vco1level", t.vco1l); setF("vco2level", t.vco2l);
            setF("fxbypass", 1.0f);
            juce::AudioBuffer<float> tb(2, 512);
            juce::MidiBuffer tm;
            std::vector<float> steady;
            for (int k = 0; k < 86; ++k)
            {
                tm.clear();
                if (k == 0) tm.addEvent(juce::MidiMessage::noteOn(1, 69, 0.9f), 0);
                tb.clear(); tp->processBlock(tb, tm);
                if (k >= 30 && k < 80)
                    for (int i = 0; i < 512; ++i) steady.push_back(tb.getSample(0, i));
            }
            const int N = 16384;
            juce::dsp::FFT fft(14);
            std::vector<float> fbuf(2 * N, 0.0f);
            for (int i = 0; i < N && i < (int)steady.size(); ++i)
            {
                float w = 0.5f - 0.5f * std::cos(2.0f * 3.14159265f * (float)i / (float)N);
                fbuf[i] = steady[(size_t)i] * w;
            }
            fft.performFrequencyOnlyForwardTransform(fbuf.data(), true);
            float expBin = t.expect * (float)N / 44100.0f;
            int lo = juce::jmax(2, (int)(expBin * 0.97f)), hi = (int)(expBin * 1.03f) + 1;
            int peakBin = lo; float peakVal = 0.0f;
            for (int b = lo; b <= hi && b < N / 2; ++b)
                if (fbuf[b] > peakVal) { peakVal = fbuf[b]; peakBin = b; }
            float adj = 0.0f;
            if (peakBin > 1 && peakBin < N / 2 - 1)
            {
                float a = fbuf[peakBin - 1], b = fbuf[peakBin], c = fbuf[peakBin + 1];
                float dd = a - 2 * b + c;
                if (std::abs(dd) > 1e-9f) adj = 0.5f * (a - c) / dd;
            }
            float meas = ((float)peakBin + adj) * 44100.0f / (float)N;
            float err = std::abs(1200.0f * std::log2(meas / t.expect));
            printf("%s: %.2f Hz, err %.0f cents %s\n", t.name, meas, err, err < 20.0f ? "OK" : "*** OFF ***");
            if (err >= 20.0f) { printf("FAIL: isolated osc out of tune\n"); return 1; }
            tp->releaseResources();
        }
    }
    printf("--- unison stays in octave (U1=U2=4, A4 must read ~440) ---\n");
    {
        std::unique_ptr<Ersa8Processor> pu(new Ersa8Processor());
        pu->setRateAndBufferSizeDetails(44100, 512);
        pu->prepareToPlay(44100.0, 512);
        pu->loadInit();
        if (auto* ch = pu->apvts.getParameter("chon")) ch->setValueNotifyingHost(0.0f);
        auto setC = [&](const char* id, float idx){
            if (auto* prm = pu->apvts.getParameter(id))
                prm->setValueNotifyingHost(prm->convertTo0to1(idx));
        };
        setC("u1", 3.0f); setC("u2", 3.0f);
        auto setF = [&](const char* id, float v){
            if (auto* prm = pu->apvts.getParameter(id))
                prm->setValueNotifyingHost(prm->convertTo0to1(v));
        };
        setF("vco1level", 0.8f); setF("vco2level", 0.0f);
        setF("fxbypass", 1.0f);
        juce::AudioBuffer<float> bu(2, 512);
        juce::MidiBuffer mu;
        std::vector<float> steady;
        for (int k = 0; k < 86; ++k)
        {
            mu.clear();
            if (k == 0) mu.addEvent(juce::MidiMessage::noteOn(1, 69, 0.9f), 0);
            bu.clear(); pu->processBlock(bu, mu);
            if (!isFiniteBuffer(bu)) { printf("FAIL: non-finite unison\n"); return 1; }
            if (k >= 30 && k < 80)
                for (int i = 0; i < 512; ++i) steady.push_back(bu.getSample(0, i));
        }
        const int N = 16384;
        juce::dsp::FFT fft(14);
        std::vector<float> fbuf(2 * N, 0.0f);
        for (int i = 0; i < N && i < (int)steady.size(); ++i)
        {
            float w = 0.5f - 0.5f * std::cos(2.0f * 3.14159265f * (float)i / (float)N);
            fbuf[i] = steady[(size_t)i] * w;
        }
        fft.performFrequencyOnlyForwardTransform(fbuf.data(), true);
        int peakBin = 2; float peakVal = 0.0f;
        for (int b = 2; b < N / 2; ++b)
            if (fbuf[b] > peakVal) { peakVal = fbuf[b]; peakBin = b; }
        float meas = (float)peakBin * 44100.0f / (float)N;
        printf("4x unison global peak = %.1f Hz\n", meas);
        for (float bf : { 110.0f, 220.0f, 440.0f, 880.0f })
        {
            int bb = (int)(bf * (float)N / 44100.0f);
            float be = 0.0f;
            for (int j = juce::jmax(2, bb - 2); j <= juce::jmin(N / 2 - 1, bb + 2); ++j)
                be = juce::jmax(be, fbuf[j]);
        }
        // stack must sit at concert pitch (440) or the by-design sub (220);
        // the old shared-phase bug put it at 1760 (2 octaves up)
        float e1 = std::abs(1200.0f * std::log2(meas / 440.0f));
        float e2 = std::abs(1200.0f * std::log2(meas / 220.0f));
        printf("unison err = %.0f cents\n", juce::jmin(e1, e2));
        CHECK(juce::jmin(e1, e2) < 60.0f, "unison does not shift octaves");
        pu->releaseResources();
    }
    printf("--- mono staccato: detached notes must re-articulate (Acid) ---\n");
    {
        std::unique_ptr<Ersa8Processor> p5(new Ersa8Processor());
        p5->setRateAndBufferSizeDetails(44100, 512);
        p5->prepareToPlay(44100.0, 512);
        p5->setCurrentProgram(5); // Acid Line: mono + portamento
        juce::AudioBuffer<float> b5(2, 512);
        juce::MidiBuffer m5;
        m5.addEvent(juce::MidiMessage::noteOn(1, 40, 0.9f), 0);
        p5->processBlock(b5, m5);
        float secondPeak = 0.0f;
        for (int k = 0; k < 30; ++k)
        {
            m5.clear();
            if (k == 2) m5.addEvent(juce::MidiMessage::noteOff(1, 40), 0);
            if (k == 4) m5.addEvent(juce::MidiMessage::noteOn(1, 45, 0.9f), 0);
            if (k == 20) m5.addEvent(juce::MidiMessage::noteOff(1, 45), 0);
            b5.clear();
            p5->processBlock(b5, m5);
            if (!isFiniteBuffer(b5)) { printf("FAIL: non-finite in staccato test\n"); return 1; }
            if (k >= 4 && k < 12)
                secondPeak = juce::jmax(secondPeak, peakAbs(b5));
        }
        printf("second staccato note peak = %f\n", secondPeak);
        CHECK(secondPeak > 0.05f, "staccato note re-articulates");
        p5->releaseResources();
    }
    printf("--- arp hold unlatch + 4 octaves + reverb decay ---\n");
    {
        std::unique_ptr<Ersa8Processor> p6(new Ersa8Processor());
        p6->setRateAndBufferSizeDetails(44100, 512);
        p6->prepareToPlay(44100.0, 512);
        p6->setCurrentProgram(80); // Arp Up
        auto setP = [&](const char* id, float v){
            if (auto* prm = p6->apvts.getParameter(id))
                prm->setValueNotifyingHost(v);
        };
        setP("arpon", 1.0f); setP("arphold", 1.0f);
        if (auto* o = p6->apvts.getParameter("arpoct"))
            o->setValueNotifyingHost(o->convertTo0to1(3.0f)); // 4 octaves
        setP("rvon", 1.0f);
        if (auto* rs = p6->apvts.getParameter("rvsize"))
            rs->setValueNotifyingHost(rs->convertTo0to1(0.9f));
        juce::AudioBuffer<float> b6(2, 512);
        juce::MidiBuffer m6;
        float latchedPeak = 0.0f;
        for (int k = 0; k < 40; ++k)
        {
            m6.clear();
            if (k == 0) m6.addEvent(juce::MidiMessage::noteOn(1, 48, 0.9f), 0);
            if (k == 2) m6.addEvent(juce::MidiMessage::noteOff(1, 48), 0);
            b6.clear(); p6->processBlock(b6, m6);
            if (!isFiniteBuffer(b6)) { printf("FAIL: non-finite arp hold\n"); return 1; }
            if (k > 10) latchedPeak = juce::jmax(latchedPeak, peakAbs(b6));
        }
        printf("latched arp peak = %f\n", latchedPeak);
        CHECK(latchedPeak > 0.01f, "hold latches arp after release");
        setP("arphold", 0.0f); // release hold -> must unlatch
        float tailPeak = 0.0f;
        for (int k = 0; k < 260; ++k) // ~3 s
        {
            m6.clear(); b6.clear(); p6->processBlock(b6, m6);
            if (!isFiniteBuffer(b6)) { printf("FAIL: non-finite after unlatch\n"); return 1; }
            if (k > 200) tailPeak = juce::jmax(tailPeak, peakAbs(b6));
        }
        printf("post-unlatch tail = %f\n", tailPeak);
        CHECK(tailPeak < 0.01f, "hold release unlatches (notes stopped)");
        p6->releaseResources();
    }
    printf("--- init action + growl recipe + max reverb tail ---\n");
    {
        std::unique_ptr<Ersa8Processor> p7(new Ersa8Processor());
        p7->setRateAndBufferSizeDetails(44100, 512);
        p7->prepareToPlay(44100.0, 512);
        p7->setCurrentProgram(27);
        p7->loadInit();
        CHECK(std::abs(p7->apvts.getRawParameterValue("eqhigh")->load()) < 1e-3f, "init resets params");
        CHECK(std::abs(p7->apvts.getRawParameterValue("vco1wave")->load() - 1.0f) < 1e-3f, "init restores saw");
        p7->setCurrentProgram(15); // Mono Growl
        CHECK(std::abs(p7->apvts.getRawParameterValue("xmod")->load() - 0.6f) < 1e-4f, "growl has cross-mod");
        CHECK(p7->apvts.getRawParameterValue("sync")->load() > 0.5f, "growl has sync");
        CHECK(std::abs(p7->apvts.getRawParameterValue("voicemode")->load() - 1.0f) < 1e-3f, "growl is mono");
        // growl must snarl, not whisper
        juce::AudioBuffer<float> b7(2, 512);
        juce::MidiBuffer m7;
        float gpeak = 0.0f;
        for (int k = 0; k < 60; ++k)
        {
            m7.clear();
            if (k == 0) m7.addEvent(juce::MidiMessage::noteOn(1, 40, 1.0f), 0);
            b7.clear(); p7->processBlock(b7, m7);
            if (!isFiniteBuffer(b7)) { printf("FAIL: non-finite growl\n"); return 1; }
            if (k > 4) gpeak = juce::jmax(gpeak, peakAbs(b7));
        }
        printf("growl peak = %f\n", gpeak);
        CHECK(gpeak > 0.15f, "growl has body");
        // max-size reverb: huge but stable, must decay after release
        if (auto* rs = p7->apvts.getParameter("rvsize"))
            rs->setValueNotifyingHost(rs->convertTo0to1(1.0f));
        if (auto* ro = p7->apvts.getParameter("rvon"))
            ro->setValueNotifyingHost(1.0f);
        for (int k = 0; k < 100; ++k) // release the note, let envelopes close (~1.2 s)
        {
            m7.clear();
            if (k == 0) m7.addEvent(juce::MidiMessage::noteOff(1, 40), 0);
            b7.clear(); p7->processBlock(b7, m7);
            if (!isFiniteBuffer(b7)) { printf("FAIL: non-finite release\n"); return 1; }
        }
        float tpeak = 0.0f;
        for (int k = 0; k < 450; ++k) // ~5.2 s of tail
        {
            m7.clear(); b7.clear(); p7->processBlock(b7, m7);
            if (!isFiniteBuffer(b7)) { printf("FAIL: reverb runaway at block %d\n", k); return 1; }
            if (k > 380) tpeak = juce::jmax(tpeak, peakAbs(b7));
        }
        printf("max verb tail = %f\n", tpeak);
        CHECK(tpeak < 0.01f, "max reverb decays (stable)");
        p7->releaseResources();
    }
    printf("--- sub-20Hz energy (Sub preset, low C) ---\n");
    {
        std::unique_ptr<Ersa8Processor> p8(new Ersa8Processor());
        p8->setRateAndBufferSizeDetails(44100, 512);
        p8->prepareToPlay(44100.0, 512);
        p8->setCurrentProgram(0); // Sub Foundation
        juce::AudioBuffer<float> b8(2, 512);
        juce::MidiBuffer m8;
        std::vector<float> steady;
        for (int k = 0; k < 172; ++k) // ~2 s
        {
            m8.clear();
            if (k == 0) m8.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0f), 0);
            b8.clear(); p8->processBlock(b8, m8);
            if (!isFiniteBuffer(b8)) { printf("FAIL: non-finite sub test\n"); return 1; }
            if (k > 25)
                for (int i = 0; i < 512; ++i) steady.push_back(0.5f * (b8.getSample(0, i) + b8.getSample(1, i)));
        }
        const int N = 16384;
        juce::dsp::FFT fft(14);
        std::vector<float> fbuf(2 * N, 0.0f);
        for (int i = 0; i < N && i < (int)steady.size(); ++i)
        {
            float w = 0.5f - 0.5f * std::cos(2.0f * 3.14159265f * (float)i / (float)N);
            fbuf[i] = steady[(size_t)i] * w;
        }
        fft.performFrequencyOnlyForwardTransform(fbuf.data(), true);
        double dc = 0.0;
        for (int i = 0; i < (int)steady.size(); ++i) dc += steady[(size_t)i];
        dc /= (double)steady.size();
        // rumble metric: strongest bin in the 8-20 Hz analyzer band, rel. fundamental
        float fund = 0.0f, rumble = 0.0f;
        for (int b = 0; b < N / 2; ++b)
        {
            float f = (float)b * 44100.0f / (float)N;
            if (f > 25.0f && f < 45.0f) fund = juce::jmax(fund, fbuf[b]);
            if (f >= 8.0f && f < 20.0f) rumble = juce::jmax(rumble, fbuf[b]);
        }
        double rumbleDb = 20.0 * std::log10(rumble / juce::jmax(fund, 1e-6f));
        printf("8-20Hz rumble = %.1f dB rel fundamental, DC = %.5f\n", rumbleDb, dc);
        CHECK(rumbleDb < -45.0, "no subsonic rumble");
        CHECK(std::abs(dc) < 0.005, "no DC offset");
        p8->releaseResources();
    }
    printf("--- transient scan: 60 s held pad, per-channel, timed ---\n");
    {
        std::unique_ptr<Ersa8Processor> ps(new Ersa8Processor());
        ps->setRateAndBufferSizeDetails(44100, 512);
        ps->prepareToPlay(44100.0, 512);
        ps->setCurrentProgram(33); // Dark Pad: slow LFO + chorus + reverb
        juce::AudioBuffer<float> bs(2, 512);
        juce::MidiBuffer ms;
        float worstL = 0.0f, worstR = 0.0f;
        long wposL = 0, wposR = 0, nclick = 0;
        float prevL = 0.0f, prevR = 0.0f;
        long t0 = 0;
        double tRender = 0.0;
        const int blocks = (int)(44100.0 * 60.0 / 512.0);
        for (int k = 0; k < blocks; ++k)
        {
            ms.clear();
            if (k == 0)
            {
                ms.addEvent(juce::MidiMessage::noteOn(1, 48, 0.8f), 0);
                ms.addEvent(juce::MidiMessage::noteOn(1, 55, 0.8f), 10);
                ms.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 20);
            }
            bs.clear();
            auto tStart = juce::Time::getHighResolutionTicks();
            ps->processBlock(bs, ms);
            tRender = juce::jmax(tRender, juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - tStart) * 1000.0);
            if (!isFiniteBuffer(bs)) { printf("FAIL: non-finite 60s scan\n"); return 1; }
            if (k > 44)
            {
                for (int i = 0; i < 512; ++i)
                {
                    float dL = std::abs(bs.getSample(0, i) - prevL);
                    float dR = std::abs(bs.getSample(1, i) - prevR);
                    if (dL > worstL) { worstL = dL; wposL = (long)k * 512 + i; }
                    if (dR > worstR) { worstR = dR; wposR = (long)k * 512 + i; }
                    if ((dL > 0.3f || dR > 0.3f) && nclick < 8)
                    {
                        printf("  transient %.3f @ %.2fs %s\n", juce::jmax(dL, dR),
                               (float)((long)k * 512 + i) / 44100.0f, dL > dR ? "L" : "R");
                        ++nclick;
                    }
                    prevL = bs.getSample(0, i); prevR = bs.getSample(1, i);
                }
            }
            else if (k == 44) { prevL = bs.getSample(0, 511); prevR = bs.getSample(1, 511); t0 = 0; (void)t0; }
        }
        printf("60s scan: worstL=%.3f @%.1fs worstR=%.3f @%.1fs maxBlock=%.2fms/budget=11.6ms\n",
               worstL, (float)wposL / 44100.0f, worstR, (float)wposR / 44100.0f, tRender);
        if (worstL > 0.35f || worstR > 0.35f) { printf("FAIL: spontaneous click\n"); return 1; }
        if (tRender > 8.0) { printf("FAIL: CPU too hot (dropout risk)\n"); return 1; }
        ps->releaseResources();
    }
    printf("--- transient scan: slow square LFO torture ---\n");
    {
        std::unique_ptr<Ersa8Processor> pq2(new Ersa8Processor());
        pq2->setRateAndBufferSizeDetails(44100, 512);
        pq2->prepareToPlay(44100.0, 512);
        pq2->setCurrentProgram(41); // Night Pad
        auto setF = [&](const char* id, float v){
            if (auto* prm = pq2->apvts.getParameter(id))
                prm->setValueNotifyingHost(prm->convertTo0to1(v));
        };
        setF("lfowave", 3.0f); // square
        setF("lforate", 0.3f);
        setF("lfovcf", 0.8f);
        juce::AudioBuffer<float> bq2(2, 512);
        juce::MidiBuffer mq2;
        float worst = 0.0f;
        float prevL = 0.0f, prevR = 0.0f;
        const int blocks = (int)(44100.0 * 25.0 / 512.0);
        for (int k = 0; k < blocks; ++k)
        {
            mq2.clear();
            if (k == 0) mq2.addEvent(juce::MidiMessage::noteOn(1, 48, 0.8f), 0);
            bq2.clear(); pq2->processBlock(bq2, mq2);
            if (!isFiniteBuffer(bq2)) { printf("FAIL: non-finite square LFO\n"); return 1; }
            if (k > 44)
            {
                for (int i = 0; i < 512; ++i)
                {
                    float d = juce::jmax(std::abs(bq2.getSample(0, i) - prevL),
                                         std::abs(bq2.getSample(1, i) - prevR));
                    if (d > worst) worst = d;
                    prevL = bq2.getSample(0, i); prevR = bq2.getSample(1, i);
                }
            }
            else if (k == 44) { prevL = bq2.getSample(0, 511); prevR = bq2.getSample(1, 511); }
        }
        printf("slow square LFO: worst step = %.3f\n", worst);
        if (worst > 0.4f) { printf("FAIL: LFO thump\n"); return 1; }
        pq2->releaseResources();
    }
    printf("--- transient scan: 20 s busy legato churn (steal torture) ---\n");
    {
        std::unique_ptr<Ersa8Processor> pq(new Ersa8Processor());
        pq->setRateAndBufferSizeDetails(44100, 512);
        pq->prepareToPlay(44100.0, 512);
        pq->setCurrentProgram(48); // String Ensemble, poly, slow release
        juce::AudioBuffer<float> bq(2, 512);
        juce::MidiBuffer mq;
        float worst = 0.0f;
        float prevL = 0.0f, prevR = 0.0f;
        int notes[] = { 48, 55, 60, 64, 67, 60, 55, 52, 48, 53, 57, 60 };
        const int blocks = (int)(44100.0 * 20.0 / 512.0);
        for (int k = 0; k < blocks; ++k)
        {
            mq.clear();
            if (k % 43 == 0) // new overlapping note ~every 0.5 s, never released (churn)
                mq.addEvent(juce::MidiMessage::noteOn(1, notes[(k / 43) % 12], 0.9f), 0);
            bq.clear(); pq->processBlock(bq, mq);
            if (!isFiniteBuffer(bq)) { printf("FAIL: non-finite churn\n"); return 1; }
            if (k > 44)
            {
                for (int i = 0; i < 512; ++i)
                {
                    float d = juce::jmax(std::abs(bq.getSample(0, i) - prevL),
                                         std::abs(bq.getSample(1, i) - prevR));
                    if (d > worst) worst = d;
                    prevL = bq.getSample(0, i); prevR = bq.getSample(1, i);
                }
            }
            else if (k == 44) { prevL = bq.getSample(0, 511); prevR = bq.getSample(1, 511); }
        }
        printf("churn: worst single-sample step = %.3f\n", worst);
        if (worst > 0.45f) { printf("FAIL: steal click\n"); return 1; }
        pq->releaseResources();
    }
    printf("--- transient scan: 12 s held chords must not click ---\n");
    {
        int scanPresets[] = { 41, 43, 83 };
        const char* scanNames[] = { "Night Pad", "Lunar Pad", "Pulsing Seq" };
        for (int pi = 0; pi < 3; ++pi)
        {
            std::unique_ptr<Ersa8Processor> ps(new Ersa8Processor());
            ps->setRateAndBufferSizeDetails(44100, 512);
            ps->prepareToPlay(44100.0, 512);
            ps->setCurrentProgram(scanPresets[pi]);
            juce::AudioBuffer<float> bs(2, 512);
            juce::MidiBuffer ms;
            float worst = 0.0f; long wpos = 0;
            float prevL = 0.0f, prevR = 0.0f;
            const int blocks = (int)(44100.0 * 12.0 / 512.0);
            for (int k = 0; k < blocks; ++k)
            {
                ms.clear();
                if (k == 0)
                {
                    ms.addEvent(juce::MidiMessage::noteOn(1, 48, 0.8f), 0);
                    ms.addEvent(juce::MidiMessage::noteOn(1, 55, 0.8f), 10);
                    ms.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 20);
                }
                bs.clear(); ps->processBlock(bs, ms);
                if (!isFiniteBuffer(bs)) { printf("FAIL: non-finite scan %s\n", scanNames[pi]); return 1; }
                if (k > 44) // skip attacks (~0.5 s)
                {
                    for (int i = 0; i < 512; ++i)
                    {
                        float dL = std::abs(bs.getSample(0, i) - prevL);
                        float dR = std::abs(bs.getSample(1, i) - prevR);
                        float d = juce::jmax(dL, dR);
                        if (d > worst) { worst = d; wpos = (long)k * 512 + i; }
                        prevL = bs.getSample(0, i); prevR = bs.getSample(1, i);
                    }
                }
                else if (k == 44) { prevL = bs.getSample(0, 511); prevR = bs.getSample(1, 511); }
            }
            printf("%s: worst single-sample step = %.3f @ %.1fs\n",
                   scanNames[pi], worst, (float)wpos / 44100.0f);
            if (worst > 0.35f) { printf("FAIL: click detected in %s\n", scanNames[pi]); return 1; }
            ps->releaseResources();
        }
        printf("ok: no spontaneous transients\n");
    }
    printf("--- glitch torture: fast resonant attacks must not slam rails ---\n");
    {
        int torture[] = { 5, 13, 19, 24, 98, 104, 27, 15 }; // Acid/Reso/Sync/Metal/Riser/Sweep/Distort/Growl
        float worstPeak = 0.0f;
        for (int t = 0; t < 8; ++t)
        {
            std::unique_ptr<Ersa8Processor> pg(new Ersa8Processor());
            pg->setRateAndBufferSizeDetails(44100, 512);
            pg->prepareToPlay(44100.0, 512);
            pg->setCurrentProgram(torture[t]);
            juce::AudioBuffer<float> bg(2, 512);
            juce::MidiBuffer mg;
            int notes[] = { 36, 60, 48, 72, 40 };
            for (int k = 0; k < 130; ++k)
            {
                mg.clear();
                if (k % 26 == 0) // staccato jumps incl. octave leaps
                {
                    mg.addEvent(juce::MidiMessage::noteOff(1, 30 + (k * 7) % 60), 0);
                    mg.addEvent(juce::MidiMessage::noteOn(1, notes[(k / 26) % 5], 1.0f), 0);
                }
                bg.clear(); pg->processBlock(bg, mg);
                if (!isFiniteBuffer(bg)) { printf("FAIL: non-finite torture preset %d\n", torture[t]); return 1; }
                if (k > 2) worstPeak = juce::jmax(worstPeak, peakAbs(bg));
            }
            pg->releaseResources();
        }
        printf("torture worst peak = %.3f\n", worstPeak);
        CHECK(worstPeak < 0.9f, "limiter contains bursts");
    }
    printf("--- extreme-state bursts: phaser howl, xmod jump, all-hot ---\n");
    {
        auto setF = [](Ersa8Processor& pr, const char* id, float v){
            if (auto* prm = pr.apvts.getParameter(id))
                prm->setValueNotifyingHost(prm->convertTo0to1(v));
        };
        // A: phaser max feedback + depth, slow sweep, sustained chord
        {
            std::unique_ptr<Ersa8Processor> pa(new Ersa8Processor());
            pa->setRateAndBufferSizeDetails(44100, 512);
            pa->prepareToPlay(44100.0, 512);
            pa->loadInit();
            setF(*pa, "phon", 1.0f); setF(*pa, "phrate", 0.15f);
            setF(*pa, "phdepth", 1.0f); setF(*pa, "phfb", 0.9f); setF(*pa, "phmix", 0.8f);
            juce::AudioBuffer<float> ba(2, 512); juce::MidiBuffer ma;
            float peak = 0.0f;
            for (int k = 0; k < 430; ++k)
            {
                ma.clear();
                if (k == 0) { ma.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
                              ma.addEvent(juce::MidiMessage::noteOn(1, 64, 1.0f), 0); }
                ba.clear(); pa->processBlock(ba, ma);
                if (!isFiniteBuffer(ba)) { printf("FAIL: non-finite phaser\n"); return 1; }
                if (k > 10) peak = juce::jmax(peak, peakAbs(ba));
            }
            printf("phaser-howl peak = %.3f\n", peak);
        if (peak > 0.9f) { printf("FAIL: phaser too hot\n"); return 1; }
            pa->releaseResources();
        }
        // B: XMOD automation jump 0 -> 1 mid-note
        {
            std::unique_ptr<Ersa8Processor> pb(new Ersa8Processor());
            pb->setRateAndBufferSizeDetails(44100, 512);
            pb->prepareToPlay(44100.0, 512);
            pb->loadInit();
            setF(*pb, "cutoff", 6000.0f);
            juce::AudioBuffer<float> bb(2, 512); juce::MidiBuffer mb2;
            float worst = 0.0f, prevL = 0.0f, prevR = 0.0f;
            for (int k = 0; k < 130; ++k)
            {
                mb2.clear();
                if (k == 0) mb2.addEvent(juce::MidiMessage::noteOn(1, 48, 1.0f), 0);
                if (k == 60) setF(*pb, "xmod", 1.0f);
                bb.clear(); pb->processBlock(bb, mb2);
                if (!isFiniteBuffer(bb)) { printf("FAIL: non-finite xmod\n"); return 1; }
                for (int i = 0; i < 512; ++i)
                {
                    float d = juce::jmax(std::abs(bb.getSample(0, i) - prevL),
                                         std::abs(bb.getSample(1, i) - prevR));
                    if (d > worst) worst = d;
                    prevL = bb.getSample(0, i); prevR = bb.getSample(1, i);
                }
            }
            printf("xmod-jump worst step = %.3f\n", worst);
            pb->releaseResources();
        }
        // C: all-knobs-hot init patch, staccato
        {
            std::unique_ptr<Ersa8Processor> pc(new Ersa8Processor());
            pc->setRateAndBufferSizeDetails(44100, 512);
            pc->prepareToPlay(44100.0, 512);
            pc->loadInit();
            setF(*pc, "cutoff", 18000.0f); setF(*pc, "reso", 0.95f);
            setF(*pc, "fenv", 1.0f); setF(*pc, "u1", 3.0f); setF(*pc, "u2", 3.0f);
            setF(*pc, "drive", 1.0f); setF(*pc, "eqhigh", 12.0f);
            setF(*pc, "ston", 1.0f); setF(*pc, "stamt", 1.0f);
            setF(*pc, "dlon", 1.0f); setF(*pc, "dlfb", 0.85f);
            setF(*pc, "rvon", 1.0f); setF(*pc, "rvmix", 0.8f);
            juce::AudioBuffer<float> bc(2, 512); juce::MidiBuffer mc;
            float peak = 0.0f;
            int notes[] = { 36, 72, 48, 84 };
            for (int k = 0; k < 130; ++k)
            {
                mc.clear();
                if (k % 26 == 0)
                {
                    mc.addEvent(juce::MidiMessage::noteOn(1, notes[(k / 26) % 4], 1.0f), 0);
                }
                bc.clear(); pc->processBlock(bc, mc);
                if (!isFiniteBuffer(bc)) { printf("FAIL: non-finite all-hot\n"); return 1; }
                if (k > 2) peak = juce::jmax(peak, peakAbs(bc));
            }
            printf("all-hot peak = %.3f\n", peak);
        if (peak > 0.9f) { printf("FAIL: all-hot too hot\n"); return 1; }
            pc->releaseResources();
        }
    }
    printf("--- rate + altitude: 48k/96k and high notes ---\n");
    {
        for (double sr : { 48000.0, 96000.0 })
        {
            std::unique_ptr<Ersa8Processor> pr(new Ersa8Processor());
            pr->setRateAndBufferSizeDetails((int)sr, 512);
            pr->prepareToPlay(sr, 512);
            pr->setCurrentProgram(16);
            juce::AudioBuffer<float> br(2, 512);
            juce::MidiBuffer mr;
            float peak = 0.0f;
            for (int k = 0; k < 100; ++k)
            {
                mr.clear();
                if (k == 0) mr.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
                br.clear(); pr->processBlock(br, mr);
                if (!isFiniteBuffer(br)) { printf("FAIL: non-finite @ %.0f\n", sr); return 1; }
                if (k > 5) peak = juce::jmax(peak, peakAbs(br));
            }
            printf("@%.0fk: peak=%.3f %s\n", sr / 1000.0, peak, peak < 1.0f ? "OK" : "*** HOT ***");
            pr->releaseResources();
        }
        // high notes, open filter, high reso, every wave
        for (int wv = 0; wv < 4; ++wv)
        {
            std::unique_ptr<Ersa8Processor> ph(new Ersa8Processor());
            ph->setRateAndBufferSizeDetails(44100, 512);
            ph->prepareToPlay(44100.0, 512);
            ph->loadInit();
            auto setF = [&](const char* id, float v){
                if (auto* prm = ph->apvts.getParameter(id))
                    prm->setValueNotifyingHost(prm->convertTo0to1(v));
            };
            auto setC = [&](const char* id, float idx){
                if (auto* prm = ph->apvts.getParameter(id))
                    prm->setValueNotifyingHost(prm->convertTo0to1(idx));
            };
            setC("vco1wave", (float)wv); setF("vco1level", 0.9f); setF("vco2level", 0.0f);
            setF("cutoff", 18000.0f); setF("reso", 0.9f); setF("fenv", 0.0f);
            juce::AudioBuffer<float> bh(2, 512);
            juce::MidiBuffer mh;
            float peak = 0.0f;
            for (int k = 0; k < 100; ++k)
            {
                mh.clear();
                if (k == 0) mh.addEvent(juce::MidiMessage::noteOn(1, 96, 1.0f), 0); // C7
                bh.clear(); ph->processBlock(bh, mh);
                if (!isFiniteBuffer(bh)) { printf("FAIL: non-finite high wave %d\n", wv); return 1; }
                if (k > 5) peak = juce::jmax(peak, peakAbs(bh));
            }
            printf("C7 wave %d open+reso: peak=%.3f %s\n", wv, peak, peak < 0.95f ? "OK" : "*** HOT ***");
            ph->releaseResources();
        }
    }
    printf("--- full bank sweep: every preset, notes + chord ---\n");
    {
        float bankWorst = 0.0f;
        int bankWorstIdx = -1;
        for (int pi = 0; pi < proc->getNumPrograms(); ++pi)
        {
            std::unique_ptr<Ersa8Processor> pq(new Ersa8Processor());
            pq->setRateAndBufferSizeDetails(44100, 512);
            pq->prepareToPlay(44100.0, 512);
            pq->setCurrentProgram(pi);
            juce::AudioBuffer<float> bq(2, 512);
            juce::MidiBuffer mq;
            float peak = 0.0f;
            for (int k = 0; k < 100; ++k)
            {
                mq.clear();
                if (k == 0) { mq.addEvent(juce::MidiMessage::noteOn(1, 48, 1.0f), 0);
                              mq.addEvent(juce::MidiMessage::noteOn(1, 55, 0.9f), 5); }
                if (k == 50) mq.addEvent(juce::MidiMessage::noteOff(1, 48), 0);
                bq.clear(); pq->processBlock(bq, mq);
                if (!isFiniteBuffer(bq)) { printf("FAIL: non-finite preset %d\n", pi); return 1; }
                if (k > 2) peak = juce::jmax(peak, peakAbs(bq));
            }
            if (peak > bankWorst) { bankWorst = peak; bankWorstIdx = pi; }
            if (peak > 0.95f)
                printf("  HOT preset %3d (%s): peak %.3f\n", pi, presetName(pi).toRawUTF8(), peak);
            pq->releaseResources();
        }
        printf("bank sweep done, worst = preset %d peak %.3f\n", bankWorstIdx, bankWorst);
    }
    printf("--- live unison drag + preset switching under sustain ---\n");
    {
        std::unique_ptr<Ersa8Processor> pu2(new Ersa8Processor());
        pu2->setRateAndBufferSizeDetails(44100, 512);
        pu2->prepareToPlay(44100.0, 512);
        pu2->setCurrentProgram(48);
        auto setF = [&](const char* id, float v){
            if (auto* prm = pu2->apvts.getParameter(id))
                prm->setValueNotifyingHost(prm->convertTo0to1(v));
        };
        auto setC = [&](const char* id, float idx){
            if (auto* prm = pu2->apvts.getParameter(id))
                prm->setValueNotifyingHost(prm->convertTo0to1(idx));
        };
        juce::AudioBuffer<float> bu2(2, 512);
        juce::MidiBuffer mu2;
        float worst = 0.0f;
        float prevL = 0.0f, prevR = 0.0f;
        int progs[] = { 48, 51, 57, 63, 32, 16 };
        for (int k = 0; k < 260; ++k)
        {
            mu2.clear();
            if (k == 0) { mu2.addEvent(juce::MidiMessage::noteOn(1, 48, 0.9f), 0);
                          mu2.addEvent(juce::MidiMessage::noteOn(1, 55, 0.9f), 0); }
            if (k == 20) { setC("u1", 3.0f); setC("u2", 3.0f); }       // unison 1 -> 4 live
            if (k == 60) { setC("u1", 0.0f); setC("u2", 0.0f); }       // and back
            if (k >= 100 && (k - 100) % 26 == 0)                        // program hop while held
                pu2->setCurrentProgram(progs[((k - 100) / 26) % 6]);
            bu2.clear(); pu2->processBlock(bu2, mu2);
            if (!isFiniteBuffer(bu2)) { printf("FAIL: non-finite live change\n"); return 1; }
            if (k > 4)
            {
                for (int i = 0; i < 512; ++i)
                {
                    float d = juce::jmax(std::abs(bu2.getSample(0, i) - prevL),
                                         std::abs(bu2.getSample(1, i) - prevR));
                    if (d > worst) worst = d;
                    prevL = bu2.getSample(0, i); prevR = bu2.getSample(1, i);
                }
            }
        }
        float peak = peakAbs(bu2);
        printf("live-change worst step = %.3f (final peak %.3f)\n", worst, peak);
        if (worst > 0.9f) { printf("FAIL: live-change burst\n"); return 1; }
        pu2->releaseResources();
    }
    printf("--- user preset save/scan/load/delete ---\n");
    {
        juce::File tmpDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile("ersa-harness-presets");
        tmpDir.deleteRecursively();
        setUserPresetDirForTest(tmpDir);
        std::unique_ptr<Ersa8Processor> pu(new Ersa8Processor());
        pu->setRateAndBufferSizeDetails(44100, 512);
        pu->prepareToPlay(44100.0, 512);
        CHECK(scanUserPresets().empty(), "user bank starts empty");
        pu->setCurrentProgram(27);
        CHECK(saveUserPreset("Harness Test!", *&pu->apvts), "save user preset");
        {
            auto st = pu->apvts.copyState();
            for (int i = 0; i < st.getNumChildren(); ++i)
            {
                auto ch = st.getChild(i);
                if (ch.getProperty("id").toString() == "eqhigh")
                    printf("live tree eqhigh value=%s\n", ch.getProperty("value").toString().toRawUTF8());
            }
        }
        CHECK(saveUserPreset("Second One", *&pu->apvts), "save second preset");
        auto found = scanUserPresets();
        CHECK(found.size() == 2, "scan finds both");
        // name sanitised (no bang) and sorted
        CHECK(found[0].name == "Harness Test", "name sanitised");
        pu->setCurrentProgram(0);
        CHECK(loadUserPreset(found[0].file, pu->apvts), "load user preset");
        printf("eqhigh after load = %f\n", pu->apvts.getRawParameterValue("eqhigh")->load());
        CHECK(std::abs(pu->apvts.getRawParameterValue("eqhigh")->load() - 4.0f) < 0.01f,
              "loaded values match");
        CHECK(!loadUserPreset(tmpDir.getChildFile("nope.xml"), pu->apvts), "missing file fails clean");
        CHECK(deleteUserPreset(found[0].file), "delete works");
        CHECK(scanUserPresets().size() == 1, "one remains");
        // safety: cannot delete outside the user dir
        CHECK(!deleteUserPreset(juce::File("/tmp/ersa-harness-presets-evil.xml")), "delete confined");
        tmpDir.deleteRecursively();
        pu->releaseResources();
        printf("ok: user preset round-trip\n");
    }
    printf("--- zap hunt: HF-flux spikes on sustained presets ---\n");
    {
        struct ZP { int preset; const char* name; };
        ZP tests[] = { { -1, "INIT" }, { 41, "Night" }, { 43, "Lunar" }, { 106, "Water" },
                       { 103, "Ghost" }, { 19, "SyncLead" }, { 105, "Alien" }, { 82, "ArpRnd" },
                       { 83, "Pulsing" }, { 107, "Storm" } };
        for (auto& t : tests)
        {
            std::unique_ptr<Ersa8Processor> pz(new Ersa8Processor());
            pz->setRateAndBufferSizeDetails(44100, 512);
            pz->prepareToPlay(44100.0, 512);
            if (t.preset < 0) pz->loadInit();
            else pz->setCurrentProgram(t.preset);
            juce::AudioBuffer<float> bz(2, 512);
            juce::MidiBuffer mz;
            // 2kHz HP for zap-band energy
            float lp = 0.0f;
            float gHP = 1.0f - std::exp(-2.0f * 3.14159265f * 2000.0f / 44100.0f);
            std::vector<float> env;
            for (int k = 0; k < 2584; ++k) // ~30 s
            {
                mz.clear();
                if (k == 0) { mz.addEvent(juce::MidiMessage::noteOn(1, 48, 0.9f), 0);
                              mz.addEvent(juce::MidiMessage::noteOn(1, 55, 0.9f), 0); }
                bz.clear(); pz->processBlock(bz, mz);
                if (!isFiniteBuffer(bz)) { printf("FAIL: non-finite zap %s\n", t.name); return 1; }
                if (k > 44)
                {
                    double e = 0.0;
                    for (int i = 0; i < 512; ++i)
                    {
                        float m = 0.5f * (bz.getSample(0, i) + bz.getSample(1, i));
                        lp += gHP * (m - lp);
                        float hp = m - lp;
                        e += (double)hp * (double)hp;
                    }
                    env.push_back((float)(e / 512.0));
                }
            }
            // median + spike count (>8x median, above -50dBFS floor)
            std::vector<float> srt = env;
            std::nth_element(srt.begin(), srt.begin() + srt.size() / 2, srt.end());
            float med = srt[srt.size() / 2];
            int spikes = 0; float worst = 0.0f;
            for (size_t i = 0; i < env.size(); ++i)
            {
                if (env[i] > med * 8.0f && env[i] > 1e-5f) { ++spikes; worst = juce::jmax(worst, env[i] / med); }
            }
            printf("%-8s: HF spikes=%3d worst-xmed=%.0f %s\n", t.name, spikes, worst,
                   spikes > 4 ? "*** ZAPPY ***" : "calm");
            pz->releaseResources();
        }
    }
    printf("--- pitch bend: center holds, full deflects by range ---\n");
    {
        auto measureA4 = [&](int bendPos) {
            std::unique_ptr<Ersa8Processor> pb(new Ersa8Processor());
            pb->setRateAndBufferSizeDetails(44100, 512);
            pb->prepareToPlay(44100.0, 512);
            pb->loadInit();
            if (auto* ch = pb->apvts.getParameter("chon")) ch->setValueNotifyingHost(0.0f);
            juce::AudioBuffer<float> bb(2, 512);
            juce::MidiBuffer mb2;
            std::vector<float> steady;
            for (int k = 0; k < 86; ++k)
            {
                mb2.clear();
                if (k == 0) mb2.addEvent(juce::MidiMessage::noteOn(1, 69, 0.9f), 0);
                if (k == 10 && bendPos >= 0)
                    mb2.addEvent(juce::MidiMessage::pitchWheel(1, bendPos), 0);
                bb.clear(); pb->processBlock(bb, mb2);
                if (k >= 40 && k < 80)
                    for (int i = 0; i < 512; ++i) steady.push_back(bb.getSample(0, i));
            }
            const int N = 16384;
            juce::dsp::FFT fft(14);
            std::vector<float> fbuf(2 * N, 0.0f);
            for (int i = 0; i < N && i < (int)steady.size(); ++i)
            {
                float w = 0.5f - 0.5f * std::cos(2.0f * 3.14159265f * (float)i / (float)N);
                fbuf[i] = steady[(size_t)i] * w;
            }
            {
                static bool dumped = false;
                if (!dumped)
                {
                    dumped = true;
                    juce::File f("/tmp/iso_raw.bin");
                    f.deleteFile();
                    juce::FileOutputStream fos(f);
                    for (size_t zz = 0; zz < steady.size(); ++zz) fos.writeFloat(steady[zz]);
                    printf("dumped %d steady samples\n", (int)steady.size());
                }
            }
            fft.performFrequencyOnlyForwardTransform(fbuf.data(), true);
            float expBin = 440.0f * (float)N / 44100.0f;
            int lo = juce::jmax(2, (int)(expBin * 0.9f)), hi = (int)(expBin * 1.12f) + 1;
            int peakBin = lo; float peakVal = 0.0f;
            for (int b = lo; b <= hi && b < N / 2; ++b)
                if (fbuf[b] > peakVal) { peakVal = fbuf[b]; peakBin = b; }
            return (float)peakBin * 44100.0f / (float)N;
        };
        float fCenter = measureA4(8192);
        float fTop = measureA4(16383);
        printf("bend center=%.1fHz top=%.1fHz\n", fCenter, fTop);
        float errC = std::abs(1200.0f * std::log2(fCenter / 440.0f));
        float errT = std::abs(1200.0f * std::log2(fTop / (440.0f * std::pow(2.0f, 2.0f / 12.0f))));
        CHECK(errC < 20.0f, "bend center does not detune");
        CHECK(errT < 25.0f, "full bend reaches +2st");
    }
    printf("--- chorus mode switch under sustain must not zap ---\n");
    {
        std::unique_ptr<Ersa8Processor> pc2(new Ersa8Processor());
        pc2->setRateAndBufferSizeDetails(44100, 512);
        pc2->prepareToPlay(44100.0, 512);
        pc2->setCurrentProgram(45); // Wide Pad (chorus wide)
        auto setC = [&](const char* id, float idx){
            if (auto* prm = pc2->apvts.getParameter(id))
                prm->setValueNotifyingHost(prm->convertTo0to1(idx));
        };
        juce::AudioBuffer<float> bc2(2, 512);
        juce::MidiBuffer mc2;
        float worst = 0.0f;
        float prevL = 0.0f, prevR = 0.0f;
        for (int k = 0; k < 130; ++k)
        {
            mc2.clear();
            if (k == 0) mc2.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9f), 0);
            if (k == 40) setC("chmode", 0.0f); // wide -> manual mid-sustain
            if (k == 80) setC("chmode", 1.0f); // -> vintage I
            bc2.clear(); pc2->processBlock(bc2, mc2);
            if (!isFiniteBuffer(bc2)) { printf("FAIL: non-finite chorus switch\n"); return 1; }
            if (k > 4)
            {
                for (int i = 0; i < 512; ++i)
                {
                    float d = juce::jmax(std::abs(bc2.getSample(0, i) - prevL),
                                         std::abs(bc2.getSample(1, i) - prevR));
                    if (d > worst) worst = d;
                    prevL = bc2.getSample(0, i); prevR = bc2.getSample(1, i);
                }
            }
        }
        printf("chorus-switch worst step = %.3f\n", worst);
        if (worst > 0.5f) { printf("FAIL: chorus switch zaps\n"); return 1; }
        pc2->releaseResources();
    }
    printf("--- S&H + latched-arp hands-free behavior ---\n");
    {
        // S&H LFO -> filter: deliberate random bleeps, must stay musical (bounded)
        std::unique_ptr<Ersa8Processor> ph(new Ersa8Processor());
        ph->setRateAndBufferSizeDetails(44100, 512);
        ph->prepareToPlay(44100.0, 512);
        ph->loadInit();
        auto setF = [&](const char* id, float v){
            if (auto* prm = ph->apvts.getParameter(id))
                prm->setValueNotifyingHost(prm->convertTo0to1(v));
        };
        auto setC = [&](const char* id, float idx){
            if (auto* prm = ph->apvts.getParameter(id))
                prm->setValueNotifyingHost(prm->convertTo0to1(idx));
        };
        setC("lfowave", 4.0f); setF("lforate", 2.0f); setF("lfovcf", 0.9f);
        setF("reso", 0.7f); setF("cutoff", 1200.0f);
        juce::AudioBuffer<float> bh(2, 512);
        juce::MidiBuffer mh;
        float peak = 0.0f;
        for (int k = 0; k < 430; ++k)
        {
            mh.clear();
            if (k == 0) mh.addEvent(juce::MidiMessage::noteOn(1, 48, 0.9f), 0);
            bh.clear(); ph->processBlock(bh, mh);
            if (!isFiniteBuffer(bh)) { printf("FAIL: non-finite S&H\n"); return 1; }
            if (k > 44) peak = juce::jmax(peak, peakAbs(bh));
        }
        printf("S&H bleep peak = %.3f %s\n", peak, peak < 0.9f ? "OK (bounded)" : "*** HOT ***");
        ph->releaseResources();
    }
    printf("--- idle gate: long silence is bit-exact zero; panic is instant ---\n");
    {
        std::unique_ptr<Ersa8Processor> pg2(new Ersa8Processor());
        pg2->setRateAndBufferSizeDetails(44100, 512);
        pg2->prepareToPlay(44100.0, 512);
        pg2->setCurrentProgram(44); // Sunset Pad (delay + reverb tails)
        juce::AudioBuffer<float> bg2(2, 512);
        juce::MidiBuffer mg2;
        for (int k = 0; k < 30; ++k)
        {
            mg2.clear();
            if (k == 0) mg2.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
            if (k == 10) mg2.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
            bg2.clear(); pg2->processBlock(bg2, mg2);
            if (!isFiniteBuffer(bg2)) { printf("FAIL: non-finite gate test\n"); return 1; }
        }
        // keep rendering silence: voices die ~3 s in, gate closes 6 s later
        float latePeak = 0.0f;
        bool exactZero = false;
        for (int k = 0; k < 920; ++k) // ~10.7 s
        {
            mg2.clear(); bg2.clear(); pg2->processBlock(bg2, mg2);
            if (!isFiniteBuffer(bg2)) { printf("FAIL: non-finite idle\n"); return 1; }
            if (k > 840) { latePeak = juce::jmax(latePeak, peakAbs(bg2)); exactZero = true; }
        }
        printf("7s-idle peak = %.8f\n", latePeak);
        CHECK(latePeak == 0.0f, "idle output is bit-exact silence");
        (void)exactZero;
        // panic during full voice + tails: next block silent
        mg2.clear(); mg2.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0f), 0);
        bg2.clear(); pg2->processBlock(bg2, mg2);
        pg2->panic();
        mg2.clear(); bg2.clear(); pg2->processBlock(bg2, mg2);
        CHECK(peakAbs(bg2) == 0.0f, "panic silences instantly");
        pg2->releaseResources();
    }
    printf("--- preset policy: leads poly, arps <= 1/16, pads have release ---\n");
    {
        std::unique_ptr<Ersa8Processor> pp(new Ersa8Processor());
        pp->setRateAndBufferSizeDetails(44100, 512);
        pp->prepareToPlay(44100.0, 512);
        auto raw = [&](const char* id){ return pp->apvts.getRawParameterValue(id)->load(); };
        for (int i = 0; i < pp->getNumPrograms(); ++i)
        {
            pp->setCurrentProgram(i);
            if (i >= 16 && i <= 31)
            {
                if ((int)std::round(raw("voicemode")) != 0)
                { printf("FAIL: lead %d not poly\n", i); return 1; }
            }
            if (raw("arpon") > 0.5f)
            {
                int div = (int)std::round(raw("arpdiv"));
                if (div < 0 || div > 12)
                { printf("FAIL: preset %d arp div out of range\n", i); return 1; }
            }
            if ((i >= 32 && i <= 47) || (i >= 128 && i <= 147) || (i >= 200 && i <= 215))
            {
                if (raw("ar") < 1.0f - 1e-6f || raw("fr") < 0.8f - 1e-6f)
                { printf("FAIL: pad %d short release (AR %.2f FR %.2f)\n", i, raw("ar"), raw("fr")); return 1; }
            }
        }
        printf("ok: leads poly, arps capped, pads released (both banks)\n");
        pp->releaseResources();
    }
    printf("--- factory backend: files, shadow, overwrite, restore ---\n");
    {
        const auto& all = getFactoryPresets();
        CHECK((int)all.size() == 256, "256 factory files scanned");
        CHECK(all[128].name == "Neon Rain", "bank 2 parses");
        CHECK(all[255].name == "Tears in the Rain", "bank 2 complete");
        CHECK(all[0].name == "Sub Foundation", "factory names parse");
        CHECK(all[0].index == 0, "factory indices parse");
        std::unique_ptr<Ersa8Processor> pf(new Ersa8Processor());
        pf->setRateAndBufferSizeDetails(44100, 512);
        pf->prepareToPlay(44100.0, 512);
        pf->setCurrentProgram(10);
        CHECK(overwriteFactoryPreset(10, pf->apvts), "overwrite factory preset");
        bool sawShadow = false;
        for (auto& pr : getFactoryPresets())
            if (pr.index == 10) sawShadow = pr.shadowed;
        CHECK(sawShadow, "shadow flagged after overwrite");
        pf->setCurrentProgram(0);
        {
            const auto& a2 = getFactoryPresets();
            for (auto& pr : a2)
                if (pr.index == 10) { applyPreset(pf->apvts, 10); break; }
        }
        CHECK(deleteFactoryShadow(10), "shadow deleted (restore)");
        bool restored = false;
        for (auto& pr : getFactoryPresets())
            if (pr.index == 10) restored = !pr.shadowed;
        CHECK(restored, "shipped file restored");
        CHECK(!deleteFactoryShadow(10), "second delete is a clean no-op");
        pf->releaseResources();
    }
    printf("--- pitch wheel: jitter must not zap, center must stick ---\n");
    {
        // Part 1: noisy wheel (deadband jitter + dirty-pot spikes + full throw)
        // must stay finite and bounded on a sustained note.
        std::unique_ptr<Ersa8Processor> pw(new Ersa8Processor());
        pw->setRateAndBufferSizeDetails(44100, 512);
        pw->prepareToPlay(44100.0, 512);
        pw->loadInit();
        if (auto* ch = pw->apvts.getParameter("chon")) ch->setValueNotifyingHost(0.0f);
        juce::AudioBuffer<float> bw(2, 512);
        juce::MidiBuffer mw;
        float worst = 0.0f;
        float prevL = 0.0f, prevR = 0.0f;
        juce::Random jr(12345);
        for (int k = 0; k < 130; ++k)
        {
            mw.clear();
            if (k == 0) mw.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9f), 0);
            // noisy-wheel simulation: jitter inside the deadband every block,
            // plus occasional large spurious spikes from a dirty pot
            mw.addEvent(juce::MidiMessage::pitchWheel(1, 8192 + (int)(jr.nextFloat() * 80.0f - 40.0f)), 0);
            if (k == 50 || k == 90)
                mw.addEvent(juce::MidiMessage::pitchWheel(1, 8192 + (k == 50 ? 3000 : -3000)), 0);
            // one deliberate full deflection and back, mid-stream
            if (k == 60) mw.addEvent(juce::MidiMessage::pitchWheel(1, 16383), 0);
            if (k == 70) mw.addEvent(juce::MidiMessage::pitchWheel(1, 8192), 0);
            bw.clear(); pw->processBlock(bw, mw);
            if (!isFiniteBuffer(bw)) { printf("FAIL: non-finite bend test\n"); return 1; }
            if (k > 44)
            {
                for (int i = 0; i < 512; ++i)
                {
                    float d = juce::jmax(std::abs(bw.getSample(0, i) - prevL),
                                         std::abs(bw.getSample(1, i) - prevR));
                    if (d > worst) worst = d;
                    prevL = bw.getSample(0, i); prevR = bw.getSample(1, i);
                }
            }
            else if (k == 44) { prevL = bw.getSample(0, 511); prevR = bw.getSample(1, 511); }
        }
        printf("bend-jitter worst step = %.3f\n", worst);
        if (worst > 0.4f) { printf("FAIL: pitch wheel zaps\n"); return 1; }
        pw->releaseResources();
    }
    {
        // Part 2: pitch must SLEW, never step. Sustained sine, one big spike
        // (+6000 LSB ~= +146 cents), track pitch via interpolated zero
        // crossings. Instant-snap code jumps the full 146c between two
        // crossings; slewed code moves ~25c max per interval.
        std::unique_ptr<Ersa8Processor> ps(new Ersa8Processor());
        ps->setRateAndBufferSizeDetails(44100, 512);
        ps->prepareToPlay(44100.0, 512);
        ps->loadInit();
        auto setF = [&](const char* id, float v){
            if (auto* prm = ps->apvts.getParameter(id))
                prm->setValueNotifyingHost(prm->convertTo0to1(v));
        };
        auto setC = [&](const char* id, float idx){
            if (auto* prm = ps->apvts.getParameter(id))
                prm->setValueNotifyingHost(prm->convertTo0to1(idx));
        };
        setF("vco1level", 0.0f); setF("vco2level", 0.9f);
        setC("vco2wave", 3.0f); // sine
        setF("fxbypass", 1.0f);
        // REGRESSION (smoothing self-cannibalisation): with both osc levels
        // at zero a held note must be (near-)silent. The old bug advanced each
        // smoother one sample-step per BLOCK, so levels sat mid-slew for
        // seconds and the "silent" voice sang audibly.
        {
            auto setF0 = [&](const char* id, float v){
                if (auto* prm = ps->apvts.getParameter(id))
                    prm->setValueNotifyingHost(prm->convertTo0to1(v));
            };
            setF0("vco2level", 0.0f);
            juce::AudioBuffer<float> bz(2, 512); juce::MidiBuffer mz;
            double eTot = 0.0; size_t eN = 0;
            for (int k = 0; k < 86; ++k)
            {
                mz.clear();
                if (k == 0) mz.addEvent(juce::MidiMessage::noteOn(1, 69, 0.9f), 0);
                bz.clear(); ps->processBlock(bz, mz);
                if (!isFiniteBuffer(bz)) { printf("FAIL: non-finite silent voice\n"); return 1; }
                if (k >= 30 && k < 80)
                    for (int i = 0; i < 512; ++i) { eTot += (double)bz.getSample(0, i) * bz.getSample(0, i); ++eN; }
            }
            float silentRms = (float)std::sqrt(eTot / (double)eN);
            printf("silent voice rms = %.5f\n", silentRms);
            if (silentRms > 0.005f) { printf("FAIL: silent voice sings (smoothing stuck?)\n"); return 1; }
            // FILTER-CLOSED: shut the VCF to 40 Hz on the restored sine; a
            // pre-filter 440 Hz tone dies >60 dB.
            {
                setF0("vco2level", 0.9f);
                setF0("cutoff", 40.0f); setF0("reso", 0.0f); setF0("fenv", 0.0f);
                juce::AudioBuffer<float> bq(2, 512); juce::MidiBuffer mq;
                double eC = 0.0; size_t eCN = 0;
                for (int k = 0; k < 86; ++k)
                {
                    mq.clear(); bq.clear(); ps->processBlock(bq, mq);
                    if (!isFiniteBuffer(bq)) { printf("FAIL: non-finite filter-closed\n"); return 1; }
                    if (k >= 30 && k < 80)
                        for (int i = 0; i < 512; ++i) { eC += (double)bq.getSample(0, i) * bq.getSample(0, i); ++eCN; }
                }
                float closedRms = (float)std::sqrt(eC / (double)eCN);
                printf("filter-closed rms = %.5f\n", closedRms);
                if (closedRms > 0.01f) { printf("FAIL: tone survives closed filter\n"); return 1; }
                // restore musical state for the slew test below
                setF0("cutoff", 3500.0f); setF0("reso", 0.25f); setF0("fenv", 0.6f);
            }
        }
        juce::AudioBuffer<float> bs(2, 512);
        juce::MidiBuffer ms;
        std::vector<float> mono;
        for (int k = 0; k < 120; ++k)
        {
            ms.clear();
            if (k == 0) ms.addEvent(juce::MidiMessage::noteOn(1, 69, 0.9f), 0);
            if (k == 60) ms.addEvent(juce::MidiMessage::pitchWheel(1, 8192 + 6000), 0);
            if (k == 70) ms.addEvent(juce::MidiMessage::pitchWheel(1, 8192), 0);
            bs.clear(); ps->processBlock(bs, ms);
            if (!isFiniteBuffer(bs)) { printf("FAIL: non-finite slew test\n"); return 1; }
            if (k >= 40)
                for (int i = 0; i < 512; ++i) mono.push_back(bs.getSample(0, i));
        }
        std::vector<double> xt;
        // lowpass ~1 kHz first: tanh adds odd harmonics that fool a naive
        // zero-crossing detector (extra crossings read as huge pitch jumps)
        {
            float gLP = 1.0f - std::exp(-2.0f * 3.14159265f * 1000.0f / 44100.0f);
            float y = 0.0f;
            for (size_t i = 0; i < mono.size(); ++i)
            {
                y += gLP * (mono[i] - y);
                mono[i] = y;
            }
        }
        for (size_t i = 1; i < mono.size(); ++i)
            if (mono[i - 1] <= 0.0f && mono[i] > 0.0f)
                xt.push_back((double)(i - 1) + (double)mono[i - 1] / ((double)mono[i - 1] - (double)mono[i]));
        float maxJump = 0.0f;
        for (size_t i = 2; i < xt.size(); ++i)
        {
            double p0 = xt[i - 1] - xt[i - 2], p1 = xt[i] - xt[i - 1];
            if (p0 < 20.0 || p1 < 20.0 || p0 > 800.0 || p1 > 800.0) continue; // sanity
            float j = std::abs(1200.0f * (float)std::log2(p0 / p1));
            if (j > maxJump) maxJump = j;
        }
        printf("pitch-slew max interval jump = %.1f cents\n", maxJump);
        if (maxJump > 60.0f) { printf("FAIL: pitch steps instead of slewing\n"); return 1; }
        ps->releaseResources();
    }
    juce::MemoryBlock mb;
    proc->getStateInformation(mb);
    CHECK(mb.getSize() > 0, "state non-empty");
    proc->setStateInformation(mb.getData(), (int)mb.getSize());
    printf("ok: state restored\n");

    proc->releaseResources();
    proc.reset();
    printf("ALL OK\n");
    return 0;
}
