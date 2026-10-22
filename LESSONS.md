# LESSONS — every issue hit building ERSA, and what fixed it

Companion to `CHECKS.md` (the pre-flight list). This file records *what
happened, why, and the fix*, so future plugins don't re-learn it the hard way.
Ordered roughly chronologically.

## 1. Build breaks against JUCE 8 API

**Issue:** Fresh JUCE 8 fetch broke the first build in five places at once:
`LadderFilter::processSample` is protected, `drawGroupComponent` was renamed
to `drawGroupComponentOutline`, `IIR::Filter::state` is private (use the
`coefficients` member), `dsp::Reverb` has no `setSampleRate` (needs
`prepare(spec)`), `Font::getStringWidthFloat` is deprecated.
**Fix:** Replaced the ladder filter with a hand-rolled cascaded TPT-SVF
(also gave us the gradual resonance curve we wanted), used the correct
override signatures, assigned coefficients objects.
**Avoid:** After any JUCE version bump, build immediately and fix override
signatures first — mismatched `override`s either fail loudly (good) or
silently change behavior (bad).

## 2. Instant DAW crash on plugin load (segfault in `setBounds`)

**Issue:** Every DAW crashed scanning the plugin.
**Root cause:** The editor constructor called `setSize()` first, which
synchronously fires `resized()`, which touched the MIDI keyboard member that
wasn't constructed until later in the constructor. Null dereference.
**Fix:** Moved `setSize()` to the end of the constructor, null-guarded
`resized()`. Reproduced headlessly and confirmed with a gdb backtrace
(`Component::setBounds ← sendMovedResizedMessages ← Ersa8Editor`).
**Avoid:** Never trigger layout before all members exist; null-guard
everything `resized()` touches.

## 3. Non-finite (NaN) audio from `getSampleRate() == 0`

**Issue:** First render produced NaN.
**Root cause:** EQ coefficient code called `getSampleRate()`, which returns 0
until the plugin wrapper calls `setRateAndBufferSizeDetails` — the harness
(and some hosts) only called `prepareToPlay`.
**Fix:** Cache the rate from `prepareToPlay` into `cachedSr`; never trust
`getSampleRate()` in DSP code. Harness replicates the wrapper call.
**Avoid:** Cache the sample rate yourself; treat `getSampleRate()` as
unreliable outside the wrapper path.

## 4. Presets leaked into each other

**Issue:** Loading preset 0 after preset 27 kept 27's +4 dB EQ.
**Root cause:** Factory presets were sparse override maps with no reset, and
JUCE 8 `replaceState()` swaps the tree without pushing values to parameters.
**Fix:** Every program load resets all params to creation defaults first,
then applies overrides. Covered by a determinism test.
**Avoid:** Programs must always be reset-then-apply, never overlay-only.

## 5. Mono/legato staccato went silent (the "Acid" bug)

**Issue:** Detached notes on mono patches smeared into silence.
**Root cause:** Any new note arriving while the voice was releasing was
treated as legato, so envelopes never retriggered.
**Fix:** Split *glide* from *re-articulation* into independent flags:
detached notes always restart filter+amp (pitch may still glide with
portamento), true overlapping legato slurs.
**Avoid:** Never conflate "voice is busy" with "player is legatoing" —
track physical held-keys separately.

## 6. ComboBox preset names rendered twice (the "overlap")

**Issue:** Every dropdown name looked smeared/overlapped, permanently.
**Root cause:** JUCE's ComboBox paints its own internal text label, and our
custom look-and-feel *also* drew the text underneath it.
**Fix:** Draw only background + arrow in `drawComboBox`; style the text via
`textColourId` + `getComboBoxFont`.
**Avoid:** Know which parts JUCE paints itself before custom-painting a widget.

## 7. Unison shifted pitch up N octaves

**Issue:** Turning up unison raised the pitch (2 taps = octave up).
**Root cause:** All unison taps shared one phase accumulator, so each tap
advanced it again per sample — N taps ran the oscillator N× fast.
**Fix:** Independent phase accumulator per tap. Regression test proves 4×
unison still reads concert pitch.
**Avoid:** Parallel voices need independent state. Shared mutable phase is
always a pitch bug.

## 8. The 12× detune bug (`pow(2, det)` vs `pow(2, det/12)`)

