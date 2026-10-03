#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <array>
#include <vector>

// Snapshot of all synth params, copied once per block (thread-safe)
struct SynthParams
{
    int vco1wave = 1, vco1range = 1;
    float vco1level = 0.8f, vco1pw = 0.5f, vco1pwm = 0.0f;
    int vco2wave = 0, vco2range = 1;
    float vco2level = 0.8f, vco2pw = 0.5f, vco2pwm = 0.0f, vco2tune = 0.0f;
    int u1 = 1, u2 = 1;                 // per-osc unison taps
    float mix1 = 0.92f, mix2 = 0.92f;   // osc balance gains (equal-power + makeup)
    float tapM1[4] = { 1, 1, 1, 1 };    // per-tap detune multipliers (from UIDET)
    float tapM2[4] = { 1, 1, 1, 1 };
    int tapN = 1;                       // max(u1,u2)
    bool sync = false; float xmod = 0.0f;
    int hpf = 0;
    float cutoff = 4000.0f, reso = 0.2f, fenv = 0.7f, flfo = 0.0f, keytrk = 0.5f;
    int slope = 1;
    float fa = 0.01f, fd = 0.2f, fs = 0.6f, fr = 0.3f;
    float aa = 0.01f, ad = 0.1f, as = 1.0f, ar = 0.2f;
    float lfoRate = 4.0f; int lfoWave = 0; float lfoDelay = 0.0f;
    float lfoVco1 = 0.0f, lfoVco2 = 0.0f, lfoVcf = 0.0f;
    int vcaMode = 1;
    float volume = 0.8f, tune = 0.0f;
    int bendRange = 2; float porta = 0.0f; int voiceMode = 0; float uidet = 0.15f, velsens = 0.5f;
    // FX
    bool chOn = true; int chMode = 1; float chRate = 0.5f, chDepth = 0.5f, chMix = 0.4f;
    bool phOn = false; float phRate = 0.5f, phDepth = 0.5f, phFb = 0.4f, phMix = 0.4f;
    bool dlOn = false; float dlTime = 350.0f, dlFb = 0.35f, dlMix = 0.25f; int dlSync = 0;
    bool rvOn = false; float rvSize = 0.5f, rvDamp = 0.5f, rvMix = 0.25f;
    bool stOn = false; float stAmt = 0.3f; int stMode = 0;
    float drive = 0.0f, eqLow = 0.0f, eqHigh = 0.0f;
    bool fxBypass = false;
    float pitchBend = 0.0f; // semitones, updated from MIDI
};

// ---------------- ADSR (exponential RC-style curves, like analog hardware) ----------------
class EnvADSR
{
public:
    void setSampleRate(double sr) { sampleRate = sr; }
    void setParams(float a, float d, float s, float r)
    {
        // seconds, with sensible minimums
        att = std::max(a, 0.001f); dec = std::max(d, 0.002f);
        sus = s; rel = std::max(r, 0.005f);
    }
    void noteOn()
    {
        if (state == State::Idle) level = 0.0f; // fresh start; retrigger glides from current
        state = State::Attack;
    }
    void noteOff() { if (state != State::Idle) state = State::Release; }
    void reset()   { state = State::Idle; level = 0.0f; }
    bool isActive() const { return state != State::Idle; }
    bool isReleasing() const { return state == State::Release; }

    inline float next()
    {
        switch (state)
        {
            case State::Idle: return 0.0f;
            case State::Attack: {
                float ka = 1.0f - std::exp(-3.0f / (att * (float)sampleRate));
                level += (1.0f - level) * ka;
                if ((1.0f - level) < 0.005f) state = State::Decay;
                return level;
            }
            case State::Decay: {
                float kd = 1.0f - std::exp(-3.0f / (dec * (float)sampleRate));
                level += (sus - level) * kd;
                if (std::fabs(level - sus) < 0.002f) { level = sus; state = State::Sustain; }
                return level;
            }
            case State::Sustain: level = sus; return sus;
            case State::Release: {
                float kr = std::exp(-3.0f / (rel * (float)sampleRate));
                level *= kr;
                if (level < 0.0006f) { level = 0.0f; state = State::Idle; return 0.0f; }
                return level;
            }
        }
        return 0.0f;
    }
private:
    enum class State { Idle, Attack, Decay, Sustain, Release };
    State state = State::Idle;
    double sampleRate = 44100.0;
    float att = 0.01f, dec = 0.2f, sus = 0.7f, rel = 0.3f, level = 0.0f;
};

inline float polyblep(double phase, double dt)
{
    if (phase < dt) { phase /= dt; return phase + phase - phase * phase - 1.0f; }
    if (phase > 1.0 - dt) { phase = (phase - 1.0) / dt; return phase * phase + phase + phase + 1.0f; }
    return 0.0f;
}

// gently rounded saw shoulders: keeps body, takes the digital edge off the top end
inline float roundSaw(float v) { return v * (1.25f - 0.25f * v * v); }

