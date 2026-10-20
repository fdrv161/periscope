**Periscope** is the TELESCOPE 0.2.0 audio analyser by OVNI Audio with speech and full keyboard control.

## Download

- **Windows**: `Periscope-windows-vst3.zip`, VST3 for Windows 10 and 11, 64-bit. Extract it and copy the `Periscope.vst3` folder into `C:\Program Files\Common Files\VST3` (or another folder your DAW scans), replacing the old one, then rescan plugins. Projects and settings from earlier versions keep working.
- **macOS, experimental**: `Periscope-macos.zip`, VST3 and AU, Apple Silicon and Intel. Speech has only been tested with NVDA on Windows. The build is not notarized: see README.txt inside or the manual for the one Terminal command it needs.

Each ZIP contains the manual in English, Russian and Spanish.

## What's new

- **Shape and tilt against the reference** (Tonal balance, Summary): a first glance at how your mix differs from the reference, the stretches that differ by 3 dB or more and the overall tilt. The Q key on the shape row gives the EQ bell (centre and Q) or shelf that would match a stretch. Before your mix plays, the bands say "programme not measured yet" with the reference level.
- **Third-octave and Bark bars read the true band level** (Spectrum). A sine at -12 dBFS now reads -12 dB in its band with every window; before, the bars read 1.8 to 3 dB high and moved when you changed the window. Bass bands at small FFT sizes read noise right; a band narrower than about three FFT bins says "limited by the FFT size".
- **Chromagram and key** (CQT, Spiral): a note no longer leaks into the class a semitone below, so recordings tuned a little flat (for example A = 432 Hz) land on the right notes; the chromagram counts 40 Hz to 5 kHz, so cymbals and sibilants no longer blur the key.
- **Spectrum**: the Q answer starts with the frequency and says "limited by the FFT size" in the bass; bass peaks are found with a small FFT; in bar mode the probe also says the bar; empty bands say "not measurable at this FFT size".
- **Speech follows the picture**: the Waterfall probe reads the drawn line, Scope Picture says "no signal", "one channel only" or "leaning left/right", CQT names notes only above the plot floor, Field says "too quiet to show", Verdict has a Summary and says what it compares against.

## Fixed

- Integrated loudness and loudness range no longer get slower the longer the plugin runs without a reset (for example in REAPER's Monitor FX): the same numbers, bit for bit, at almost constant cost.
- A reference file with a Cyrillic name is drawn correctly in Tonal balance.
- Smaller findings of the review after 1.3.0.

Tested with NVDA in REAPER on Windows.

## Manual

[MANUAL.md](https://github.com/fdrv161/periscope/blob/periscope-1.4.0/MANUAL.md), also in [Russian](https://github.com/fdrv161/periscope/blob/periscope-1.4.0/MANUAL.ru.md) and [Spanish](https://github.com/fdrv161/periscope/blob/periscope-1.4.0/MANUAL.es.md): installing, first steps, all keys, and every lens explained.

## Licence and source

GNU AGPLv3. TELESCOPE is by [OVNI Audio](https://ovniaudio.com/telescope); Periscope is based on its AGPLv3 release 0.2.0. The full source of this release is at the tag [`periscope-1.4.0`](https://github.com/fdrv161/periscope/tree/periscope-1.4.0).

**How this was made.** The speech and keyboard layer, its translations, the Russian translation, the fixes, the new features, the build scripts and this documentation were written by Claude, an AI model by Anthropic, working in Claude Code at the request of [fdrv161](https://github.com/fdrv161). fdrv161 tested the Windows build by hand with NVDA in REAPER. The macOS build has not been tested by a person.

`SHA256SUMS.txt` holds the checksums of the ZIPs.
