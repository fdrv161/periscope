**Periscope** is the TELESCOPE 0.2.0 audio analyser by OVNI Audio with speech and full keyboard control.

## Download

- **Windows**: `Periscope-windows-vst3.zip`, VST3 for Windows 10 and 11, 64-bit. Extract it and copy the `Periscope.vst3` folder into `C:\Program Files\Common Files\VST3` (or another folder your DAW scans), replacing the old one, then rescan plugins. Projects and settings from earlier versions keep working.
- **macOS, experimental**: `Periscope-macos.zip`, VST3 and AU, Apple Silicon and Intel. Speech has only been tested with NVDA on Windows. The build is not notarized: see README.txt inside or the manual for the one Terminal command it needs.

Each ZIP contains the manual in English, Russian and Spanish.

## What's new

Ideas from reference-comparison tools such as ADPTR Metric AB, as measurements only: Periscope still never changes the sound.

- **Comparison with the reference** (Tonal balance, a new readings group after the summary). The Loudness row says how much to turn the reference down or up so that both play equally loud, for example "reference louder by 2.6 dB, turn it down by 2.6 dB; your mix -12.4 LUFS, reference -9.8 LUFS". Below it: loudness range, PLR and true peak max, yours and the reference's. Your mix counts from the last reset; the reference is the whole file or its section.
- **PSR target** (Dynamics). A new setting, 3 to 20 dB, default 8, kept in the project. The PSR row says "below target" where the bar turns amber, and a new row says how much of the analysed time (up to the last 10 minutes) PSR stayed below the target. On screen, the reference line on the PSR bar follows the target.
- **Crest factor** (Dynamics): sample peak over RMS since the reset, per channel, counted the AES17 way, so a sine reads 0 dB, as in dpMeter and most meters.
- **Correlation history** (Scope, after Picture): the correlation over the whole time since the reset, up to 10 minutes, recorded whatever lens is open: how long it was below zero, how long below 0.5, and the lowest value.
- Spoken help (F1), speech in all seven languages and the three manuals describe all of them.

## Fixed

- A review of the new code found nothing serious; its smaller findings are fixed: a channel that is silent while the other plays now shows as correlation 0 instead of being skipped, broken input values (infinity, NaN) can no longer spoil the crest factor until a reset, and the engine's outputs are wired once instead of on every state restore.

Tested with NVDA in REAPER on Windows.

## Manual

[MANUAL.md](https://github.com/fdrv161/periscope/blob/periscope-1.3.0/MANUAL.md), also in [Russian](https://github.com/fdrv161/periscope/blob/periscope-1.3.0/MANUAL.ru.md) and [Spanish](https://github.com/fdrv161/periscope/blob/periscope-1.3.0/MANUAL.es.md): installing, first steps, all keys, and every lens explained.

## Licence and source

GNU AGPLv3. TELESCOPE is by [OVNI Audio](https://ovniaudio.com/telescope); Periscope is based on its AGPLv3 release 0.2.0. The full source of this release is at the tag [`periscope-1.3.0`](https://github.com/fdrv161/periscope/tree/periscope-1.3.0).

**How this was made.** The speech and keyboard layer, its translations, the Russian translation, the fixes, the new features, the build scripts and this documentation were written by Claude, an AI model by Anthropic, working in Claude Code at the request of [fdrv161](https://github.com/fdrv161). fdrv161 tested the Windows build by hand with NVDA in REAPER. The macOS build has not been tested by a person.

`SHA256SUMS.txt` holds the checksums of the ZIPs.