// wave: VCO1 0 tri 1 saw 2 pulse 3 square | VCO2 0 saw 1 pulse 2 square 3 sine 4 noise
inline float oscSample(int wave, double phase, float pw, double dt, float& noiseState, juce::Random& rng)
{
    switch (wave)
    {
        case 0: { // VCO1 triangle (naive, low aliasing risk at low freqs)
            return (float)(4.0 * std::abs(phase - 0.5) - 1.0);
        }
        case 1: { // saw w/ BLEP, gently rounded
            float v = (float)(2.0 * phase - 1.0);
            v -= (float)polyblep(phase, dt);
            return roundSaw(v);
        }
        case 2: { // pulse w/ BLEP
            float v = phase < pw ? 1.0f : -1.0f;
            v += (float)polyblep(phase, dt);
            double ph2 = phase - pw; if (ph2 < 0) ph2 += 1.0;
            v -= (float)polyblep(ph2, dt);
            return v * 0.75f;
        }
        case 3: { // square 50%
            float v = phase < 0.5 ? 1.0f : -1.0f;
            v += (float)polyblep(phase, dt);
            double ph2 = phase - 0.5; if (ph2 < 0) ph2 += 1.0;
            v -= (float)polyblep(ph2, dt);
            return v * 0.65f;
        }
        default: return 0.0f;
    }
}

inline float oscSampleVCO2(int wave, double phase, float pw, double dt, juce::Random& rng)
{
    switch (wave)
    {
        case 0: { float v = (float)(2.0 * phase - 1.0); return roundSaw(v - (float)polyblep(phase, dt)); }
        case 1: {
            float v = phase < pw ? 1.0f : -1.0f;
            v += (float)polyblep(phase, dt);
            double ph2 = phase - pw; if (ph2 < 0) ph2 += 1.0;
            v -= (float)polyblep(ph2, dt);
            return v * 0.75f;
        }
        case 2: {
            float v = phase < 0.5 ? 1.0f : -1.0f;
            v += (float)polyblep(phase, dt);
            double ph2 = phase - 0.5; if (ph2 < 0) ph2 += 1.0;
            v -= (float)polyblep(ph2, dt);
            return v * 0.65f;
        }
        case 3: return (float)std::sin(2.0 * juce::MathConstants<double>::pi * phase);
        case 4: return rng.nextFloat() * 2.0f - 1.0f;
        default: return 0.0f;
    }
}

inline double rangeMultiplier(int r) // octave switch: -2 -1 0 +1 +2
{
    switch (r) { case 0: return 0.25; case 1: return 0.5; case 2: return 1.0; case 3: return 2.0; default: return 4.0; }
}
inline float midiToFreq(float midi) { return 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f); }

// ---------------- Voice filter: cascaded TPT-SVF lowpass, 12/24 dB ----------------
// Per-sample Zavalishin SVF (stable, cheap) — two cascaded 12 dB stages for 24 dB mode.
struct VoiceFilter
{
    void reset() { ic1a = ic2a = ic1b = ic2b = 0.0f; }
    inline float process(float x, float cutoffHz, float reso01, double sr, int slope24)
    {
        float fc = juce::jlimit(30.0f, 19000.0f, cutoffHz);
        float g = std::tan(juce::MathConstants<float>::pi * fc / (float)sr);
        g = juce::jmin(g, 8.0f);
        // gradual resonance: gentle at low knob, singing (not screaming) at max
        float r = juce::jlimit(0.0f, 0.98f, reso01);
        float Q = 0.5f + r * r * 5.0f; // 0.5 .. ~5.3
        float k = 1.0f / Q;
        float a1 = 1.0f / (1.0f + g * (g + k));
        float a2 = g * a1;
        float a3 = g * a2;
        // stage A
        float v3 = x - ic2a;
        float v1 = a1 * ic1a + a2 * v3;
        float v2 = ic2a + a2 * ic1a + a3 * v3;
        ic1a = 2.0f * v1 - ic1a;
        ic2a = 2.0f * v2 - ic2a;
        if (!slope24) return v2;
        // stage B (share coefficients)
        float w3 = v2 - ic2b;
        float w1 = a1 * ic1b + a2 * w3;
        float w2 = ic2b + a2 * ic1b + a3 * w3;
        ic1b = 2.0f * w1 - ic1b;
        ic2b = 2.0f * w2 - ic2b;
        return w2;
    }
    float ic1a = 0.0f, ic2a = 0.0f, ic1b = 0.0f, ic2b = 0.0f;
};