**Issue:** After the unison rewrite "nothing was in tune, unusable."
**Root cause:** Slop moved from "added to MIDI note" (semitones, correct) to
a frequency multiplier `pow(2, det)` — but that function takes *octaves*, so
every ±4¢ tolerance hit 12× too hard (±48¢+ random per note).
**Fix:** One-line `/12`. Proven with an FFT tuning probe (VCO2 read 452 Hz
instead of 440 pre-fix; 0–8¢ everywhere post-fix).
**Avoid:** Unit-test tuning with an FFT probe after touching any pitch path.
Keep semitone/octave conversions in one place with the units in the name.

## 9. Footage-to-octave migration cascade

**Issue:** A script migrating `16'/8'/4'` → `-2..+2` indices corrupted every
preset.
**Root cause:** Sequential find-replace of overlapping values (0→1 rewrites
what 1→2 then rewrites again).
**Fix:** Regenerated from an index-based plan instead of string replacement.
**Avoid:** Never sequential-replace overlapping values; always map old→new
in one pass from a table.

## 10. Sub-20 Hz rumble on the analyzer

**Issue:** Huge sub-20 Hz energy (mostly <10 Hz) on bass patches.
**Root cause:** The sub oscillator itself went infrasonic on low notes (a
16 Hz sine on low C) — inaudible, but eats headroom and pumps the bus.
**Fix:** Sub fades out fully below 20 Hz + a steep 20 Hz cleanup filter on
the master. Verified −55 dB in the 8–20 Hz band with zero DC.
**Avoid:** Every synth needs a master highpass and a spectrum test; subs must
be gated to the audible range.

## 11. Steal clicks under busy playing

**Issue:** Clicks during fast legato/chord playing despite clean sustains.
**Root cause:** Voice stealing swapped waveforms instantly (verified with a
"steal torture" test hitting 0.42 steps).
**Fix:** Steal the quietest voice + 3 ms crossfade from its frozen output.
Also: fresh-attack fade-in, quietest-voice stealing, LFO edge slew.
**Avoid:** Sustained-note tests are not enough; always torture-test stealing
and live parameter drags.

## 12. JUCE 8 stores denormalized values in APVTS trees

**Issue:** User-preset load set absurd values (eqhigh read 12.0 instead of 4.0).
**Root cause:** Assumed tree `value`s were normalized 0–1; JUCE 8 stores real
units — must `convertTo0to1()` on load.
**Fix:** Convert on load; regression test asserts round-trip values.
**Avoid:** Never assume normalization direction; assert a round-trip value in
tests, not just "applied > 0".

## 13. Pitch-bend center bug (8192, not 0)

**Issue:** Any wheel touch sharpened everything; jitter randomly detuned notes.
**Root cause:** Code divided the raw 0–16383 value by 8192, so *center*
computed as full-up bend.
**Fix:** Subtract-then-divide, smooth bend for held notes, and a ±48 LSB
center deadband so worn wheels pin exact zero.
**Avoid:** MIDI centers are 8192 for bend, 64 for CCs — never treat raw wheel
bytes as bipolar directly.

## 14. Pitch-wheel snaps caused laser zaps

**Issue:** Occasional laser "pew" while playing; stopped when touching the wheel.
**Root cause:** The wheel handler snapped every held voice's target pitch
instantly, so each jitter event stepped the pitch discontinuously.
**Fix:** Removed the immediate snap — voices follow the smoothed bend path
(~8 ms glide). Verified with a zero-crossing pitch tracker (46¢ max jump).
**Avoid:** MIDI input must never step DSP state directly; everything from
the outside world goes through a smoother.

## 15. THE BIG ONE: smoothing self-cannibalisation (the zap root cause)

**Issue:** Random laser zaps; a "silent" voice sang a steady 220+440 Hz tone.
**Root cause:** `smX += (p.x - smX) * k; p.x = smX;` inside the sample loop.
After sample 0 writes back into `p.x`, every remaining sample computes
`(sm-sm) = 0` — each smoother advanced one sample-step per *block*, i.e.
512× too slow. Every knob/preset change glided over ~4 s (slow filter sweeps
sound exactly like lasers) and voices sat at mid-slew levels for seconds.
**Fix:** Snapshot targets into locals once per block, smooth toward those.
Silent voices now read bit-exact 0.00000. (Members like `bendSemis` were
immune — only read-then-overwrite of `p.*` self-cannibalises.)
**Avoid:** This is now a CHECKS.md item with its own regression tests
(silent-voice, filter-closed, pitch-slew). Any per-sample smoother that both
reads and writes the same field is suspect on sight.

