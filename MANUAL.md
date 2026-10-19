# Periscope — manual

This manual in other languages: [Русский](MANUAL.ru.md) · [Español](MANUAL.es.md). The English one is the main version; the translations are updated together with it.

Periscope is TELESCOPE, the audio analyser by OVNI Audio that also draws conclusions, with speech and keyboard control added. The plugin's own screen still says TELESCOPE, and so does this manual where it describes the analyser. This manual covers every lens, what it measures and how to read it, and how to use all of it with a keyboard and a screen reader.

The analysis engine, the lenses and the measurement notes come from the original [TELESCOPE by OVNI Audio](https://ovniaudio.com/telescope), version 0.2.0, which the author published as open source under AGPLv3. The keyboard and screen reader layer was added in [this fork](https://github.com/fdrv161/periscope/tree/accessible). Where this manual explains what a lens measures, it follows the author's manual, shortened. The full original, with every formula and test result, is in [`plugins/telescope/docs/manual/README.md`](plugins/telescope/docs/manual/README.md).

## Contents

- [What this plugin is](#what-this-plugin-is)
- [Installing](#installing)
- [First steps with a screen reader](#first-steps-with-a-screen-reader)
- [The window and the three panels](#the-window-and-the-three-panels)
- [Keyboard reference](#keyboard-reference)
- [The plugin panel: bypass, presets, size, language, theme](#the-plugin-panel)
- The lenses:
  [Loudness](#1--loudness) ·
  [Dynamics](#2--dynamics) ·
  [Spectrum](#3--spectrum) ·
  [Spectrogram](#4--spectrogram) ·
  [Waterfall](#5--waterfall) ·
  [CQT](#6--cqt) ·
  [Spiral](#7--spiral) ·
  [Scope](#8--scope) ·
  [Band correlation](#9--band-correlation) ·
  [Stereo spectrogram](#10--stereo-spectrogram) ·
  [Field](#11--field) ·
  [Tonal balance](#12--tonal-balance) ·
  [Verdict](#13--verdict)
- [Analysing a file](#analysing-a-file)
- [Languages](#languages)
- [Known limits](#known-limits)
- [Licence and credits](#licence-and-credits)

---

## What this plugin is

Thirteen **lenses** over one analysis engine, plus a rules engine (Verdict) that turns what was measured into sentences. Each sentence carries the number behind it.

- **It does not touch your audio.** TELESCOPE reads the left and right channels, analyses them on its own thread and writes nothing back. Zero latency, no tail, bit-exact output. The author verifies this with automated tests. This holds while the host parameters In, Out and In Phase stay at their defaults: they are not in the plugin window, but your DAW's parameter list and automation show them, so leave them alone.
- **The audio never waits for a measurement.** Analysis runs on a separate thread.
- **Stereo or mono input.** A mono input is measured as identical left and right (correlation +1, width 0). No surround, no Ambisonics.
- **It does not locate sources.** Field and the polar views show energy by pan direction, not where a sound was in a room.
- **No AI, no network, no telemetry.** Verdict is a fixed table of thresholds: the same signal always gives the same report, and nothing leaves your computer.

### What this fork adds

The original interface is fully painted, so screen readers found nothing in it. This build puts real, focusable controls on top of the painted interface:

- a **lens list**, a **readings panel** and a **plugin panel**, all reachable with Tab;
- every number of every lens as text rows ("Integrated, -14.2 LUFS");
- every setting adjustable from the keyboard;
- spoken messages when a background analysis finishes;
- speech in seven languages: English, Spanish, Portuguese, French, German, Italian and Russian;
- Russian on screen as well.

For sighted users nothing changes: the mouse works exactly as before, and a mouse click does not move keyboard focus into the plugin, so your DAW shortcuts keep working.

Tested with NVDA in REAPER on Windows. Other screen readers that use UI Automation (Narrator, JAWS) should work too.

---

## Installing

### Windows

A **VST3 plugin for Windows 10 or 11, 64-bit**. This is the tested build.

1. Download [`Periscope-windows-vst3.zip`](https://github.com/fdrv161/periscope/releases/latest/download/Periscope-windows-vst3.zip) from the [latest release](https://github.com/fdrv161/periscope/releases/latest).
2. Extract the ZIP. Inside is a folder named `Periscope.vst3`. Copy that whole folder into your VST3 folder:
   - the standard one is `C:\Program Files\Common Files\VST3` (Windows asks for administrator permission to copy there);
   - or any folder you added to your DAW's VST3 paths. In REAPER: Options, Preferences, Plug-ins, VST, "VST plug-in paths".
3. Restart your DAW or rescan plugins. In REAPER: Options, Preferences, Plug-ins, VST, "Re-scan".
4. Insert **Periscope** on a track or on the master. In the plugin list it is "Periscope (fdrv161)".

To update, close the DAW and replace the `Periscope.vst3` folder with the new one.

The build is not code-signed. That does not affect a VST3 loaded by a DAW.

### macOS (experimental)

VST3 and AU, universal (Apple Silicon and Intel), macOS 11 or newer. It builds and passes pluginval, but **speech has only been tested with NVDA on Windows**. JUCE exposes the same controls to VoiceOver and the announcements use VoiceOver's own mechanism, so it may work well, partly, or not at all. If you try it, please [report how it goes](https://github.com/fdrv161/periscope/issues).

1. Download [`Periscope-macos.zip`](https://github.com/fdrv161/periscope/releases/latest/download/Periscope-macos.zip) and open it.
2. Copy `Periscope.vst3` into `~/Library/Audio/Plug-Ins/VST3` and `Periscope.component` into `~/Library/Audio/Plug-Ins/Components`.
3. The build is signed ad hoc, not notarized by Apple, so macOS blocks it until you remove the quarantine flag. In Terminal:

   ```bash
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Periscope.vst3 ~/Library/Audio/Plug-Ins/Components/Periscope.component
   ```

4. Rescan plugins in your DAW. Logic may need a restart to list the AU.

On macOS the keys are the same as below, with Option in place of Alt.

---

## First steps with a screen reader

1. Insert Periscope and open its window. In REAPER the FX window opens with the plugin. If focus is still on the FX chain list, press **F6** (an OSARA key): focus moves into the plugin, to the lens list.
2. Focus lands on the **lens list**. NVDA says something like "Lenses, Loudness".
3. Press **Up** and **Down** to choose a lens. The lens switches immediately, the same as clicking it.
4. Press **Enter** to go to the **readings** of that lens. NVDA reads the current row, for example "Integrated, -14.2 LUFS".
5. Use **Up** and **Down** to move through rows, **Left** and **Right** to move between groups (Readings, Settings, Actions and so on).
6. On a setting, press **plus** or **minus** to change it, or **Enter** for edit mode. **Alt+Up / Alt+Down** also change it by one step, and **Alt+Page Up / Alt+Page Down** by a big step.
7. Press **Escape** to go back to the lens list.
8. Press **F1** anywhere for spoken help about the current panel.

Start playback with **Space**: it goes to the DAW, so you can start and stop the transport without leaving the plugin.

---

## The window and the three panels

The painted window has a strip of thirteen lenses on the left, the open lens in the middle, and a header at the top with bypass, presets, A/B, window size, language and theme.

For the keyboard it is three panels, in this Tab order:

1. **Lens list.** One row per lens. Choosing a row opens the lens.
2. **Readings.** A grid of groups and rows for the open lens: its numbers, its settings and its actions. Each lens remembers where your cursor was, so coming back to a lens returns you to the same row.
3. **Plugin.** The header controls: bypass, presets, A/B, window size, language and theme.

Tab moves forward and Shift+Tab moves back. After the last panel Tab wraps to the first.

Only the modules the open lens needs are computed, so lenses you are not looking at cost nothing. The exception is loudness, which always runs, so integrated loudness, loudness range, the histogram and the clip count never have gaps.

---

## Keyboard reference

### Lens list

- **Up / Down**: choose a lens (it opens at once).
- **A letter**: jump to the next lens whose name starts with that letter.
- **Enter**: go to the readings of the lens.
- **Tab / Shift+Tab**: next or previous panel.
- **F1**: spoken help.

### Readings

Moving:

- **Up / Down**: previous or next row in the group.
- **Left / Right**: previous or next group.
- **Home / End**: first or last row of the group.
- **Ctrl+Home / Ctrl+End**: first or last row of the whole grid.
- **Page Up / Page Down**: 10 rows up or down.
- **A letter or digit**: next row starting with it. With **Shift**: previous.
- **F5**: read the row again with a fresh value. Readings are live, so this is how you follow a changing number.

Changing values:

- **Plus / Minus** (main keyboard or numpad; `=` also counts as plus): one step.
- **Alt+Up / Alt+Down**: one step.
- **Alt+Page Up / Alt+Page Down**: a big step.
- **Alt+Home / Alt+End**: minimum or maximum.
- **Enter on a setting**: edit mode. Then **Up / Down** change the value, **Page Up / Page Down** make a big step, and **Enter** or **Escape** leave edit mode.
- **Enter on an action** (Reset, Load and so on): run it.
- **Enter on a plain reading**: read it again.

When a value changes, only the new value is spoken. At the end of a range nothing is spoken and nothing wraps around: silence means you are at the edge. The same goes for the first and last rows. Three things do go round: plus and minus on the Preset row, Enter on the Language row, and the search by first letter.

Other keys:

- **Escape**: back to the lens list (or out of edit mode).
- **F1**: spoken help. On a row that has its own help (the Spectrum settings, for example) it explains that row; elsewhere it gives the help of the panel.
- **Tab / Shift+Tab**: next or previous panel.
- **Ctrl+C**: copy the current row as text, the way it is spoken. **Ctrl+Shift+C**: copy the whole group, its name and then one row per line, for example all the Spectrum readings. Paste them into notes or a message.

### Keys that go to your DAW

- **Space**: start and stop the transport.
- **Ctrl and Alt shortcuts** that the plugin does not use (Ctrl+S, Alt+F4 and so on).
- **Function keys** other than F1 and F5.

Other plain keys are kept by the plugin while it has focus, so an arrow or Delete never triggers a DAW action by accident.

### Spoken messages

TELESCOPE speaks on its own when a background analysis starts and finishes, and when the analysis falls behind:

- Tonal balance: "Analysing reference", then "Reference analysed, ..." or "Reference could not be read: ...".
- Verdict: "Analysing" with the file name, then "File analysed, ..." followed by the headline, or "File could not be read: ...".
- Loudness: "Analysis fell behind and skipped some audio, the readings may be off", once, when the computer could not keep up.

Everything else is spoken in response to your keys.

---

## The plugin panel

The rows of the third panel, in order:

- **Bypass**: Enter or plus/minus switch it on and off. TELESCOPE passes audio through unchanged either way.
- **Preset**: the current preset name, with "modified" if you changed something. **Plus / minus** select the next or previous preset, factory presets and yours in one list, going round. **Enter** opens the preset menu; arrow keys and Enter choose, and focus returns to this row afterwards. In the menu, Production holds the four factory presets (the Loudness lens with no target, Spotify, Apple or YouTube), Sound design is empty in this plugin, and User lists your own presets once you have saved some.
- **Save preset**: **Enter** opens a dialog with a name field. Type a name and press Enter, or Escape to cancel. Characters a file name cannot have (\ / : * ? " < > |) are removed from the name, and a preset with the same name is replaced without asking. A preset keeps the whole state, including the language, the reference path and the lens settings. Your presets are files in `%APPDATA%\Application Support\OVNI TELESCOPE\User Presets`, shared with the original TELESCOPE.
- **A B slot**: which of the two comparison slots is active. Plus selects B, minus selects A, Enter switches.
- **Copy this slot to the other**: Enter copies the active slot over the other one.
- **Window size**: Small, Medium or Large (S, M and L on screen). Enter starts edit mode; plus and minus also work. Each size has its own layout, not a scaled drawing.
- **Language**: the language of the screen and of speech, see [Languages](#languages). Enter goes to the next language; plus and minus also work.
- **Theme**: dark or light. Plus selects light, Enter switches.

Theme, language and window size are remembered for new windows and new instances. They are kept in `%APPDATA%\OVNI\OVNI.settings` (on macOS `~/Library/Application Support/OVNI/OVNI.settings`), a file shared with the original TELESCOPE. A saved project keeps its own language.

---

# The lenses

Each lens below has two parts: what it measures and how to read it (from the author's manual), and **With the keyboard**: the groups and rows you will hear.

## 1 · Loudness

![Loudness lens: four level bars, the integrated value and three minutes of history](plugins/telescope/docs/manual/images/loudness.png)

**What it measures.** How loud the programme is, to the broadcast standard (ITU-R BS.1770 and EBU R 128).

- **Integrated** (LUFS): the whole programme since the last reset, with the standard double gate (absolute at -70 LUFS, relative at 10 LU below the mean).
- **Momentary** (LUFS): a 400 ms window.
- **Short-term** (LUFS): a 3 s window, updated 10 times a second.
- **Loudness range** (LU): EBU Tech 3342, the spread between the 10th and 95th percentiles of the short-term values.
- **True peak** (dBTP): BS.1770 Annex 2, measured before the K-weighting, overall and per channel.
- **Maximum momentary and short-term**: since the last reset.

**How to read it.** On screen there are four bars on one 0 to -60 scale: momentary and short-term in LUFS, left and right true peak in dBTP. Below is three minutes of short-term history. If you choose a streaming platform target, its line is drawn and the distance from your integrated value is written out.

**While a window is still filling** (400 ms for momentary, 3 s for short-term) the value is *partial*: the same calculation over the audio there is, not a guess. Once the window fills it matches the official number exactly. Integrated loudness has no partial value: the gate is the gate.

**Worth knowing.** True peak is measured with 4x oversampling, which can under-read slightly, within the standard's own tolerance. If your master reads -1.0 dBTP here, treat it as a little higher.

### With the keyboard

- **Readings**: Integrated, Short-term, Momentary, Loudness range, True peak max (overall, then left and right), Max short-term, Max momentary, History, Target (the platform and how far you are from it). A value that is still filling is followed by "partial". When the analysis had to skip audio because the computer could not keep up, the screen shows an amber "analysis behind" line: TELESCOPE says it once when it appears, and Integrated is followed by "analysis fell behind" until the next reset.
- **History** tells the three minutes of short-term history on screen the way the eye reads the graph: steady stretches with their length and level, oldest first, then the loudest and the quietest moment. For example: "last 3 minutes, 40 seconds around -14 LUFS, then 1 minute 20 seconds around -9 LUFS, then 1 minute around -11 LUFS, up to now, loudest -8.7 LUFS, 1 minute 30 seconds ago". It shows where the chorus is louder than the verse, or where the loudness jumped. At most five stretches: when there are more steps, small ones are joined, as the eye sees fewer steps from further away.
- **Actions**:
  - **Platform target**: plus and minus step through none, Spotify, Apple Music, YouTube, Amazon Music, Tidal and Deezer. It is also the host parameter "Target", so it can be automated.
  - **Pause**: Enter pauses or resumes measuring.
  - **Reset**: Enter clears everything and starts again.

---

## 2 · Dynamics

![Dynamics lens: PSR and PLR, the short-term histogram and the clip timeline](plugins/telescope/docs/manual/images/dynamics.png)

**What it measures.** Not how loud the mix is (that is Loudness) but how much headroom is left in it.

- **PSR** (dB): highest true peak of the last 3 seconds minus short-term loudness. The live reading.
- **PLR** (dB): highest true peak since reset minus integrated loudness (AES TD1004).
- **Histogram**: short-term loudness since reset, in 1 LU steps.
- **Clips**: events above the clip threshold (default -1.0 dBTP, range -3 to 0) since reset, and a 10-minute timeline.

**How to read it.** A steady sine reads PSR = PLR = 0 dB, and that is correct: a steady tone has no peak headroom above its own level.

**One clip event is not one sample over the line.** An overshoot that lasts 50 ms crosses the threshold once per cycle, but it is counted as one event: an event starts on the first sample above the threshold and ends only after 100 ms below it.

**Changing the threshold restarts the count**, because a count measured against two different ceilings would mean nothing.

The line on screen is your PSR target, 8 dB until you change it (see Actions below). It is a reference, not a judgement. Judgements are Verdict's job, with the rule visible.

### With the keyboard

- **Readings**: PLR, PSR (with "low" under 4 dB, and "below target" under your PSR target, where the bar turns amber), Time below the PSR target (the share of the analysed time, up to the last 10 minutes, with PSR under the target, for example "35 percent of the time below 8 dB, over 2 minutes 10 seconds"; silent seconds are left out), Crest factor (sample peak over RMS since reset, per channel, the higher of the two, with RMS counted the AES17 way so a sine reads 0 dB, as in dpMeter and most meters), Short-term, Most frequent short-term (the histogram's peak and how much of the time it held), Most of the time (where the middle 80 percent of the short-term readings lie, the width of the histogram, for example "-12 to -8 LUFS"), Clip events (count above the threshold), Clips in the last 10 minutes (and how long ago the latest one was).
- **Actions**:
  - **Clip threshold**: plus and minus move it by 0.1 dB, a big step is 0.5 dB.
  - **PSR target**: plus and minus move it by 0.5 dB, a big step is 1 dB, from 3 to 20 dB. Kept in the project. As a guide: about 12 to 14 dB for a mix, 7 to 8 for a loud master that still breathes, 5 to 6 for dance music.
  - **Pause** and **Reset**: Enter.

---

## 3 · Spectrum

![Spectrum lens: FFT curve with peak hold](plugins/telescope/docs/manual/images/spectrum.png)

**What it measures.** The frequency spectrum, with an exact dB reference: each bin is in dBFS relative to a full-scale sine, so a sine of a given amplitude reads its true level with every window shape.

**How to read it.** Two consequences are worth knowing:

- **Full-scale white noise does not read 0 dBFS per bin.** Its energy is spread over thousands of bins, so each one reads far lower.
- **Changing the FFT size moves the noise floor, not the tones.** Doubling the size splits each bin in two: a tone keeps its level, noise drops 3 dB per bin.

**Why the default slope is 3 dB per octave.** An FFT bin is a fixed width in hertz, while music and hearing work in octaves. Pink noise, the neutral reference, falls 3 dB per octave in a raw spectrum. With a slope of 3, pink noise reads flat. A typical finished mix falls a little faster, which is why many analysers use 4.5. The slope is a display setting: it does not change the data, and it is not applied to the band modes, which already measure in fractions of an octave.

**Smoothing does not change the data either.** It smooths the drawn curve: off shows every narrow peak (a busy picture, good for hunting resonances), 1/6 octave shows only broad humps and holes (the overall balance).

**Weighting.** Instead of the Slope tilt, the plot can show the spectrum through a standard curve, the way a meter or the ear weighs it: A, B, C and D (sound level meters), K (the loudness curve of LUFS), ITU-R 468 (noise as the ear hears it), ISO 226 (the equal-loudness contour at 80 phon), and the inverse cinema X and N curves, which, as on a cinema analyser, also draw pink noise flat. Every curve is 0 dB at 1 kHz, so a 1 kHz tone reads the same under all of them. Like the slope, it is a display setting: it changes the plot, the bars and every level spoken in this lens, not the data, Tonal balance or Verdict. Choosing Slope brings back the Slope setting.

**Masking.** An estimate of auditory masking: the frequency ranges that louder sounds nearby cover up, so the ear does not hear them in the mix, and the ranges below the threshold of hearing. It uses the psychoacoustic model of AAC encoders (3GPP TS 26.403) on bands a third of a Bark wide. On screen, masked bands are darkened, those below hearing less. It follows the curve as drawn, so the Average setting sets how fast it changes: with Average off it is moment by moment and jumps around, with an average it shows what stays masked. A well-balanced mix usually shows little masking apart from the deepest lows and the top octave; a lot of it in one range means the parts there fight for the same place. It is an estimate, not a full model of hearing.

**A band with no bin in it is not a band at zero.** With a small FFT, low bands can be narrower than one bin. That means "cannot be measured at this resolution", not "no energy". For bass, use a big FFT.

### With the keyboard

Slope and Smoothing exist to help the eye: Slope makes a balanced mix look flat, Smoothing decides how big a detail must be to show. Speech uses them the same way, and every level it speaks is the one the plot shows, so a sighted helper reads the same numbers. With a Slope, levels above 1 kHz go up and levels below go down: at the default 3 dB per octave a tone at 440 Hz reads about 3.5 dB lower here than in CQT or Spectrogram, and a tone at 4 kHz 6 dB higher. Set Slope to 0 to hear a tone at its true level. The third-octave bands are never tilted, as on screen; with a weighting curve instead of the Slope they are weighted too, as on screen.

**The line.** A flat line on the plot, at the median height of the curve between 40 Hz and 16 kHz: where the eye sees the curve lie. With Slope at 4.5, a typical finished mix lies along it: a place well above it has more energy than a balanced mix, a place well below has less.

- **Readings**:
  - **Probe**: a frequency cursor that moves by notes. Plus and minus move it a semitone, a big step is an octave, and the range is about 20 Hz to 20 kHz. It reads the frequency, the note name, the level of the left and right channels (with peak hold if it is on) and how far it is from the line, for example "3 dB above the line". The levels are the plot's, with the Slope. With smoothing on, the level is the height of the smoothed curve, as drawn, and the hold is the unsmoothed peak: that is the number the mouse box shows (the box always shows the unsmoothed level). With smoothing off, the probe and the box agree. The crosshair on screen follows the probe, so a sighted helper can see where you are.
  - **Signal tilt**: how the curve leans on the plot from 50 Hz to 10 kHz, for example "falls 1.5 dB per octave, darker than the line", "rises 0.3 dB per octave, brighter than the line" or "flat, follows the line". The Slope makes a balanced mix flat, so this is how much darker or brighter the mix is than the Slope expects. As for the eye, a narrow spike or notch, such as a single loud tone, does not change it. With Slope at 0 it is the signal's own tilt: pink noise falls 3 dB per octave, a typical mix about 4.5.
  - **Shape against the line**: the first glance at the picture. First the height of the line, then the stretches of the curve that stand out above or below it (see below what counts as standing out), from low to high, with their average distance, for example "line at -42 dB; 35 to 80 Hz +5 dB, 85 to 400 Hz -4 dB, 450 Hz to 2 kHz +3 dB, above 12 kHz -8 dB". A range that is not mentioned follows the line. It always looks at a broad 1/3 octave view, whatever the Smoothing, so the overview does not change while you zoom into details.
  - **Masking**: with Masking on in the settings, the masked ranges and the ranges below hearing, from low to high, for example "masked: 870 Hz to 1.3 kHz; below hearing: above 18 kHz", or "nothing masked". With it off, the row says "Off". The probe, the peaks and the dips add "masked" or "below hearing" when their frequency is.
  - **Peaks and dips** (up to 10 of each): places where the curve stands out from the neighbouring frequencies, the most prominent first. How much a hump stands out is measured the way the eye sees it: above the higher of the two valleys beside it (for a hole, below the lower of the two crests). Smoothing sets their size:
    - With smoothing off, the peaks are narrow resonances and strong notes: frequency, note with cents, how many dB it stands out, and its level on the plot.
    - With smoothing on, they are humps and holes: frequency, where it starts and ends (for example "290 to 400 Hz", handy for an EQ band), and how many dB it stands out.

    The list keeps the 10 most prominent of each; the Smoothing row says how many there are in all. Unsmoothed there are dozens of spikes and the list holds the tallest, as the eye picks them out; at 1/6 octave there are a few humps and usually all of them fit. If nothing stands out, the row says "none". Dips are listed only with smoothing on. **Enter** on a peak or a dip moves the probe there and the cursor to the probe row, so you can walk around it by semitones. **Q** on a peak or a dip says the Q of an EQ bell of the same width, for example "Q 5.3". It is counted the usual way, from the width at half the height in dB (RBJ cookbook, Equalizer APO and most EQs: one octave is Q 1.41); FabFilter Pro-Q counts about 1.4 times higher. Smoothing makes narrow peaks wider, so when the Q says "limited by the smoothing", turn smoothing off or to 1/24 to measure a narrow resonance. While music plays, the list and the shape hold still for a second, so rows read one after another agree.
- **Third-octave bands**: the 30 standard bands from 25 Hz to 20 kHz, with the level of each.
- **Settings**: the buttons under the plot, the ones that matter most for listening first:
  - **Weighting**: Slope, A, B, C, D, K, ITU-R 468, ISO 226, inverse X-curve, inverse N-curve. The value is followed by the new signal tilt.
  - **Masking**: on or off.
  - **Slope**: 0 to 4.5 dB per octave. The value is followed by the new signal tilt, so you hear the effect at once. It is used only when Weighting is Slope; otherwise the row says so, and changing it brings the slope back.
  - **Smoothing**: off, 1/24, 1/12, 1/6 or 1/3 octave. 1/3 octave leaves only the overall balance, the usual view for comparing mixes. The value is followed by how many peaks and dips stand out at this smoothing, for example "1/6 octave, peaks: 7, dips: 5": you hear how busy the picture is.
  - **Average**: off, a running average (its time is spoken), or infinite.
  - **Peak hold**: off, 12 dB per second, 30 dB per second, infinite.
  - **Range**: 60, 90 or 120 dB, how far below 0 dB the plot goes. It also sets what counts as standing out, see below.
  - **FFT size** (1,024 to 32,768 points), **Window** (Hann, Blackman-Harris, Kaiser), **Overlap**, **Channel** (left, right, mid, side, left and right), **Display** (FFT curve, third-octave bars, Bark bars).
- **F1** on the probe, the signal tilt, the shape, a peak or dip, or any setting explains it in a sentence or two.

**What counts as standing out.** An analyser has no threshold: the eye notices a bump when it has some height on screen. The plot always spans the same pixels, so that height depends on the Range. TELESCOPE counts a stretch, a peak or a dip when it stands out by a twentieth of the Range: 3 dB at 60 dB, 4.5 dB at 90, 6 dB at 120. It is the same at every smoothing, so the smoothing alone decides how much detail there is, as on screen. To hear smaller bumps, choose a smaller Range, just as a sighted user would to see them.

A way to use it while mixing:

1. For a whole song, set Average to infinite and press Reset (in the Loudness or Dynamics lens) at the start.
2. Read Signal tilt and Shape: is the mix brighter or darker than the line, and which ranges stick out or sag?
3. Smoothing at 1/6 octave: the peaks and dips are the humps and holes inside those ranges, with their edges.
4. Smoothing off: the peaks are narrow resonances and strong notes. A resonance shows in both the level and the peak hold; a peak that is only in the hold was a short hit.
5. Enter on a peak, then plus and minus around it to hear its width. When you need every number from the same moment, use Pause in the Loudness or Dynamics lens: it holds every lens, Spectrum included.

The FFT settings are shared with Spectrogram, Waterfall and the other spectral lenses: it is one setting, not a copy.

---

## 4 · Spectrogram

![Spectrogram: frequency over time with level as colour](plugins/telescope/docs/manual/images/spectrogram.png)

**What it measures.** Level over time: frequency up the screen (20 Hz to 20 kHz), time across it (10, 30 or 60 seconds, now on the right), level as colour.

**How to read it.** The colour scales are chosen so that brighter always means louder (except the "spectrum" palette, where hue orders the level). Each row takes the maximum of its bins, so narrow peaks are not flattened. Changing the FFT size, channel, range or history clears the picture, because mixing columns measured in two ways would be a picture that lies.

**The palette is one setting for four lenses**: Spectrogram, Stereo spectrogram, Waterfall and Field.

### With the keyboard

On screen, the mouse over the picture shows one point: how long ago, the frequency and the level. The keyboard probe gives the same numbers.

- **Readings**:
  - **Probe**: the frequency. Plus and minus move it a semitone, Alt with Page Up or Page Down an octave. For example "1.05 kHz, -34 dB, 2.5 seconds ago".
  - **Probe time**: how long ago it looks. Plus goes a quarter second further back, minus towards now, Alt with Page Up or Page Down one mark of the time scale (2, 5 or 10 seconds, as on screen), Alt with Home now, Alt with End the oldest moment on screen. It also says the loudest frequency at that moment, the brightest point of the column: "2.5 seconds ago, 1.05 kHz, -34 dB, loudest at that moment: 6.3 kHz, -20 dB".
- **Settings**: History, Range, Channel, Palette (Ovni, Inferno, Viridis, Spectrum). They are shared with Spectrum, so you can set them here too.

The probe gives the loudest point within a quarter tone and a quarter second, so a short click between two steps is not missed. The crosshair on screen follows the probe, so a sighted helper sees where it looks; its box reads the single point under the crosshair and can be a little lower.

**What sighted users look for** in a spectrogram, and how to find it with the probe:

- a ringing resonance: a narrow line that stays on one frequency and goes on after the note. Put the probe on the frequency and step back in time: the level stays high;
- hum: lines at 50 or 60 Hz and their multiples, all the time;
- clicks: short vertical lines, loud at every frequency for a moment. Step through time at a high frequency;
- sibilance: short bursts between about 4 and 10 kHz.

The picture moves while sound plays. To read a moment, stop playback and read it before it scrolls away; the longer the History, the more time you have. Pause in the Loudness lens does not help here: the plugin computes only what the open lens needs, so while Loudness is open the spectrogram is not written at all. Sighted users have the same limit in this plugin.

---

## 5 · Waterfall

![Waterfall: the spectrum over time in 3D](plugins/telescope/docs/manual/images/waterfall.png)

**What it measures.** The same data as the spectrogram, drawn in depth: frequency across, level as height, time going back into the distance.

**How to read it.** The eye compares heights better than colours, so narrow peaks stand out here. The cost is that a short event can hide behind a later one. Spectrogram and Waterfall complement each other.

### With the keyboard

- **Readings**: **Probe**, what the mouse shows on the front line, the newest one: frequency and level now, for example "250 Hz, -18 dB, now". Plus and minus move it a semitone, Alt with Page Up or Page Down an octave. As on screen, the lines behind cannot be read one by one; the Spectrogram probe reaches back in time over the same data.
- **Settings**: Lines (60, 90, 120), Tilt (low, medium, high), History, Range, Palette.

---

## 6 · CQT

![CQT: the spectrum by notes over a piano keyboard, with the chromagram below](plugins/telescope/docs/manual/images/cqt.png)

**What it measures.** The spectrum by **note**. A constant-Q transform spaces its bins evenly in octaves rather than hertz: 24 bins per octave (two per semitone) from A0 (27.5 Hz) upwards. Below it is the chromagram, the energy of each of the 12 pitch classes, and the estimated key.

**The bass is late, and the lens says so.** Quarter-tone resolution at 27.5 Hz needs over a second of audio (1.24 s at 48 kHz), and the time halves with every octave up. That is physics, not a shortcut, and the lens shows it.

**The key is never stated alone.** It always comes with a confidence and the share of time it held, for two reasons:

1. Even noise "wins" some key by chance, with a confidence around 0.4 to 0.5. What gives noise away is the share of time: a few percent, against nearly 100 percent for real music.
2. Some music is genuinely ambiguous. A bare C, E, G triad scores slightly higher for E minor than C major; double the C in the bass and C major wins clearly.

Silence reports no key, because "I don't know" is a result.

### With the keyboard

- **Readings**:
  - **Key**: for example "A minor, confidence 0.82, best 64 percent of the time".
  - **Strongest pitch classes**: the three strongest notes of the chromagram.
  - **Probe**: what the mouse shows on one bar: the note, its frequency and level, for example "A 2, 110 Hz, -12 dB". Plus and minus move it a semitone, Alt with Page Up or Page Down an octave. Between two notes there is a fainter quarter-tone bar; when it is louder than the note, the probe names it with 50 cents ("A 2 plus 50 cents"), so a detuned note is not missed.
  - **Note 1 to 5**: the five loudest notes of the spectrum, each with octave, deviation in cents and level. Enter puts the probe on the note.
- **Settings**: Channel, Chroma window (how long the chromagram averages: 0.5, 2 or 5 seconds).

Note names follow the plugin language (letters, do-re-mi, or German with H and B), and sharps are spoken as words.

---

## 7 · Spiral

![Spiral: the constant-Q spectrum coiled one turn per octave](plugins/telescope/docs/manual/images/spiral.png)

**What it measures.** The same constant-Q spectrum, coiled one turn per octave, so the same note in different octaves lines up on one radius. It shows the octave structure of what is playing at a glance, with a chroma wheel and the key in the centre.

### With the keyboard

- **Readings**:
  - **Key**, **Strongest pitch classes**, as in CQT.
  - **Probe**: what the mouse shows on one spike: the note, its frequency and level. The same keys as the CQT probe.
  - **Same note in every octave**: the spikes the eye sees lined up on one ray from the centre, for the note of the probe: each octave where it sounds above the range, with its level, for example "A 1, -40 dB; A 2, -12 dB; A 3, -25 dB". Plus and minus on this row change the note.
- **Settings**: Channel, Chroma window.

---

## 8 · Scope

![Scope: goniometer, correlation meter and oscilloscope](plugins/telescope/docs/manual/images/scope.png)

**What it measures.** The classic ways of looking at stereo, over a sliding window of 100, 300 or 1,000 ms.

- **Correlation**: +1 is mono, 0 is unrelated channels, -1 is out of phase.
- **Width**: 0 is mono, 1 is two independent sources, above 1 the side signal dominates.
- **Balance** (dB): positive means the right channel is louder.
- **Mono loss** (dB): how much level is lost when the mix is summed to mono. 0 if left equals right, -3 dB for independent channels, a very large loss if they cancel.

**Modes.** *Lissajous* is the goniometer: it scales itself to the signal so you see the shape of the stereo, not the level, and says what the edge level is. *Polar sample* keeps the angle and makes the radius the level. *Polar level* (called Hemisphere in the settings) is a half-circle with mono at the top and left and right on the base, energy drawn as one ray per degree; what is out of phase is folded onto the base and the percentage is printed. The oscilloscope shows 40 ms, optionally triggered on a rising zero crossing.

**Edge cases are reported, never as nonsense numbers.** No signal reads "no signal", which is not the same as perfect mono. With one channel silent the correlation is undefined and shown as 0.

**It is not localisation.** Pan direction by energy, not where a sound comes from in a room.

### With the keyboard

- **Readings** (Directions only in Hemisphere mode, like the half circle it reads): Picture, Correlation history (the needle over the whole time since reset, up to 10 minutes, one value per second, recorded whatever lens is open: how long it was below zero, how long below 0.5, and the lowest value, for example "over 3 minutes 20 seconds: below zero 4 seconds, below 0.5 40 seconds, lowest -0.21"), Correlation (with a word for its meaning, such as "nearly mono" or "out of phase"), Width, Balance ("1.2 dB louder on the right", or "centred"), Mono loss, Out of phase energy (percent), Measured over (the window length), Directions: where the energy sits from left to right, as the filled half circle shows it, for example "centre: 10 percent right, spread from 40 percent left to 60 percent right, strongest at 80 percent right". Out-of-phase energy is not in it: it has its own row.
- **Settings**: Mode (Lissajous, Polar, Hemisphere), Oscilloscope trigger (on, off), Window, Hemisphere decay (12, 24 or 48 dB per second) and Hemisphere scale (relative or decibels), these two only in Hemisphere mode, as on screen.

The picture itself is not described in text: its meaning is these numbers.

**Picture** says in words what the goniometer and the correlation needle show, as a sighted user reads them: "almost mono" (a vertical line), "normal stereo" (a round cloud), "very wide" (a flat cloud), "out of phase" or "like a flipped polarity" (a horizontal line, the mix cancels in mono). What matters is where the needle stays, not one moment: a short dip below zero from a wide reverb is harmless, staying below zero means parts of the mix vanish in mono. So the word comes from the last 10 seconds, followed by how much of that time the correlation was below zero and its lowest value. Bass and kick should stay close to mono; check them in the Band correlation lens.

---

## 9 · Band correlation

![Band correlation: correlation per third-octave band](plugins/telescope/docs/manual/images/band-correlation.png)

**What it measures.** Scope's stereo figures **per third-octave band**: correlation, width, balance and mono loss for each of the 30 bands.

**Why.** On a mix with mono bass and wide highs, a single correlation meter gives an in-between number that says nothing. In the author's test signal (an 80 Hz tone in mono plus out-of-phase noise above 2 kHz) the broadband meter reads +0.24, while the 80 Hz band reads +1.00 and the 4 kHz band -1.00. Two different things are happening, and only a per-band view shows both.

**A band with no FFT bin in it is not zero.** At small FFT sizes the lowest bands cannot be measured, and the lens says so. In the bass the readings also scatter more, because there is less signal per second down there; no analyser can invent it.

### With the keyboard

- **Summary**: Lowest correlation, Most mono loss (the worst bands, the same as the line above the plot), Bands measured.
- **Third-octave bands**: each band with all four numbers at once: "correlation 0.95; width 0.2; balance 0.1 dB; mono loss -0.1 dB". On screen the second line shows only one of the three; here you get all of them.
- **Settings**: Window (0.3, 1 or 3 seconds), Lower graph (which figure the screen shows under the correlation).

---

## 10 · Stereo spectrogram

![Stereo spectrogram: colour is phase, brightness is level](plugins/telescope/docs/manual/images/stereo-spectrogram.png)

**What it measures.** A spectrogram where **colour is phase** (red out of phase, green wide, white mono) and **brightness is level**. A cell with no energy is black, so the noise floor does not paint meaningless colours.

### With the keyboard

- **Readings**: **Probe** and **Probe time**, as in Spectrogram, with the coherence the colour shows: "1.05 kHz, -30 dB, coherence 0.95, nearly mono, 2 seconds ago". 1 is mono, 0 wide, below 0 out of phase. A cell with no energy is black on screen and has no coherence, so only its level is spoken. The level is both channels together, so a mono signal reads 3 dB above what Spectrum shows for one channel.
- **Settings**: History, Range, Window, Palette.

For numbers per third-octave band, use Band correlation.

---

## 11 · Field

![Field: energy by pan direction and frequency](plugins/telescope/docs/manual/images/field.png)

**What it measures.** Energy by **pan direction and frequency**, with decay and a trail. Pan goes from -1 (left only) through 0 (centre) to +1 (right only).

**What only this lens shows.** A mix with two hard-panned sources and a mix of decorrelated noise give nearly the same correlation and width. In Field the first is two blobs in opposite corners and the second an even cloud across. That difference is the point of the lens.

**What it does not do.**

- It does not measure where sources are. From a finished stereo mix that has no unique answer, so the axis is left, centre, right, never degrees, and the screen says "energy by pan direction, not localisation".
- It does not tell mono from out of phase: both put equal energy in each channel. Band correlation and Stereo spectrogram show that difference; the three lenses are read together.
- The dB here are **relative** to the loudest cell. Absolute levels are in Spectrum.

### With the keyboard

- **Octaves**: one row per octave band, for example "250 Hz octave, centre 10 percent left, spread from 40 percent left to 20 percent right, -6 dB". Centre is where the energy sits on the pan axis, the spread covers the main part of it (10th to 90th percentile), and the dB is relative to the loudest octave. A narrow spread is said as "narrow"; an octave with no energy as "silent".
- **Settings**: Decay (0.5, 1 or 2 seconds), Window (0.3, 1 or 3 seconds), Palette.

---

## 12 · Tonal balance

![Tonal balance: programme and reference curves with the difference below](plugins/telescope/docs/manual/images/tonal-balance.png)

**What it measures.** Not "how loud is it" but **"what colour is it, compared to this other track"**. Load a reference track, yours or a commercial one. TELESCOPE analyses it whole, offline, and compares your live programme against it, band by band.

**It compares tilt, not level.** Both curves are normalised by their own integrated loudness, so the same pink noise at -14 and at -20 LUFS shows no difference at all. Without that the graph would answer "which one is louder", which Loudness already tells you.

**So the difference sums to zero.** Add 6 dB to everything above 2 kHz and the overall loudness rises too. The lens then shows about +1.7 dB in the top and -4.3 dB in the bass: still a 6 dB gap between the two regions, but counted at equal loudness, which is how you compare a mix with a reference.

**How to read it.** A fixed scale, so two captures of the same mix stay comparable. The ±3 dB band on screen is a reference, not a judgement. A band that only one side can measure is never shown as "no difference": the row says why it cannot be compared.

**The reference is stored as a path, not as numbers.** Reopen a session and the file is analysed again. If it is gone, the lens says "reference not found" and shows nothing, rather than a reference that no longer exists.

### With the keyboard

- **Summary**:
  - **State**: what the lens is doing, as the status line on screen says (measuring, analysing the reference and so on).
  - **Reference**: the reference file name, or "none".
  - **Reference section**: the part of the reference being used, or "whole file".
  - **Shape against the reference**: the first glance at the difference curve, as Spectrum's shape against its line, with the reference in place of the line. The broad stretches where your mix is 3 dB or more above or below the reference (the band drawn on screen), each with its average difference, for example "85 to 180 Hz +4 dB, above 11 kHz -6 dB"; or "all within 3 dB of the reference". Loudness is already matched, so overall level does not show here. **Q** on this row says the EQ setting for each stretch, for example "85 to 180 Hz: Q 1.4; above 11 kHz: shelf": the Q of a bell of the same width (counted as on Spectrum's peaks), or a shelf for a stretch at either end of the range. The edges are where the difference crosses 3 dB, not half its height, so take the Q as a starting point; a bell is often set a little wider.
  - **Tilt against the reference**: how the difference leans from 50 Hz to 10 kHz, for example "darker than the reference by 0.8 dB per octave", or "same tilt as the reference". A tilt says the whole balance leans one way; a shape without a tilt points at single regions.
  - **Largest difference 1 to 5**: the bands where your programme differs most from the reference, for example "125 Hz, 3.2 dB louder than the reference".
- **Comparison with the reference**: your mix against the reference, in the same units as the Loudness and Dynamics lenses. Your mix counts from the last reset; the reference is the whole file or its section.
  - **Loudness**: how much to turn the reference down or up so that both play equally loud, then both integrated values. For example: "reference louder by 2.6 dB, turn it down by 2.6 dB; your mix -12.4 LUFS, reference -9.8 LUFS". Set the reference track's fader by that amount and compare by ear at the same loudness.
  - **Loudness range**, **PLR** and **True peak max**: yours and the reference's. A much smaller PLR than the reference's means your mix is more compressed.
- **Third-octave bands**: each of the 30 bands with the programme level, the reference level and the difference.
- **Actions**:
  - **Reset averaging**: restarts the programme's average and keeps the reference.
  - **Load reference file**: opens a file dialog.
  - **Remove reference**.
  - **Section start** and **Section end**: choose the part of the reference to compare against, without the mouse. Plus and minus move by one second, a big step is 10 seconds, Alt+Home and Alt+End go to the start or end of the file. The new bounds are spoken as you move; the reference is analysed again once you stop.
  - **Use the whole reference file**: the same as a double click on the strip.

When the reference analysis finishes, TELESCOPE says so: "Reference analysed, ..." with its name and loudness, or the reason it could not be read.

---

## 13 · Verdict

![Verdict: headline, findings by section and the within-range list](plugins/telescope/docs/manual/images/verdict.png)

Twelve lenses show. This one **says**. The rule is that every sentence carries the number that supports it.

**What it is.** A fixed rules engine: no AI, no network. A table of thresholds and the conditions that read them. The same signal gives the same report, word for word, today and in a year. Every threshold with its reasoning is in [`telescope-diccionario.md`](plugins/telescope/docs/telescope-diccionario.md) (Spanish).

**How it reads.** First a **headline**: the count of what was measured, with no adjectives, for example "15 checks within range, 3 to look at, the first at 0:20". Then the sections:

- **Within range**: every rule that was checked and did not fire, with its number and its limit, for example "Transients have room: PSR 9.4 dB (floor 8.0 dB)". A rule that could not be checked says nothing; "within range" is a measurement, not a default.
- **How it will feel**: crushed transients, thin or muddy low mids, harsh presence, no air on top, a hollow centre, and the key.
- **Where it translates**: generic checks for phone, headphones, laptop, car, club (mono sub) and hi-fi.
- **What to check, and where**: dips in bands, quiet and loud sections, clip bursts, bands out of phase, left-right imbalance, DC offset, and streaming platform targets.

**How a finding reads.** The number first, the mixing term in brackets, and at the end where to look, never what to do: "630 Hz dips 26.0 dB below its own average between 0:20 and 0:35. Check what plays in that band there." Severity is problem, warning or note; problem is kept for things that are objectively broken (clip bursts, DC, bands that cancel in mono).

**What it compares against.** With a reference loaded in Tonal balance, against the reference's shape. Without one, against your material's own spectral trend, a straight line fitted through your mix. The second is deliberate, so no hidden "good mix curve" is passed off as measurement, but it is blunt: a mix with a strong intention can fire a rule with nothing to fix. **Loading a reference is strictly better.**

**What it is not.**

- It does not give opinions. It never says "professional" or "ready"; the author's tests scan every phrase in every language for such words.
- The device checks are generic definitions of classes of systems, not measurements or simulations of real speakers.
- **"No findings" is not "it's finished".** When nothing fires, it says: "Nothing outside the ranges of these rules. They measure; what they can't hear is yours."

**Live or file.** In live mode Verdict uses the last 10 minutes since reset, once a second. The spectral part is only measured while Verdict or Tonal balance is open, so if you open Verdict halfway through a track, the report starts there. For exact times over a whole track, use **file** mode: the file analysis is identical to the live one.

### With the keyboard

- **Summary**: State (the status line on screen), Result (the headline).
- **How it will feel**, **Where it translates**, **What to check, and where**: one row per finding. Each row reads the full sentence with its numbers, followed by its severity: problem, warning or note (for the devices: OK, warning or fails). An empty section is one row that says so.
- **Within range**: one row per rule that was checked and passed.
- **Actions**:
  - **Reset analysis**.
  - **Mode**: live or file; plus selects file, minus live, Enter switches.
  - **Load a file to judge**: opens a file dialog and switches to file mode.

Section names and findings are spoken in the plugin language. When a file analysis finishes, TELESCOPE says "File analysed" and reads the headline.

---

# The rest

## Analysing a file

Tonal balance (reference) and Verdict (file mode) can analyse a whole audio file in the background, with Enter on their Load row. With the mouse, you can also drag a file onto those lenses.

- **The offline analysis is identical to the live one**, to the bit. The engine counts samples, not host blocks, and the file path uses the same code.
- **Formats**: WAV, AIFF, FLAC and Ogg; on Windows also WMA and MP3. Any file is accepted; if it cannot be read, you are told why.
- **The sample rate is the file's.** Nothing is resampled, and neither third-octave bands nor LUFS depend on sample rate, so a 44.1 kHz reference compares directly with a 48 kHz session.
- **It is fast**: a minute of audio takes well under a second on a modern computer.
- **Only a new file starts a new analysis.** Reopening a project analyses the file again, but undo, presets and A/B do not, as long as the file and the section stay the same.

## Languages

The **Language** row in the plugin panel switches the whole plugin: the screen and the speech. Seven languages:

- **English** (default) and **Spanish**, reviewed by the author;
- **Portuguese, French, German, Italian**: translated by the author with industry terms, not yet reviewed by native speakers;
- **Russian**: added in this fork, on screen and in speech.

Speech in Spanish, Portuguese, French, German and Italian was translated for this fork and has not been reviewed by native speakers. If a phrase is missing, that phrase falls back to English and the rest stays in your language.

Units (dB, Hz, kHz, ms, LUFS) and industry terms that are normally left in English (Loudness, Scope, Short-term, Momentary, True peak, Peak hold, Bypass) stay in English in every language, as on the author's screens.

Numbers are spoken with a decimal point in every language, as on screen: "-14.2 LUFS".

**Note names follow each language's convention**: letters (C, D, E) in English and Russian, do-re-mi in Spanish, Portuguese, French and Italian, and the German system with H and B.

## Known limits

- The macOS build is experimental: it builds and passes validation, but has not been tested with VoiceOver.
- Tested with NVDA in REAPER. Other hosts and screen readers should work but have not been tested.
- If another program keeps the shared settings file locked for more than a second, the theme, language or window size are not saved that time, and a new window opens with the defaults.
- The main fonts have no Cyrillic. With Russian selected, the system substitutes a font, so Russian text looks different from the rest.

## Licence and credits

- **TELESCOPE** is by [OVNI Audio](https://ovniaudio.com), free. Version 0.2.0, on which this fork is based, was published as open source under the **GNU AGPLv3**. The author no longer publishes the source code, so this fork does not receive later changes. The author's site: <https://ovniaudio.com/telescope>.
- Speech and keyboard control, their translations and the Russian translation were added in [fdrv161/periscope](https://github.com/fdrv161/periscope/tree/accessible), under the same licence. The full source of every release is in that repository, at the release tag.
- Third-party notices: [`NOTICE.md`](NOTICE.md).

**How this was made.** The speech and keyboard layer, its translations, the Russian translation, the build scripts and this documentation were written by Claude, an AI model by Anthropic, working in Claude Code at the request of [fdrv161](https://github.com/fdrv161). fdrv161 tested the Windows build by hand with NVDA in REAPER. The macOS build has not been tested by a person.