// ---------------- Global LFO ----------------
class GlobalLFO
{
public:
    void setSampleRate(double sr) { sampleRate = sr; }
    void reset() { phase = 0.0; snhValue = 0.0f; delayCount = 0; }
    // returns -1..+1 with delay fade applied
    float next(float rate, int wave, float delaySec, juce::Random& rng)
    {
        double dt = rate / sampleRate;
        phase += dt; if (phase >= 1.0) { phase -= 1.0; snhValue = rng.nextFloat() * 2.0f - 1.0f; }
        float raw = 0.0f;
        switch (wave)
        {
            case 0: raw = (float)(4.0 * std::abs(phase - 0.5) - 1.0); break;              // tri
            case 1: raw = (float)std::sin(2.0 * juce::MathConstants<double>::pi * phase); break; // sine
            case 2: raw = (float)(2.0 * phase - 1.0); break;                              // saw
            case 3: raw = phase < 0.5 ? 1.0f : -1.0f; break;                               // square
            case 4: raw = snhValue; break;                                                // S&H
            default: raw = 0.0f;
        }
        float fade = 1.0f;
        if (delaySec > 0.001f)
        {
            delayCount++;
            float total = delaySec * (float)sampleRate;
            fade = juce::jlimit(0.0f, 1.0f, delayCount / total);
        }
        return raw * fade;
    }
    void noteOn() { if (true) { /* keep free-running; reset delay */ delayCount = 0; } }
private:
    double sampleRate = 44100.0, phase = 0.0;
    float snhValue = 0.0f;
    long delayCount = 0;
};

// ---------------- Voice ----------------
class SynthVoice
{
public:
    bool active = false;
    int note = -1;
    float velocity = 0.8f;
    int age = 0;