## 16. Chorus Wide == Manual (copy-paste constant bug)

**Issue:** Wide mode sounded identical to Manual.
**Root cause:** The mode branch assigned the same constants (leftover copy-paste).
**Fix:** Wide got its own design (longer base + quadrature LFO) plus
wet-path-only darkening for all modes.
**Avoid:** When adding modes, test each mode's output differs (or at least
listens different) — dead branches hide behind shared code paths.

## 17. UI geometry by eyeball (the 20 px gap, Mix/bar overlap)

**Issue:** Bottom-row gaps uneven; Mix knob overlapped the section bar.
**Root cause:** Hand-computed coordinates, twice.
**Fix:** Measured bar edges/gaps from rendered pixels; 6 px everywhere,
verified by script, not by looking.
**Avoid:** Assert layout numerically (pixel-measure in tests/renders);
eyes adapt, rulers don't.

## 18. Stale-binary confusion (two incidents)

**Issue:** (a) Screenshots showed old UI after a rebuild; (b) the shipped
`.so` lacked new code while the build log was quiet.
**Root cause:** (a) harness wrote the PNG without deleting the old one;
(b) a relink race left a stale shared object.
**Fix:** Harness deletes screenshots before writing; verify shipped binaries
with a `strings` check for new symbols, not just the build log.
**Avoid:** Never trust "build succeeded" — verify the artifact contains the
change. Delete-then-write for all generated test outputs.

## 19. Test flakiness from timing assumptions

**Issue:** Idle-gate and tuning tests flaked intermittently.
**Root cause:** Fixed block counts that didn't cover release time + gate
time, and FFT peak-picking biased by sub-oscillator dominance.
**Fix:** Windows sized from actual time constants (release + 6 s gate);
FFT searches ±3% around the *expected* partial; paranoid tolerances with
documented rationale (slop/drift are random by design).
**Avoid:** Derive every test window from the DSP's own time constants, and
remember analog-character features make exact-value asserts invalid —
assert bands, not points.

## 20. Preset system grown out of code
**Issue:** Needed a second 128-preset bank, editable/overwritable presets,
and bank tags — without recompiling.
**Fix:** Presets are now data files (`NNN Name [Bank].xml`), scanned at
runtime; user overwrites shadow (never modify) shipped files; DEL restores.
`getNumPrograms` follows scanned content.
**Avoid:** Ship content as data from day one. Bonus lesson: when reordering
an indexed list (arp divisions slow→fast), migrate stored indices and
preserve original file line-endings (CRLF→LF flip polluted a whole commit).

## 21. Exact float equality in tests breaks on ARM (FMA contraction)

**Issue:** Self-test passed on Linux/Windows x86-64 but failed on macOS ARM64
at `CHECK(raw eqhigh == 4.0f)`.
**Root cause:** JUCE 8 `getRawParameterValue` returns *denormalised* values,
so the check round-trips 4.0 → normalized → back. ARM64 clang fuses
multiply-add by default, landing on `4.0000005f` instead of exactly `4.0f`.
The test (not the DSP) was wrong.
**Fix:** All such checks now use epsilon comparison (`< 1e-3f`).
**Avoid:** Never `==`-compare floats in tests, especially across a
normalize/denormalize round-trip, and especially when CI spans x86 + ARM.

## 22. Factory banks were clones of one template; one preset was off-key

**Issue:** Pads and leads sounded alike; whole Arp/Bass sub-banks differed by
a single cutoff or a few cents of detune (two arps were bit-identical).
Mono Growl played a quarter-tone sharp.
**Root cause:** Bank 2 was generated by copying one template and nudging
one or two values. Pitch: VCO1 FM is `f * (1 + xmod*2.5*s2)` clamped at
1 Hz, so xmod > 0.4 clips the negative half-swing and biases pitch sharp;
with sync on, even smaller amounts shift the period.
**Fix:** Redesigned leads, pads, bank-2 arps and bank-2 basses so each owns
a distinct source / motion / envelope / FX recipe; volumes re-solved per
bank from rendered loudness. Pitch-probed every preset; capped FM at
xmod ≤ 0.35 where it detuned.
**Avoid:** Judge presets from rendered audio, not parameter diffs: render
each one, compare spectral/envelope features pairwise within a bank, and
measure held-note pitch (decaying, gated or resonant-arp patches fool f0
estimators — re-check those with sustain on / arp off). For decaying sounds
(keys, plucks) compare the attack window and decay curve too — features from
the held portion miss them. Two pure tones at the same pitch with random
free-running phases cancel/reinforce per note: stack short sine/tri sounds
an octave apart.

