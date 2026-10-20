# Periscope

**An audio analyser that works with a screen reader.** Loudness, dynamics, spectrum, stereo, key, tonal balance against a reference, and a rules engine that writes down what it measured, all readable with NVDA and fully usable from the keyboard.

This is a fork of [TELESCOPE by OVNI Audio](https://ovniaudio.com/telescope), a free analyser plugin. It is based on TELESCOPE 0.2.0, which the author published as open source under the GNU AGPLv3. The original draws everything as pictures, so screen readers found nothing in it. This fork adds real controls on top: every number as text, every setting from the keyboard, spoken messages when an analysis finishes. For sighted users the plugin looks and works exactly as before.

## Download

**[Download Periscope for Windows](https://github.com/fdrv161/periscope/releases/latest/download/Periscope-windows-vst3.zip)**: a ZIP with the VST3 plugin and the manual, for Windows 10 and 11, 64-bit.

**[macOS, experimental](https://github.com/fdrv161/periscope/releases/latest/download/Periscope-macos.zip)**: VST3 and AU, Apple Silicon and Intel. It builds and passes validation, but speech has only been tested with NVDA on Windows: VoiceOver may or may not work well. Reports are welcome.

Release notes: [Releases](https://github.com/fdrv161/periscope/releases).

Install on Windows: extract the ZIP and copy the `Periscope.vst3` folder into `C:\Program Files\Common Files\VST3` (or another folder your DAW scans), then rescan plugins. In the DAW it is listed as "Periscope (fdrv161)", so it can sit next to the original TELESCOPE. For macOS, see [Installing](MANUAL.md#installing) in the manual.

## Manual

**[Read the manual](MANUAL.md)**: installing, first steps with a screen reader, all keys, and every lens explained, from what it measures to the rows you will hear.

Also in [Russian (Русский)](MANUAL.ru.md) and [Spanish (Español)](MANUAL.es.md). The English manual is the main version; the translations are updated together with it.

## What's new in 1.4.0

- **Shape and tilt against the reference** in Tonal balance, with the Q key for the matching EQ bell or shelf.
- **Third-octave and Bark bars read the true band level**, the same with every window.
- **Chromagram and key**: no leak a semitone down, so recordings tuned a little flat land on the right notes.
- **Long sessions**: integrated loudness and loudness range no longer slow down without a reset.

## What's new in 1.3.0

- **Comparison with the reference** in Tonal balance: how much to turn the reference down or up so both play equally loud, plus loudness range, PLR and true peak, yours and the reference's.
- **PSR target** in Dynamics, and how much of the time PSR stayed below it.
- **Crest factor** in Dynamics, counted as most meters do (a sine reads 0 dB).
- **Correlation history** in Scope: how long the correlation was below zero and below 0.5 over up to 10 minutes.

## What's new in 1.2.0

- **Q key** on a Spectrum peak or dip: speaks the Q of an EQ bell of the same width, counted the usual way (one octave is Q 1.41).
- **1/3 octave smoothing** in the Spectrum lens, the usual view for judging the overall balance.
- **Copy readings**: Ctrl+C copies the current row as text, Ctrl+Shift+C the whole group, to paste into notes or a chat.

## What's new in 1.1.1

Fixes only, found by a review of how the plugin starts, stops and restores its state.

- REAPER no longer stays in the task manager after closing: the last cause, a settings file kept open until unload, is gone.
- The first A/B press no longer loses the language, the reference and the lens settings.
- Undo, presets and A/B no longer analyse the same reference or Verdict file again.
- A mono input is measured as identical left and right, not as left and silence.
- Fixed rare crashes when a lens or the plugin window closed while a file or Save preset dialog was open.
- Smaller fixes: Reset during playback, preset names, damaged projects, the shared settings file. The manuals were checked against the code.

## What's new in 1.1.0

- **Weighting** in the Spectrum lens: A, B, C, D, K, ITU-R 468, ISO 226 and the inverse cinema X and N curves, in place of the Slope tilt. The plot, the bars and every spoken level follow it.
- **Masking** in the Spectrum lens: an estimate of which frequency ranges louder sounds nearby cover up, and which lie below the threshold of hearing. Darkened on screen, spoken in a Masking row and by the probe and the peaks.
- Both follow TDR Prism's Analyzer Weighting and Auditory Masking, written from public standards and checked against Prism's display.
- A newly inserted Periscope starts in the language you chose last, like the theme.
- Fixed: REAPER could stay in the task manager after closing (1.0.0).

## What's new in 1.0.0

- First release under the name Periscope: "Periscope (fdrv161)" in the DAW, `Periscope.vst3` on disk.

## In short

- Thirteen lenses: Loudness, Dynamics, Spectrum, Spectrogram, Waterfall, CQT, Spiral, Scope, Band correlation, Stereo spectrogram, Field, Tonal balance, Verdict.
- Three panels, reached with Tab: the lens list, the readings of the open lens, and the plugin controls (bypass, presets, A/B, size, language, theme).
- In the readings: arrows move through rows and groups, plus and minus change a setting, Enter opens edit mode or runs an action, F5 reads a live value again, F1 gives spoken help, Escape goes back.
- Space, function keys and the Ctrl and Alt shortcuts the plugin does not use go to your DAW while the plugin has focus; other plain keys stay in the plugin.
- In REAPER's FX chain, F6 (an OSARA key) moves focus into the plugin, to the lens list.
- Speech in English, Spanish, Portuguese, French, German, Italian and Russian. The screen in the same languages.
- It never changes your audio: zero latency, bit-exact pass-through, as long as the host parameters In, Out and In Phase stay at their defaults.

Tested with NVDA in REAPER on Windows. The macOS build is experimental.

## Source and licence

TELESCOPE is by [OVNI Audio](https://ovniaudio.com/telescope). Version 0.2.0, on which this fork is based, was published as open source under the **GNU AGPLv3**, and this fork stays under that licence. The author no longer publishes the source code of TELESCOPE, so this fork does not receive the author's later changes. This fork is on the [`accessible`](https://github.com/fdrv161/periscope/tree/accessible) branch, and the full source of every release is available at its tag.

- Plugin source: [`plugins/telescope`](plugins/telescope)
- Speech and keyboard code: [`plugins/telescope/source/a11y`](plugins/telescope/source/a11y)
- The author's original manual, with every formula and test result: [`plugins/telescope/docs/manual/README.md`](plugins/telescope/docs/manual/README.md)
- The repository also holds OVNI's other plugins as they were at that time, unchanged in this fork. For their current versions, see [ovniaudio.com](https://ovniaudio.com).
- Third-party notices: [`NOTICE.md`](NOTICE.md)

**How this was made.** The speech and keyboard layer, its translations, the Russian translation, the build scripts and this documentation were written by Claude, an AI model by Anthropic, working in Claude Code at the request of [fdrv161](https://github.com/fdrv161). fdrv161 tested the Windows build by hand with NVDA in REAPER. The macOS build has not been tested by a person.

## Feedback

Found something that is not readable, a key that does not work, or a phrase that sounds wrong in your language? [Open an issue](https://github.com/fdrv161/periscope/issues).