    void prepare(double sr)
    {
        sampleRate = sr;
        fenv.setSampleRate(sr); aenv.setSampleRate(sr);
        vcf.reset();
        hpf.prepare({ sr, 8, 1 });
        rng.setSeedRandomly();
        driftPh = rng.nextFloat();
        // 1-pole warmth lowpass (~12 kHz)
        warmG = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * 12000.0f / (float)sr);
        warmY = 0.0f;
    }

    // glide: keep current pitch/phases and slide to the new note (legato/portamento)
    // retrigger: restart filter+amp envelopes (detached/staccato articulation)
    void noteOn(int midiNote, float vel, const SynthParams& p, bool glide, bool retrigger)
    {
        bool wasActive = active; // live voice = keep everything running (analog-style)
        note = midiNote; velocity = vel; active = true; gated = true;
        float target = (float)midiNote + p.pitchBend + p.tune / 100.0f;
        if (!glide)
        {
            curMidi1 = target; curMidi2 = target;
            targetMidi = target;
            if (!wasActive)
            {
                // true fresh attack: new circuit state + fade in.
                // Tolerance is subtle; slow beating comes from the dual
                // detuned oscillators, not from wobble.
                slopDetune = (rng.nextFloat() * 2.0f - 1.0f) * 3.0f;
                slopCut    = 1.0f + (rng.nextFloat() * 2.0f - 1.0f) * 0.03f;
                driftRate  = 0.05f + rng.nextFloat() * 0.25f;
                // every unison tap gets an independent free-running phase
                for (int u = 0; u < 4; ++u)
                {
                    uph1[u] = rng.nextFloat();
                    uph2[u] = p.sync ? 0.0 : rng.nextFloat();
                    tapG1[u] = (u < p.u1) ? 1.0f : 0.0f;
                    tapG2[u] = (u < p.u2) ? 1.0f : 0.0f;
                }
                subPh = rng.nextFloat();
                vcf.reset();
                clickCount = 0;
            }
            // else: retrigger on a live voice — oscs/filter keep running, pitch
            // snaps, envelopes restart from current level: no dip, no thump
        }
        else
        {
            targetMidi = target; // slide from current pitch, oscs keep running
        }
        fenv.setParams(p.fa, p.fd, p.fs, p.fr);
        aenv.setParams(p.aa, p.ad, p.as, p.ar);
        if (retrigger) { fenv.noteOn(); aenv.noteOn(); }
    }

    void noteOff() { gated = false; fenv.noteOff(); aenv.noteOff(); }
    void kill() { active = false; fenv.reset(); aenv.reset(); }
    bool isReleasing() const { return !gated; }
    bool envDone() const { return !fenv.isActive() && !aenv.isActive(); }
    float ampLevel() const { return lastAmp; } // current amp-env level, for steal decisions
    float lastOut() const { return lastSample; } // last rendered sample, for steal crossfades

    float render(const SynthParams& p, float lfo, float& outVcoMix)
    {
        // glide
        if (p.porta > 0.001f)
        {
            float c = 1.0f - (float)std::exp(-1.0 / (sampleRate * (0.002 + p.porta * 0.4)));
            curMidi1 += (targetMidi - curMidi1) * c;
            curMidi2 += (targetMidi - curMidi2) * c;
        }
        else { curMidi1 = targetMidi; curMidi2 = targetMidi; }

        float fenvV = fenv.next();
        float aenvV = aenv.next();
        if (!fenv.isActive() && !aenv.isActive() && !gated)
        {
            active = false; lastSample = 0.0f; return 0.0f;
        }

        float pw1 = juce::jlimit(0.05f, 0.95f, p.vco1pw + p.vco1pwm * 0.4f * lfo);
        float pw2 = juce::jlimit(0.05f, 0.95f, p.vco2pw + p.vco2pwm * 0.4f * lfo);

        float lfoSemi1 = p.lfoVco1 * lfo * 1.0f;   // +/-1 semitone max
        float lfoSemi2 = p.lfoVco2 * lfo * 1.0f;

        // slow per-voice pitch drift on top of the static tolerance.
        // Kept to a whisper: movement comes from dual-osc beating instead.
        driftPh += (double)driftRate / sampleRate;
        if (driftPh >= 1.0) driftPh -= 1.0;
        float driftCents = std::sin(2.0f * juce::MathConstants<float>::pi * (float)driftPh) * 0.5f;
        float det1 = (slopDetune + driftCents) / 100.0f;        // semitones, osc 1
        float det2 = (slopDetune * 0.8f + driftCents * 0.6f + 4.0f) / 100.0f; // osc 2 beats gently

        double m1 = rangeMultiplier(p.vco1range);
        double m2 = rangeMultiplier(p.vco2range);

        float v1 = midiToFreq(curMidi1 + lfoSemi1) * (float)m1;
        float v2base = midiToFreq(curMidi2 + lfoSemi2 + p.vco2tune / 100.0f) * (float)m2;
        // exact pow only for the (small, drifting) base detune; taps use cheap mults
        // NOTE: det1/det2 are in semitones -> /12 for octave ratio (12x bug if omitted!)
        float base1 = (float)std::pow(2.0, det1 / 12.0);
        float base2 = (float)std::pow(2.0, det2 / 12.0);

        // per-osc unison: each osc renders its own tap count (VCO2 -> VCO1 cross-mod)
        // NOTE: every tap owns an independent phase accumulator — sharing one
        // would advance it N times per sample and shift pitch up N times!
        // Tap gains glide (~5 ms) so changing unison live never steps.
        float sum1 = 0.0f, sum2 = 0.0f, norm1 = 0.0f, norm2 = 0.0f;
        float dummy = 0.0f;
        float kTap = 1.0f - std::exp(-1.0f / (0.005f * (float)sampleRate));
        for (int u = 0; u < p.tapN; ++u)
        {
            float t1 = (u < p.u1) ? 1.0f : 0.0f;
            float t2 = (u < p.u2) ? 1.0f : 0.0f;
            tapG1[u] += (t1 - tapG1[u]) * kTap;
            tapG2[u] += (t2 - tapG2[u]) * kTap;
            norm1 += tapG1[u]; norm2 += tapG2[u];
            double f2u = (double)v2base * (double)base2 * (double)p.tapM2[u];
            double dt2 = f2u / sampleRate;
            uph2[u] += dt2; if (uph2[u] >= 1.0) uph2[u] -= 1.0;
            float s2 = (u < p.u2) ? oscSampleVCO2(p.vco2wave, uph2[u], pw2, dt2, rng) : 0.0f;
            // XMOD: VCO1 frequency modulated by VCO2 output
            double f1u = (double)v1 * (double)base1 * (double)p.tapM1[u];
            double f1m = f1u;
            if (p.xmod > 0.001f)
                f1m = std::max(1.0, f1u * (1.0 + p.xmod * 2.5 * s2));
            double dt1m = f1m / sampleRate;
            uph1[u] += dt1m;
            if (uph1[u] >= 1.0) { uph1[u] -= 1.0; if (p.sync) uph2[u] = 0.0; }
            float s1 = (u < p.u1) ? oscSample(p.vco1wave, uph1[u], pw1, dt1m, dummy, rng) : 0.0f;

            sum1 += s1 * tapG1[u]; sum2 += s2 * tapG2[u];
        }
        // normalize by live tap gains: level stays constant while taps morph
        float osc = (sum1 / juce::jmax(1.0f, norm1)) * p.vco1level * p.mix1
                  + (sum2 / juce::jmax(1.0f, norm2)) * p.vco2level * p.mix2;
        osc *= 0.62f;
        // sub sine, -1 octave under VCO1 (follows range + drift) — weight without mud.
        // fully muted below 20 Hz: infrasonic subs eat headroom and pump
        // compressors/speakers without being heard.
        subPh += (double)(v1 * 0.5f) / sampleRate;
        if (subPh >= 1.0) subPh -= 1.0;
        float subFreq = v1 * 0.5f;
        float subAud = juce::jlimit(0.0f, 1.0f, (subFreq - 20.0f) / 10.0f);
        osc += std::sin(2.0 * juce::MathConstants<double>::pi * subPh)
               * p.vco1level * p.mix1 * 0.32f * subAud;
        outVcoMix = osc;

        // HPF (fixed steps) — one-pole HP approx
        float hpfCut = 0.0f;
        if (p.hpf == 1) hpfCut = 120.0f; else if (p.hpf == 2) hpfCut = 350.0f; else if (p.hpf == 3) hpfCut = 800.0f;
        if (hpfCut > 0.0f)
        {
            float rc = 1.0f / (2.0f * (float)juce::MathConstants<double>::pi * hpfCut);
            float alpha = rc / (rc + 1.0f / (float)sampleRate);
            hpfY = alpha * (hpfY + osc - hpfX); hpfX = osc;
            osc = hpfY;
        }

        // VCF cutoff w/ env, lfo, keytrack, velocity
        float envAmt = p.fenv * (1.0f + p.velsens * (velocity - 0.7f));
        float cutoffHz = p.cutoff
            * std::pow(2.0f, envAmt * 6.0f * fenvV)
            * std::pow(2.0f, p.flfo * 3.0f * lfo)
            * std::pow(2.0f, p.keytrk * ((float)note - 60.0f) / 12.0f);
        cutoffHz = juce::jlimit(30.0f, 19000.0f, cutoffHz * slopCut);

        float filtered = vcf.process(osc, cutoffHz, p.reso, sampleRate, p.slope == 1);
        // gentle post-filter saturation: creamy glue, keeps the low end tight
        float drv = 1.0f + p.reso * 1.2f;
        float y = filtered * drv;
        filtered = y / (1.0f + 0.35f * std::fabs(y));
        // resonance loudness compensation (peaks, not harshness)
        filtered *= 1.0f / (1.0f + p.reso * 1.6f);

        // analog top-end rolloff: softens digital edge above ~12 kHz
        warmY += warmG * (filtered - warmY);
        filtered = warmY;

        float amp = (p.vcaMode == 1) ? aenvV : 1.0f;
        float ampVel = (1.0f - 0.7f * p.velsens) + 0.7f * p.velsens * velocity;
        lastAmp = aenvV;
        // ~2 ms raised-cosine fade on fresh attacks: kills phase-discontinuity pops
        float clickG = 1.0f;
        if (clickCount < 96)
        {
            clickG = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::pi * (float)clickCount / 96.0f);
            ++clickCount;
        }
        lastSample = filtered * amp * ampVel * clickG;
        return lastSample;
    }

    void setTarget(float midiWithBend) { targetMidi = midiWithBend; }

    double sampleRate = 44100.0;
    VoiceFilter vcf;
    juce::dsp::FirstOrderTPTFilter<float> hpf;