## 23. Code review sweep: silent DSP bugs the harness never exercised

**Issue / root cause / fix** (each verified old-vs-new with a targeted probe):
- Delay sync note values were all 25% short ("1/4" echoed at 379 ms @120).
- Delay time jumped the read head on every change → zipper; now glides ~80 ms.
- `GlobalLFO::noteOn()` was never called → LFO DELAY only faded in once at load.
- Unison taps were cut (not faded) when the count dropped; loop stopped at
  the new count so their gains also froze.
- XMOD clamped VCO1 to ≥1 Hz → pitch dragged sharp above 0.4 (an octave+
  at 0.9). Now through-zero FM.
- Pitch bend lived in the glide target → wheel crawled through portamento.
- GATE VCA held full level for the whole amp release, then hard-cut.
- Envelope params latched at note-on only; sustain now glides when edited.
- MIDI events stamped ≥ block length were dropped (stuck notes).
- Chorus/delay fractional read: `rp = pos - d; if (rp<0) rp += len;` can
  round to exactly `len` in float → `buf[len]` read past the end. Garbage
  depended on heap layout, so an unrelated UI change "caused" a click at a
  fixed LFO phase. Wrap the integer index too.
- Audio-thread heap allocs (EQ coefficients, scratch vectors, arp list);
  Panic/Init mutated voices from the UI thread (now deferred flags).
**Avoid:** For every user-facing control, write a probe that proves it
*does something audible and correct* — "renders finite and doesn't click"
tests can't catch a dead knob or a wrong note value.

## 24. Leads were mono, glides never landed, sync sat flat

**Issue:** "All factory leads are monophonic and drift in pitch."
**Root cause:** Four separate things: (a) 16 of 20 bank-2 `[Lead]` presets
were Mono/Legato, and the "leads poly" policy test only covered 16–31.
(b) Glide was a plain RC, which never arrives: a 5-semitone legato slide at
porta 0.3 still sat 20–50 cents flat for most of the next note. (c) Legato
mode glided into *detached* notes while the previous one was still releasing.
Poly portamento slid each voice from its own previous note, which with
rotating voice assignment was several notes back (random swoops). (d) Under
hard sync, VCO-2 tap u locks to VCO-1 tap u. With fewer VCO-2 taps it
followed VCO-1's outermost (flattest) tap, about 13 cents flat. Also,
vibrato on only one VCO beats against the other and reads as detune.
**Fix:** The RC now aims 0.5 st past the note and stops on it. Legato glides
only from a held key. Poly glide starts from the last note played on any
voice. A synced VCO-2 uses VCO-1's tap count. All leads are poly with porta
0 except Portamento Lead. Vibrato goes to both VCOs, capped at 0.12 with
≥0.5 s delay. Mono→poly presets were re-levelled to their old survey
loudness. The policy test now covers every `[Lead]` file.
**Avoid:** `ErsaHarness --leadprobe <dir>` renders a chord plus a
detached/legato phrase per lead, for offline pitch tracking. Check chord
voice count, settle time and steady-state cents, not just "is it finite".

## 25. Arp drifted against the DAW

**Issue:** "ERSA's arp is broken and is out of sync with the DAW."
**Root cause:** The arp clock was a free-running sample countdown. It read
only the host BPM and never its beat position, so steps fell at an arbitrary
phase against the bar. The first key waited a random fraction of a step. The
pattern resumed from a stale step index, and the swing long/short phase
wandered. Division multipliers were rounded floats (0.3333), which would
drift once derived from song position.
**Fix:** While the transport plays, each step is a slot on the host grid
(`ppq × steps-per-beat`; swing moves odd slots to 1+swing). A step fires when
the slot number changes. Starting the transport, enabling the arp or changing
the division joins the grid without an off-beat step. A loop or rewind restarts
the pattern. A fresh phrase starts at step 0, and a key hit up to ¼ step late
plays at once. Stopped transport free-runs from the first key. Divisions use
exact fractions.
**Avoid:** Anything tempo-synced in a plugin must follow the host's ppq
position, not just its BPM. The harness drives a `FakePlayHead` that starts
mid-beat and asserts every onset sits on the grid.
