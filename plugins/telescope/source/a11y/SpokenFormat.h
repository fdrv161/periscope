#pragma once
#include <algorithm>
#include <cmath>
#include <juce_core/juce_core.h>
#include "a11y/A11yStrings.h"
#include "lenses/NoteName.h"

// ========================================================================================================
// Numbers for speech: short, no trailing zeros, units as words that synthesizers read the same way.
// ASCII only in the English templates: symbols such as the middle dot, cent or infinity sign are read differently by synthesizers.
// Words and word order come from the translation catalog (A11yStrings.h), in the language chosen in the plugin.
// ========================================================================================================
namespace telescope::spoken
{
using a11ytext::T;
using a11ytext::TF;

// Number without trailing zeros: 1.50 -> "1.5", 2.00 -> "2".
inline juce::String trimmed (double v, int decimals)
{
    auto s = juce::String (v, decimals);
    if (s.containsChar ('.'))
    {
        s = s.trimCharactersAtEnd ("0");
        if (s.endsWithChar ('.')) s = s.dropLastCharacters (1);
    }
    return s == "-0" ? juce::String ("0") : s;
}

// Frequency: "61.5 Hz", "440 Hz", "1.25 kHz", "12.5 kHz".
inline juce::String hz (double f)
{
    if (f >= 1000.0) return TF ("{0} kHz", { trimmed (f / 1000.0, f >= 10000.0 ? 1 : 2) });
    if (f >= 100.0)  return TF ("{0} Hz", { juce::String (juce::roundToInt (f)) });
    return TF ("{0} Hz", { trimmed (f, 1) });
}

// Seconds as a setting: "0.5 seconds".
inline juce::String seconds (double s) { return TF ("{0} seconds", { trimmed (s, 1) }); }

// Time: "45 seconds", "1 minute 5.5 seconds", "2 minutes". Instead of "1:05": synthesizers read the colon
// differently.
inline juce::String duration (double secs)
{
    secs = juce::jmax (0.0, std::round (secs * 10.0) / 10.0);
    const int minutes = (int) (secs / 60.0);
    const double rest = secs - 60.0 * minutes;
    juce::StringArray parts;
    if (minutes > 0) parts.add (TF (minutes == 1 ? "{0} minute" : "{0} minutes", { juce::String (minutes) }));
    if (rest >= 0.05 || minutes == 0)
    {
        const auto r = trimmed (rest, 1);
        parts.add (TF (r == "1" ? "{0} second" : "{0} seconds", { r }));
    }
    return parts.joinIntoString (" ");
}

// Decibels: "-18.3 dB".
inline juce::String dB (const juce::String& number) { return TF ("{0} dB", { number }); }
inline juce::String dB (double v, int decimals = 1) { return dB (trimmed (v, decimals)); }

// Level: "-18.3 dB". Below the plot floor: "below -90 dB" (a sighted user sees nothing there either).
inline juce::String db (float v, int floorDb)
{
    if (! std::isfinite (v) || v < (float) -floorDb) return TF ("below {0} dB", { "-" + juce::String (floorDb) });
    return dB (v);
}

// Periscope: moved here from SpectrumLens.cpp so Tonal balance speaks ranges the same way.
// Edges of a stretch or a hump are estimates: two significant digits are what a listener needs (437 Hz -> 440 Hz).
inline double roundHz (double f)
{
    if (f <= 0.0) return f;
    const double step = std::pow (10.0, std::floor (std::log10 (f)) - 1.0);
    return std::round (f / step) * step;
}

// The number of hz() without its unit, for the first end of a range in the same unit.
inline juce::String hzNumber (double f)
{
    if (f >= 1000.0) return trimmed (f / 1000.0, f >= 10000.0 ? 1 : 2);
    if (f >= 100.0)  return juce::String (juce::roundToInt (f));
    return trimmed (f, 1);
}

// "85 to 400 Hz", "450 Hz to 2 kHz": the unit once when both ends share it.
inline juce::String hzRange (double lo, double hi)
{
    lo = roundHz (lo);
    hi = roundHz (hi);
    if (hi <= lo) return hz (lo);
    const bool sameUnit = (lo >= 1000.0) == (hi >= 1000.0);
    return TF ("{0} to {1}", { sameUnit ? hzNumber (lo) : hz (lo), hz (hi) });
}

// Signed value: "+3.2 dB", "-1 dB", "0 dB".
inline juce::String signedDb (float v)
{
    const auto t = trimmed (v, 1);
    return dB ((t != "0" && v > 0.0f ? "+" : "") + t);
}

inline juce::String percent (int v) { return TF ("{0} percent", { juce::String (v) }); }

// On-screen text -> speech text. The middle dot separator becomes a comma, dashes and the minus sign become a hyphen,
// a line break becomes a period. Otherwise NVDA says "middle dot" or drops the minus sign of a number.
inline juce::String clean (juce::String t)
{
    t = t.replace (juce::String::fromUTF8 ("\xc2\xb7"), ",")          // ·
         .replace (juce::String::fromUTF8 ("\xe2\x80\x94"), " - ")    // —
         .replace (juce::String::fromUTF8 ("\xe2\x80\x93"), " - ")    // –
         .replace (juce::String::fromUTF8 ("\xe2\x88\x92"), "-")      // −
         .replace (juce::String::fromUTF8 ("\xc2\xa0"), " ")          // non-breaking space
         .replace (juce::String::fromUTF8 ("\xe2\x88\x9e"), T ("infinite"))
         .replace ("\n", ". ");
    while (t.contains ("  ")) t = t.replace ("  ", " ");
    return t.replace (" ,", ",").replace (",,", ",").trim();
}

// Channel balance (positive: right is louder): "centred", "1.2 dB louder on the right".
inline juce::String balance (float db)
{
    if (std::abs (db) < 0.05f) return T ("centred");
    return TF (db > 0.0f ? "{0} dB louder on the right" : "{0} dB louder on the left", { trimmed (std::abs (db), 1) });
}

// Pan from -1 (left) to +1 (right): "centre", "40 percent left".
inline juce::String pan (float p)
{
    const int pct = juce::roundToInt (std::abs (p) * 100.0f);
    if (pct < 5) return T ("centre");
    return TF (p < 0.0f ? "{0} percent left" : "{0} percent right", { juce::String (pct) });
}

// Correlation: the number and its meaning in words (on screen it is a colour and the needle position).
inline juce::String correlation (float c)
{
    const auto n = trimmed (c, 2);
    if (c < 0.0f)  return TF ("{0}, out of phase", { n });
    if (c > 0.9f)  return TF ("{0}, nearly mono", { n });
    return n;
}

// Palette name in normal case (proper names, not translated).
inline juce::String palette (int index)
{
    static const char* const names[] = { "Ovni", "Inferno", "Viridis", "Spectrum" };
    return names[juce::jlimit (0, 3, index)];
}

// Pitch class (0 = C) as musicians of this language call it: letters, do-re-mi or German
// letters with H and B. Sharp as a word: synthesizers read # as "hash" or "number".
inline juce::String pitchClass (int pc)
{
    static const char* const names[a11ytext::kNumLangs][12] = {
        { "C", "C sharp", "D", "D sharp", "E", "F", "F sharp", "G", "G sharp", "A", "A sharp", "B" },
        { "Do", "Do sostenido", "Re", "Re sostenido", "Mi", "Fa", "Fa sostenido", "Sol", "Sol sostenido", "La", "La sostenido", "Si" },
        { "D\xc3\xb3", "D\xc3\xb3 sustenido", "R\xc3\xa9", "R\xc3\xa9 sustenido", "Mi", "F\xc3\xa1", "F\xc3\xa1 sustenido", "Sol", "Sol sustenido", "L\xc3\xa1", "L\xc3\xa1 sustenido", "Si" },
        { "Do", "Do di\xc3\xa8se", "R\xc3\xa9", "R\xc3\xa9 di\xc3\xa8se", "Mi", "Fa", "Fa di\xc3\xa8se", "Sol", "Sol di\xc3\xa8se", "La", "La di\xc3\xa8se", "Si" },
        { "C", "Cis", "D", "Dis", "E", "F", "Fis", "G", "Gis", "A", "B", "H" },
        { "Do", "Do diesis", "Re", "Re diesis", "Mi", "Fa", "Fa diesis", "Sol", "Sol diesis", "La", "La diesis", "Si" },
        { "\xd0\xb4\xd0\xbe", "\xd0\xb4\xd0\xbe \xd0\xb4\xd0\xb8\xd0\xb5\xd0\xb7", "\xd1\x80\xd0\xb5", "\xd1\x80\xd0\xb5 \xd0\xb4\xd0\xb8\xd0\xb5\xd0\xb7",
          "\xd0\xbc\xd0\xb8", "\xd1\x84\xd0\xb0", "\xd1\x84\xd0\xb0 \xd0\xb4\xd0\xb8\xd0\xb5\xd0\xb7", "\xd1\x81\xd0\xbe\xd0\xbb\xd1\x8c",
          "\xd1\x81\xd0\xbe\xd0\xbb\xd1\x8c \xd0\xb4\xd0\xb8\xd0\xb5\xd0\xb7", "\xd0\xbb\xd1\x8f", "\xd0\xbb\xd1\x8f \xd0\xb4\xd0\xb8\xd0\xb5\xd0\xb7", "\xd1\x81\xd0\xb8" },
    };
    return juce::String::fromUTF8 (names[(int) a11ytext::language()][((pc % 12) + 12) % 12]);
}

inline juce::String key (int tonic, int mode)
{
    if (tonic < 0 || tonic > 11 || mode < 0) return {};
    return TF (mode == 1 ? "{0} minor" : "{0} major", { pitchClass (tonic) });
}

// Key with confidence: "A minor, confidence 0.82, best 64 percent of the time".
inline juce::String keyReading (int tonic, int mode, float confidence, float timeFraction)
{
    const auto k = key (tonic, mode);
    if (k.isEmpty()) return T ("not estimated yet");
    return TF ("{0}, confidence {1}, best {2} percent of the time",
               { k, trimmed (confidence, 2), juce::String (juce::roundToInt (timeFraction * 100.0f)) });
}

// The three strongest pitch classes of the chromagram (12 values, 0 = C): "A, E, C sharp".
inline juce::String strongestClasses (const float* chroma12)
{
    int order[12];
    for (int i = 0; i < 12; ++i) order[i] = i;
    std::sort (order, order + 12, [chroma12] (int a, int b) { return chroma12[a] > chroma12[b]; });
    if (! (chroma12[order[0]] > 1.0e-6f)) return T ("no signal");
    juce::StringArray names;
    for (int i = 0; i < 3; ++i)
        if (chroma12[order[i]] >= 0.25f * chroma12[order[0]]) names.add (pitchClass (order[i]));
    return names.joinIntoString (", ");
}

// Note with octave: "C sharp 3", "A 4" (scientific numbering, C4 is middle C).
inline juce::String note (const NoteReadout& n)
{
    const int pc  = ((n.midi % 12) + 12) % 12;
    const int oct = (int) std::floor ((double) n.midi / 12.0) - 1;
    return pitchClass (pc) + " " + juce::String (oct);
}

// Note with deviation: "A 4 plus 3 cents". Zero cents are not spoken.
inline juce::String noteWithCents (const NoteReadout& n)
{
    if (n.cents == 0) return note (n);
    return TF (n.cents > 0 ? "{0} plus {1} cents" : "{0} minus {1} cents", { note (n), juce::String (std::abs (n.cents)) });
}
}