private:
    int age2 = 0;
    bool gated = true;
    EnvADSR fenv, aenv;
    double uph1[4] = { 0.0, 0.0, 0.0, 0.0 }; // independent phase per unison tap
    double uph2[4] = { 0.0, 0.0, 0.0, 0.0 };
    float curMidi1 = 60.0f, curMidi2 = 60.0f, targetMidi = 60.0f;
    float hpfX = 0.0f, hpfY = 0.0f;
    // analog slop: static per-note tolerance + slow drift
    float slopDetune = 0.0f, slopCut = 1.0f, driftRate = 0.1f;
    double driftPh = 0.0;
    float warmY = 0.0f, warmG = 0.5f;
    double subPh = 0.0;
    int clickCount = 96; // de-click fade progress (>=96 = idle)
    float lastAmp = 0.0f;
    float lastSample = 0.0f; // last rendered output (for steal crossfades)
    float tapG1[4] = { 1.0f, 1.0f, 1.0f, 1.0f }; // smoothed unison tap gains
    float tapG2[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    juce::Random rng;
};

// ---------------- FX ----------------
class ChorusFX
{
public:
    void prepare(double sr, int maxBlock)
    {
        sampleRate = sr;
        int maxLen = (int)(sr * 0.05) + 16;
        bufL.assign(maxLen, 0.0f); bufR.assign(maxLen, 0.0f);
        len = maxLen; pos = 0;
        lfoPhase = 0.0;
        wlpL = wlpR = 0.0f;
        smBase = 12.0f; smDepth = 3.0f; smRate = 0.6f;
        juce::ignoreUnused(maxBlock);
    }
    void process(float* L, float* R, int n, float rate, float depth, float mix, int mode)
    {
        if (mix <= 0.001f) return;
        float baseMs = 8.0f, depthMs = 4.0f * depth; // Manual: subtle, minimal comb color
        bool quad = false;
        if (mode == 1)      { baseMs = 14.0f; depthMs = 3.0f; rate = 0.45f; } // Vintage I
        else if (mode == 2) { baseMs = 9.0f;  depthMs = 5.0f; rate = 0.85f; } // Vintage II
        else if (mode == 3) { baseMs = 14.0f; depthMs = 7.0f * depth; quad = true; } // Wide: quadrature
        // glide delay parameters (~30 ms): mode/knob jumps morph instead of zapping
        float k = 1.0f - std::exp(-1.0f / (0.030f * (float)sampleRate));
        // (smoothing applied per-sample below via smBase/smDepth/smRate)
        // gentle BBD-style darkening on the wet path only
        float wg = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * 7000.0f / (float)sampleRate);
        for (int i = 0; i < n; ++i)
        {
            smBase += (baseMs - smBase) * k;
            smDepth += (depthMs - smDepth) * k;
            smRate += (rate - smRate) * k;
            lfoPhase += smRate / sampleRate; if (lfoPhase >= 1.0) lfoPhase -= 1.0;
            float ph = (float)lfoPhase * 2.0f * (float)juce::MathConstants<double>::pi;
            float lfoL = std::sin(ph);
            float lfoR = quad ? std::cos(ph) : -lfoL; // quadrature spreads wide, antiphase stays classic
            bufL[pos] = L[i]; bufR[pos] = R[i];
            float wetL = readDelay(bufL, smBase + smDepth * lfoL);
            float wetR = readDelay(bufR, smBase + smDepth * lfoR);
            wlpL += wg * (wetL - wlpL); wlpR += wg * (wetR - wlpR);
            L[i] = L[i] * (1.0f - mix * 0.5f) + wlpL * mix * 0.5f;
            R[i] = R[i] * (1.0f - mix * 0.5f) + wlpR * mix * 0.5f;
            pos = (pos + 1) % len;
        }
    }
