**Periscope** is the TELESCOPE 0.2.0 audio analyser by OVNI Audio with speech and full keyboard control.

## Download

- **Windows**: `Periscope-windows-vst3.zip`, VST3 for Windows 10 and 11, 64-bit. Extract it and copy the `Periscope.vst3` folder into `C:\Program Files\Common Files\VST3` (or another folder your DAW scans), replacing the old one, then rescan plugins. Projects and settings from earlier versions keep working.
- **macOS, experimental**: `Periscope-macos.zip`, VST3 and AU, Apple Silicon and Intel. Speech has only been tested with NVDA on Windows. The build is not notarized: see README.txt inside or the manual for the one Terminal command it needs.

Each ZIP contains the manual in English, Russian and Spanish.

## Fixed

- More stable when left running for days, for example in REAPER's Monitoring FX: memory no longer grows over long sessions, and a broken plugin earlier in the chain can no longer freeze the readings until Reset.
- A possible REAPER crash with NVDA running is fixed.
- Periscope never changes the sound passing through it.

Tested with NVDA in REAPER on Windows.

## Manual

[MANUAL.md](https://github.com/fdrv161/periscope/blob/periscope-1.4.2/MANUAL.md), also in [Russian](https://github.com/fdrv161/periscope/blob/periscope-1.4.2/MANUAL.ru.md) and [Spanish](https://github.com/fdrv161/periscope/blob/periscope-1.4.2/MANUAL.es.md): installing, first steps, all keys, and every lens explained.

## Licence and source

GNU AGPLv3. TELESCOPE is by [OVNI Audio](https://ovniaudio.com/telescope); Periscope is based on its AGPLv3 release 0.2.0. The full source of this release is at the tag [`periscope-1.4.2`](https://github.com/fdrv161/periscope/tree/periscope-1.4.2).

**How this was made.** The speech and keyboard layer, its translations, the Russian translation, the fixes, the new features, the build scripts and this documentation were written by Claude, an AI model by Anthropic, working in Claude Code at the request of [fdrv161](https://github.com/fdrv161). fdrv161 tested the Windows build by hand with NVDA in REAPER. The macOS build has not been tested by a person.

`SHA256SUMS.txt` holds the checksums of the ZIPs.