private:
    float readDelay(const std::vector<float>& buf, float ms)
    {
        float samples = (float)(ms * sampleRate / 1000.0);
        float rp = (float)pos - samples;
        while (rp < 0) rp += len;
        int i0 = (int)rp; float frac = rp - i0;
        int i1 = (i0 + 1) % len;
        return buf[(size_t)i0] * (1 - frac) + buf[(size_t)i1] * frac;
    }
    double sampleRate = 44100.0; std::vector<float> bufL, bufR; int len = 1024, pos = 0; double lfoPhase = 0.0;
    float wlpL = 0.0f, wlpR = 0.0f; // wet-path darkening state
    float smBase = 12.0f, smDepth = 3.0f, smRate = 0.6f; // gliding delay params
public:
    void clear()
    {
        std::fill(bufL.begin(), bufL.end(), 0.0f);
        std::fill(bufR.begin(), bufR.end(), 0.0f);
        wlpL = wlpR = 0.0f;
    }
};

class PhaserFX
{
public:
    void prepare(double sr) { sampleRate = sr; phase = 0.0; std::fill(stage, stage + 6, 0.0f); std::fill(stageL, stageL + 6, 0.0f); }
    void clear() { std::fill(stage, stage + 6, 0.0f); std::fill(stageL, stageL + 6, 0.0f); fbL = fbR = 0.0f; }
    inline float allpass(float x, float& s, float g) { float y = -g * x + s; s = x + g * y; return y; }
    void process(float* L, float* R, int n, float rate, float depth, float fb, float mix)
    {
        if (mix <= 0.001f) return;
        for (int i = 0; i < n; ++i)
        {
            phase += rate / sampleRate; if (phase >= 1.0) phase -= 1.0;
            float lfo = std::sin(2.0f * (float)juce::MathConstants<double>::pi * (float)phase);
            float g = juce::jlimit(0.05f, 0.95f, 0.5f + depth * 0.42f * lfo);
            float inL = L[i] + fbL * fb * 0.8f;
            float inR = R[i] + fbR * fb * 0.8f;
            float oL = inL, oR = inR;
            for (int s = 0; s < 6; ++s) { oL = allpass(oL, stage[s], g); oR = allpass(oR, stageL[s], g); }
            fbL = oL; fbR = oR;
            L[i] = L[i] * (1 - mix) + oL * mix;
            R[i] = R[i] * (1 - mix) + oR * mix;
        }
    }
private:
    double sampleRate = 44100.0, phase = 0.0;
    float stage[6] = {}, stageL[6] = {};
    float fbL = 0.0f, fbR = 0.0f;
};

// ---------------- ModVerb: modulated Schroeder reverb ----------------
// 8 modulated comb filters (stereo pairs) into 2+2 allpasses.
// Slow LFO on each comb's delay time smears resonant modes -> lush, not metallic.
class ModVerb
{
public:
    void prepare(double sr)
    {
        sampleRate = sr;
        // long, spread delay ratios (ms) for a lush tail; R side offset
        const float combMs[4] = { 47.0f, 53.0f, 44.0f, 58.0f };
        for (int i = 0; i < 4; ++i)
        {
            int maxLen = (int)(sr * 0.13) + 64;
            cl[i].buf.assign((size_t)maxLen, 0.0f);
            cr[i].buf.assign((size_t)maxLen, 0.0f);
            cl[i].len = maxLen; cr[i].len = maxLen;
            cl[i].base = combMs[i] * (float)sr / 1000.0f;
            cr[i].base = (combMs[i] + 1.1f) * (float)sr / 1000.0f;
            cl[i].pos = cr[i].pos = 0; cl[i].lp = cr[i].lp = 0.0f;
        }
        const float apMs[4] = { 5.0f, 1.7f, 3.4f, 2.6f };
        const float apG[4] = { 0.7f, 0.6f, 0.65f, 0.6f };
        for (int i = 0; i < 4; ++i)
        {
            int maxLen = (int)(sr * 0.01) + 16;
            al[i].buf.assign((size_t)maxLen, 0.0f);
            ar[i].buf.assign((size_t)maxLen, 0.0f);
            al[i].len = ar[i].len = maxLen;
            al[i].d = apMs[i] * (float)sr / 1000.0f;
            ar[i].d = (apMs[i] + 0.4f) * (float)sr / 1000.0f;
            al[i].pos = ar[i].pos = 0; al[i].g = ar[i].g = apG[i];
        }
        // size-scaled predelay (0..25 ms) keeps dry attacks clean on huge tails
        pd.buf.assign((size_t)((int)(sr * 0.06) + 16), 0.0f);
        pd.len = (int)pd.buf.size(); pd.pos = 0;
    }

    void process(float* L, float* R, int n, float size, float damp, float mix)
    {
        if (mix <= 0.001f) return;
        size = juce::jlimit(0.0f, 1.0f, size);
        float fb = 0.70f + size * 0.25f;                    // 0.70..0.95, LP keeps it stable
        float lpC = 0.55f * (1.0f - juce::jlimit(0.0f, 1.0f, damp)) + 0.05f; // darker tail w/ damp
        float lenScale = 0.75f + size * 0.6f;               // longer chambers when big
        float kLen = 1.0f - std::exp(-1.0f / (0.050f * (float)sampleRate));
        float modAmp = (float)((0.0006 + size * 0.002) * sampleRate); // deeper wobble when big
        const float rates[4] = { 0.11f, 0.19f, 0.27f, 0.23f };
        float wetG = (0.5f + size * 0.7f);
        float preD = size * 0.025f * (float)sampleRate;     // 0..25 ms predelay

        for (int i = 0; i < n; ++i)
        {
            curLenScale += (lenScale - curLenScale) * kLen;
            float dry = (L[i] + R[i]) * 0.5f;
            // predelay (interpolated)
            pd.buf[(size_t)pd.pos] = dry;
            float rp = (float)pd.pos - preD;
            while (rp < 0.0f) rp += (float)pd.len;
            int pi0 = (int)rp % pd.len; if (pi0 < 0) pi0 += pd.len;
            int pi1 = (pi0 + 1) % pd.len;
            float pf = rp - std::floor(rp);
            float in = pd.buf[(size_t)pi0] * (1.0f - pf) + pd.buf[(size_t)pi1] * pf;
            pd.pos = (pd.pos + 1) % pd.len;
            float accL = 0.0f, accR = 0.0f;
            for (int c = 0; c < 4; ++c)
            {
                mlfo[c] += rates[c] / sampleRate;
                if (mlfo[c] >= 1.0) mlfo[c] -= 1.0;
                float mod = std::sin(2.0f * juce::MathConstants<float>::pi * (float)mlfo[c]) * modAmp;
                accL += runComb(cl[c], in, fb, lpC, mod, curLenScale);
                accR += runComb(cr[c], in, fb, lpC, -mod, curLenScale);
            }
            accL *= 0.25f; accR *= 0.25f;
            float wL = accL, wR = accR;
            for (int a = 0; a < 4; ++a) { wL = runAP(wL, al[a]); wR = runAP(wR, ar[a]); }
            L[i] = L[i] * (1.0f - mix) + wL * wetG * mix;
            R[i] = R[i] * (1.0f - mix) + wR * wetG * mix;
        }
    }
    void clear()
    {
        for (auto* c : { cl, cr }) for (int i = 0; i < 4; ++i)
        { std::fill(c[i].buf.begin(), c[i].buf.end(), 0.0f); c[i].lp = 0.0f; }
        for (auto* a : { al, ar }) for (int i = 0; i < 4; ++i)
            std::fill(a[i].buf.begin(), a[i].buf.end(), 0.0f);
        std::fill(pd.buf.begin(), pd.buf.end(), 0.0f);
    }

private:
    struct Comb { std::vector<float> buf; int len = 1, pos = 0; float base = 0.0f, lp = 0.0f; };
    struct AP { std::vector<float> buf; int len = 1, pos = 0; float d = 0.0f, g = 0.65f; };
    struct PD { std::vector<float> buf; int len = 1, pos = 0; };
    Comb cl[4], cr[4];
    AP al[4], ar[4];
    PD pd;
    float curLenScale = 1.0f;
    double sampleRate = 44100.0, mlfo[4] = { 0.0, 0.25, 0.5, 0.75 };

    inline float readInterp(const std::vector<float>& buf, int len, float delaySamp, int writePos)
    {
        float rp = (float)writePos - delaySamp;
        while (rp < 0.0f) rp += (float)len;
        int i0 = (int)rp % len; if (i0 < 0) i0 += len;
        int i1 = (i0 + 1) % len;
        float frac = rp - std::floor(rp);
        return buf[(size_t)i0] * (1.0f - frac) + buf[(size_t)i1] * frac;
    }
    inline float runComb(Comb& c, float in, float fb, float lpC, float mod, float lenScale)
    {
        float d = juce::jlimit(4.0f, (float)c.len - 2.0f, c.base * lenScale + mod);
        float y = readInterp(c.buf, c.len, d, c.pos);
        c.lp += lpC * (y - c.lp);
        c.buf[(size_t)c.pos] = in + c.lp * fb;
        c.pos = (c.pos + 1) % c.len;
        return y;
    }
    inline float runAP(float x, AP& a)
    {
        float g = a.g;
        int rp = a.pos - (int)a.d;
        while (rp < 0) rp += a.len;
        rp %= a.len;
        float bufOut = a.buf[(size_t)rp];
        float y = -g * x + bufOut;
        a.buf[(size_t)a.pos] = x + g * y;
        a.pos = (a.pos + 1) % a.len;
        return y;
    }
};
// ---------------- SatFX: master saturator (tape / tube) ----------------
// Gentle harmonic glue after reverb: drive -> shape (+even harmonics) ->
// DC block -> tone lowpass -> dry/wet mix.
class SatFX
{
public:
    void prepare(double sr) { sampleRate = sr; reset(); }
    void reset() { dcL = dcR = lpL = lpR = 0.0f; }

    void process(float* L, float* R, int n, float amt, float tone, float mix, int mode)
    {
        if (amt <= 0.001f || mix <= 0.001f) return;
        amt = juce::jlimit(0.0f, 1.0f, amt);
        float d = 1.0f + amt * 9.0f;
        float fc = 2000.0f * std::pow(9.0f, juce::jlimit(0.0f, 1.0f, tone)); // 2k..18k
        float g = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * fc / (float)sampleRate);
        float dcC = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * 5.0f / (float)sampleRate);
        float post = 1.0f / (1.0f + amt * 1.5f);
        float even = (mode == 1) ? 0.18f : 0.06f;
        for (int i = 0; i < n; ++i)
        {
            float wetL = shape(L[i] * d, mode, even);
            float wetR = shape(R[i] * d, mode, even);
            // DC block (removes offset from even harmonics)
            dcL += dcC * (wetL - dcL); wetL -= dcL;
            dcR += dcC * (wetR - dcR); wetR -= dcR;
            // tone lowpass
            lpL += g * (wetL - lpL); lpR += g * (wetR - lpR);
            L[i] = L[i] * (1.0f - mix) + lpL * post * mix;
            R[i] = R[i] * (1.0f - mix) + lpR * post * mix;
        }
    }
private:
    inline float shape(float x, int mode, float even)
    {
        float s = (mode == 1) ? x / (1.0f + 0.5f * std::fabs(x))   // tube: open
                              : std::tanh(x);                      // tape: soft
        return s + even * s * s;
    }
    double sampleRate = 44100.0;
    float dcL = 0.0f, dcR = 0.0f, lpL = 0.0f, lpR = 0.0f;
};
class DelayFX
{
public:
    void prepare(double sr)
    {
        sampleRate = sr;
        int maxLen = (int)(sr * 2.0) + 16;
        bufL.assign(maxLen, 0.0f); bufR.assign(maxLen, 0.0f);
        len = maxLen; pos = 0; lpL = lpR = 0.0f;
    }
    void process(float* L, float* R, int n, float timeMs, float fb, float mix)
    {
        if (mix <= 0.001f) return;
        timeMs = juce::jlimit(20.0f, 1500.0f, timeMs);
        float samples = (float)(timeMs * sampleRate / 1000.0);
        for (int i = 0; i < n; ++i)
        {
            float rp = (float)pos - samples;
            while (rp < 0) rp += len;
            int i0 = (int)rp; float frac = rp - i0; int i1 = (i0 + 1) % len;
            float dL = bufL[(size_t)i0] * (1 - frac) + bufL[(size_t)i1] * frac;
            float dR = bufR[(size_t)i0] * (1 - frac) + bufR[(size_t)i1] * frac;
            // damping
            lpL += 0.25f * (dL - lpL); lpR += 0.25f * (dR - lpR);
            bufL[pos] = L[i] + lpL * fb;
            bufR[pos] = R[i] + lpR * fb;
            L[i] = L[i] * (1 - mix) + dL * mix;
            R[i] = R[i] * (1 - mix) + dR * mix;
            pos = (pos + 1) % len;
        }
    }
    void clear() { std::fill(bufL.begin(), bufL.end(), 0.0f); std::fill(bufR.begin(), bufR.end(), 0.0f); }
private:
    double sampleRate = 44100.0; std::vector<float> bufL, bufR; int len = 1024, pos = 0; float lpL = 0, lpR = 0;
};
