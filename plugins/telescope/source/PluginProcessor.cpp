#include "PluginProcessor.h"
#include "data/Rules.h"   // ===== 55: los idiomas de VERDICT (D-50) =====
#include "lenses/Strings.h"   // Periscope: strings::tableFor for the saved language
#include "PluginEditor.h"
#include "lenses/LensIds.h"
#include "lenses/Palettes.h"   // ===== 57b: look::kNumPalettes =====
#include "analysis/modules/Loudness.h"
#include "analysis/modules/Stereo.h"
#include "analysis/modules/StereoBands.h"
#include "data/StreamingTargets.h"
#include "presets/PresetTypes.h"

namespace telescope
{
juce::AudioProcessorValueTreeState::ParameterLayout TelescopeProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // `bypass` lo declara cada plugin (el chasis sólo lo LEE). inGain/output/monoSafe los inyecta la base
    // y quedan FUERA de la UI de TELESCOPE (el host igual los lista: limitación conocida del chasis).
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 }, "Bypass", false));

    // LENTE. Las 13 desde el día uno, en el orden del spec §2: el índice es CONTRATO (estado y presets
    // guardados con una lente tienen que seguir abriendo esa lente cuando estén las 13 construidas).
    juce::StringArray lensNames;
    // Las tres tablas de abajo son `const char*` con UTF-8 crudo: siempre por fromUTF8 (String (const char*)
    // decodifica byte a byte y "Loudness · libre" salía "Loudness Â· libre" en el host; PresetNamesTest.cpp).
    for (const auto* n : kLensNames) lensNames.add (juce::String::fromUTF8 (n));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "lens", 1 },
                                                              "Lens", lensNames, 0));

    // OBJETIVO de plataforma (ver data/StreamingTargets.h y su nota de honestidad).
    juce::StringArray targetNames;
    for (const auto& t : kStreamingTargets) targetNames.add (juce::String::fromUTF8 (t.name));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "target", 1 },
                                                              "Target", targetNames, 0));

    // PRESET de fábrica ("vistas": lente + objetivo), como SUPERNOVA.
    juce::StringArray presetNames;
    for (const auto& p : ovni::presets::factoryPresets()) presetNames.add (juce::String::fromUTF8 (p.name));
    if (presetNames.isEmpty()) presetNames.add ("Default");
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "preset", 1 },
                                                              "Preset", presetNames, 0));

    return layout;
}

TelescopeProcessor::TelescopeProcessor()
    : ovni::PluginProcessorBase ("TELESCOPE", createParameterLayout()),
      bandsFrames (std::make_unique<TripleBuffer<StereoBandsFrame>>()),
      stereoRing (std::make_unique<StereoSpectrogramRing>()),
      cqtFrames (std::make_unique<TripleBuffer<CqtFrame>>())
{
    syncLensSettings();
}

//======================================================================================= settings de lente
// Propiedades del ValueTree, no parámetros (ver la nota del header). El getter lee el árbol; el setter
// escribe el árbol Y empuja al motor. Así el estado guardado y lo que el worker usa nunca divergen.
int  TelescopeProcessor::stereoWindowMs() const
{
    return (int) apvts.state.getProperty (kStereoWindowMs, Stereo::kDefaultWindowMs);
}

void TelescopeProcessor::setStereoWindowMs (int ms)
{
    apvts.state.setProperty (kStereoWindowMs, ms, nullptr);
    analysisThread.setStereoWindowMs (ms);
}

bool TelescopeProcessor::scopePolar() const   { return (bool) apvts.state.getProperty (kScopePolar, false); }
void TelescopeProcessor::setScopePolar (bool on)   { apvts.state.setProperty (kScopePolar, on, nullptr); }

bool TelescopeProcessor::scopeTrigger() const { return (bool) apvts.state.getProperty (kScopeTrigger, true); }
void TelescopeProcessor::setScopeTrigger (bool on) { apvts.state.setProperty (kScopeTrigger, on, nullptr); }

float TelescopeProcessor::clipThresholdDbtp() const
{
    // Periscope: limited like the setter. A NaN from a saved state made the worker clear the clip history on
    // every hop, because NaN never equals the threshold it already has.
    const auto v = (float) (double) apvts.state.getProperty (kClipThreshold, Loudness::kDefaultClipThresholdDbtp);
    if (! std::isfinite (v)) return Loudness::kDefaultClipThresholdDbtp;
    return juce::jlimit (Loudness::kMinClipThresholdDbtp, Loudness::kMaxClipThresholdDbtp, v);
}

void TelescopeProcessor::setClipThresholdDbtp (float dbtp)
{
    const float v = juce::jlimit (Loudness::kMinClipThresholdDbtp, Loudness::kMaxClipThresholdDbtp, dbtp);
    apvts.state.setProperty (kClipThreshold, (double) v, nullptr);
    analysisThread.setClipThresholdDbtp (v);
}

float TelescopeProcessor::psrTargetDb() const
{
    const auto v = (float) (double) apvts.state.getProperty (kPsrTarget, (double) kDefaultPsrTargetDb);
    if (! std::isfinite (v)) return kDefaultPsrTargetDb;   // see clipThresholdDbtp
    return juce::jlimit (kMinPsrTargetDb, kMaxPsrTargetDb, v);
}

void TelescopeProcessor::setPsrTargetDb (float db)
{
    apvts.state.setProperty (kPsrTarget, (double) juce::jlimit (kMinPsrTargetDb, kMaxPsrTargetDb, db), nullptr);
}

// La ventana del estéreo por banda (0.3 / 1 / 3 s) y la fila secundaria de BAND CORRELATION.
int TelescopeProcessor::bandsWindowIndex() const
{
    return juce::jlimit (0, StereoBands::kNumWindowOptions - 1,
                         (int) apvts.state.getProperty (kBandsWindow, StereoBands::kDefaultWindowIndex));
}

void TelescopeProcessor::setBandsWindowIndex (int i)
{
    const int v = juce::jlimit (0, StereoBands::kNumWindowOptions - 1, i);
    apvts.state.setProperty (kBandsWindow, v, nullptr);
    analysisThread.setBandsWindowIndex (v);
}

int  TelescopeProcessor::bandsRow() const { return juce::jlimit (0, 2, (int) apvts.state.getProperty (kBandsRow, 0)); }
void TelescopeProcessor::setBandsRow (int row) { apvts.state.setProperty (kBandsRow, juce::jlimit (0, 2, row), nullptr); }

//------------------------------------------------------------------------------------ settings del CQT
// El decaimiento del peak hold sale de los settings de SPECTRUM: es la misma perilla, y el motor la lee
// de ahí (ver AnalysisThread::cqtSettings). Guardarla dos veces sería garantizar que algún día difieran.
Cqt::Settings TelescopeProcessor::cqtSettings() const
{
    Cqt::Settings d, s;
    s.channel           = (int) apvts.state.getProperty (kCqtChannel, d.channel);
    s.chromaSecIndex    = (int) apvts.state.getProperty (kCqtChromaSec, d.chromaSecIndex);
    s.holdDecayDbPerSec = spectrumSettings().holdDecayDbPerSec;
    s.sanitise();
    return s;
}

void TelescopeProcessor::setCqtSettings (const Cqt::Settings& s)
{
    Cqt::Settings v = s;
    v.sanitise();
    apvts.state.setProperty (kCqtChannel, v.channel, nullptr);
    apvts.state.setProperty (kCqtChromaSec, v.chromaSecIndex, nullptr);
    analysisThread.setCqtSettings (v);
}

//------------------------------------------------------------------ ===== 53: las dos lentes 3D =====
int TelescopeProcessor::waterfallLinesIndex() const
{
    return juce::jlimit (0, kNumWaterfallLineOptions - 1,
                         (int) apvts.state.getProperty (kWaterfallLines, kDefaultWaterfallLinesIndex));
}

void TelescopeProcessor::setWaterfallLinesIndex (int i)
{
    apvts.state.setProperty (kWaterfallLines, juce::jlimit (0, kNumWaterfallLineOptions - 1, i), nullptr);
}

int TelescopeProcessor::waterfallTiltIndex() const
{
    return juce::jlimit (0, kNumWaterfallTiltOptions - 1,
                         (int) apvts.state.getProperty (kWaterfallTilt, kDefaultWaterfallTiltIndex));
}

void TelescopeProcessor::setWaterfallTiltIndex (int i)
{
    apvts.state.setProperty (kWaterfallTilt, juce::jlimit (0, kNumWaterfallTiltOptions - 1, i), nullptr);
}

// La paleta de los mapas de calor (57b). De VISTA: no se le empuja nada al motor.
int TelescopeProcessor::paletteIndex() const
{
    return juce::jlimit (0, look::kNumPalettes - 1,
                         (int) apvts.state.getProperty (kPalette, kDefaultPaletteIndex));
}

void TelescopeProcessor::setPaletteIndex (int i)
{
    apvts.state.setProperty (kPalette, juce::jlimit (0, look::kNumPalettes - 1, i), nullptr);
}

// El suavizado de pantalla de SPECTRUM (57b). De DIBUJO: tampoco se le empuja nada al motor.
int TelescopeProcessor::spectrumSmoothIndex() const
{
    return juce::jlimit (0, kNumSpectrumSmoothOptions - 1,
                         (int) apvts.state.getProperty (kSpectrumSmooth, kDefaultSpectrumSmoothIndex));
}

void TelescopeProcessor::setSpectrumSmoothIndex (int i)
{
    apvts.state.setProperty (kSpectrumSmooth, juce::jlimit (0, kNumSpectrumSmoothOptions - 1, i), nullptr);
}

int TelescopeProcessor::fieldDecayIndex() const
{
    return juce::jlimit (0, Field::kNumDecayOptions - 1,
                         (int) apvts.state.getProperty (kFieldDecay, Field::kDefaultDecayIndex));
}

void TelescopeProcessor::setFieldDecayIndex (int i)
{
    const int v = juce::jlimit (0, Field::kNumDecayOptions - 1, i);
    apvts.state.setProperty (kFieldDecay, v, nullptr);
    analysisThread.setFieldDecayIndex (v);   // éste SÍ es de motor
}

//------------------------------------------------------------------------------------ settings de espectro
Spectrum::Settings TelescopeProcessor::spectrumSettings() const
{
    Spectrum::Settings d;   // los defaults del módulo son los defaults del árbol
    Spectrum::Settings s;
    s.fftOrder          = (int) apvts.state.getProperty (kSpectrumFftOrder, d.fftOrder);
    s.window            = (int) apvts.state.getProperty (kSpectrumWindow, d.window);
    s.overlapIndex      = (int) apvts.state.getProperty (kSpectrumOverlap, d.overlapIndex);
    s.channel           = (int) apvts.state.getProperty (kSpectrumChannel, d.channel);
    s.slopeDbPerOct     = (float) (double) apvts.state.getProperty (kSpectrumSlope, (double) d.slopeDbPerOct);
    s.avgMode           = (int) apvts.state.getProperty (kSpectrumAvgMode, d.avgMode);
    s.avgSeconds        = (float) (double) apvts.state.getProperty (kSpectrumAvgSeconds, (double) d.avgSeconds);
    s.peakHold          = (bool) apvts.state.getProperty (kSpectrumPeakHold, d.peakHold);
    s.holdDecayDbPerSec = (float) (double) apvts.state.getProperty (kSpectrumHoldDecay, (double) d.holdDecayDbPerSec);
    s.bandsMode         = (int) apvts.state.getProperty (kSpectrumBandsMode, d.bandsMode);
    s.rangeDbIndex      = (int) apvts.state.getProperty (kSpectrumRange, d.rangeDbIndex);
    s.historySecIndex   = (int) apvts.state.getProperty (kSpectrogramHistory, d.historySecIndex);
    s.weighting         = (int) apvts.state.getProperty (kSpectrumWeighting, d.weighting);
    s.masking           = (bool) apvts.state.getProperty (kSpectrumMasking, d.masking);
    s.sanitise();
    return s;
}

void TelescopeProcessor::setSpectrumSettings (const Spectrum::Settings& in)
{
    Spectrum::Settings s = in;
    s.sanitise();

    apvts.state.setProperty (kSpectrumFftOrder,  s.fftOrder, nullptr);
    apvts.state.setProperty (kSpectrumWindow,    s.window, nullptr);
    apvts.state.setProperty (kSpectrumOverlap,   s.overlapIndex, nullptr);
    apvts.state.setProperty (kSpectrumChannel,   s.channel, nullptr);
    apvts.state.setProperty (kSpectrumSlope,     (double) s.slopeDbPerOct, nullptr);
    apvts.state.setProperty (kSpectrumAvgMode,   s.avgMode, nullptr);
    apvts.state.setProperty (kSpectrumAvgSeconds, (double) s.avgSeconds, nullptr);
    apvts.state.setProperty (kSpectrumPeakHold,  s.peakHold, nullptr);
    apvts.state.setProperty (kSpectrumHoldDecay, (double) s.holdDecayDbPerSec, nullptr);
    apvts.state.setProperty (kSpectrumBandsMode, s.bandsMode, nullptr);
    apvts.state.setProperty (kSpectrumRange,     s.rangeDbIndex, nullptr);
    apvts.state.setProperty (kSpectrogramHistory, s.historySecIndex, nullptr);
    apvts.state.setProperty (kSpectrumWeighting, s.weighting, nullptr);
    apvts.state.setProperty (kSpectrumMasking,   s.masking, nullptr);

    analysisThread.setSpectrumSettings (s);
}

void TelescopeProcessor::syncLensSettings()
{
    // Escribir los defaults que falten deja el árbol EXPLÍCITO: un estado guardado hoy sigue abriendo
    // igual mañana aunque cambie el default del código.
    if (! apvts.state.hasProperty (kStereoWindowMs)) apvts.state.setProperty (kStereoWindowMs, Stereo::kDefaultWindowMs, nullptr);
    if (! apvts.state.hasProperty (kScopePolar))     apvts.state.setProperty (kScopePolar, false, nullptr);
    if (! apvts.state.hasProperty (kScopeTrigger))   apvts.state.setProperty (kScopeTrigger, true, nullptr);
    if (! apvts.state.hasProperty (kClipThreshold))  apvts.state.setProperty (kClipThreshold, (double) Loudness::kDefaultClipThresholdDbtp, nullptr);
    if (! apvts.state.hasProperty (kBandsWindow))    apvts.state.setProperty (kBandsWindow, StereoBands::kDefaultWindowIndex, nullptr);
    if (! apvts.state.hasProperty (kBandsRow))       apvts.state.setProperty (kBandsRow, 0, nullptr);
    if (! apvts.state.hasProperty (kCqtChannel))    apvts.state.setProperty (kCqtChannel, Cqt::Settings{}.channel, nullptr);
    if (! apvts.state.hasProperty (kCqtChromaSec))  apvts.state.setProperty (kCqtChromaSec, Cqt::Settings{}.chromaSecIndex, nullptr);
    // ===== 53 =====
    if (! apvts.state.hasProperty (kWaterfallLines)) apvts.state.setProperty (kWaterfallLines, kDefaultWaterfallLinesIndex, nullptr);
    if (! apvts.state.hasProperty (kWaterfallTilt))  apvts.state.setProperty (kWaterfallTilt, kDefaultWaterfallTiltIndex, nullptr);
    if (! apvts.state.hasProperty (kFieldDecay))     apvts.state.setProperty (kFieldDecay, Field::kDefaultDecayIndex, nullptr);
    // ===== 57b =====
    if (! apvts.state.hasProperty (kPalette))        apvts.state.setProperty (kPalette, kDefaultPaletteIndex, nullptr);
    if (! apvts.state.hasProperty (kSpectrumSmooth))
        apvts.state.setProperty (kSpectrumSmooth, kDefaultSpectrumSmoothIndex, nullptr);

    analysisThread.setStereoWindowMs (stereoWindowMs());
    analysisThread.setClipThresholdDbtp (clipThresholdDbtp());
    analysisThread.setBandsWindowIndex (bandsWindowIndex());
    analysisThread.setFieldDecayIndex (fieldDecayIndex());   // ===== 53 =====
    setSpectrumSettings (spectrumSettings());   // escribe los defaults que falten Y empuja al motor
    setCqtSettings (cqtSettings());             // (va DESPUÉS: lee holdDecay de los del espectro)

    // ===== 54: TONAL BALANCE · engancha la salida y RE-ANALIZA la referencia del estado =====
    analysisThread.setReferenceOutput (&refFrames);
    analysisThread.setSecondHistory (&seconds);   // ===== 55 =====
    analysisThread.setCorrelationHistory (&corrSeconds);   // Periscope
    syncReference (true);   // Periscope: an undo or a preset does not re-analyse the same file

    // ===== 55: VERDICT =====
    // Periscope: a new instance (or a project saved without a language) starts in the language the user chose
    // last in any instance, kept in the settings file like the theme (ui/ThemePreference.h); English otherwise.
    if (! apvts.state.hasProperty (kVerdictLanguage))
    {
        const auto saved = ThemePreference::getValue (ThemePreference::kLanguageKey, {});
        apvts.state.setProperty (kVerdictLanguage,
                                 strings::tableFor (saved) != nullptr ? saved : juce::String (rules::kDefaultLanguage), nullptr);
    }
    if (! apvts.state.hasProperty (kVerdictMode)) apvts.state.setProperty (kVerdictMode, verdictLive, nullptr);
    syncVerdictFile (true);
}

//------------------------------------------------------------------------------------ 55 · VERDICT
juce::String TelescopeProcessor::verdictLanguage() const
{
    const auto code = apvts.state.getProperty (kVerdictLanguage, juce::String (rules::kDefaultLanguage)).toString();
    // Un estado guardado con un idioma que esta versión no tiene NO deja la lente muda: cae al default.
    return rules::hasLanguage (code.toRawUTF8()) ? code : juce::String (rules::kDefaultLanguage);
}

void TelescopeProcessor::setVerdictLanguage (const juce::String& code)
{
    if (! rules::hasLanguage (code.toRawUTF8())) return;
    apvts.state.setProperty (kVerdictLanguage, code, nullptr);
}

int  TelescopeProcessor::verdictMode() const
{
    return juce::jlimit ((int) verdictLive, (int) verdictFile, (int) apvts.state.getProperty (kVerdictMode, verdictLive));
}

void TelescopeProcessor::setVerdictMode (int mode)
{
    apvts.state.setProperty (kVerdictMode, juce::jlimit ((int) verdictLive, (int) verdictFile, mode), nullptr);
}

void TelescopeProcessor::loadVerdictFile (const juce::File& f)
{
    apvts.state.setProperty (kVerdictFilePath, f.getFullPathName(), nullptr);
    setVerdictMode (verdictFile);   // cargar un archivo ES pedir el modo archivo
    syncVerdictFile();
}

void TelescopeProcessor::clearVerdictFile()
{
    apvts.state.setProperty (kVerdictFilePath, juce::String(), nullptr);
    setVerdictMode (verdictLive);
    verdictStartedPath.clear();
    if (verdictAnalyzer != nullptr) verdictAnalyzer->cancel();
}

void TelescopeProcessor::syncVerdictFile (bool onlyIfChanged)
{
    if (verdictAnalyzer == nullptr) verdictAnalyzer = std::make_unique<FileAnalyzer>();

    const auto path = verdictFilePath();
    if (path.isEmpty()) { verdictStartedPath.clear(); return; }

    // Periscope: REAPER restores the state on every undo; the same file, still running or analysed to the end,
    // keeps its result. This also keeps the message thread off the disk (an offline network share) on undo.
    if (onlyIfChanged && path == verdictStartedPath
        && (verdictAnalyzer->busy() || verdictAnalyzer->result().valid))
        return;

    verdictStartedPath.clear();
    const juce::File f (path);
    if (f.existsAsFile())
    {
        verdictStartedPath = path;
        verdictAnalyzer->start (f);
    }
}

juce::String TelescopeProcessor::verdictFileName() const
{
    return verdictAnalyzer != nullptr ? verdictAnalyzer->currentName() : juce::String();
}

// Mismo criterio que referenceError(): el error es del archivo QUE ESTÁ CARGADO. Sin path no hay archivo.
juce::String TelescopeProcessor::verdictFileError() const
{
    if (verdictAnalyzer == nullptr || verdictAnalyzer->busy() || verdictFilePath().isEmpty()) return {};
    return verdictAnalyzer->result().error;
}

const FileAnalysis TelescopeProcessor::verdictAnalysis() const
{
    return verdictAnalyzer != nullptr ? verdictAnalyzer->result() : FileAnalysis{};
}

void TelescopeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    ovni::PluginProcessorBase::setStateInformation (data, sizeInBytes);
    syncLensSettings();   // replaceState cambió el árbol entero: el motor tiene que enterarse
}

TelescopeProcessor::~TelescopeProcessor()
{
    // Accessible fork: the file analyzers go first, explicitly. The reference analyzer's onFinished pushes
    // its result into analysisThread from its own thread, so it must be fully stopped before the engine
    // is (implicit member destruction would do it the other way round).
    verdictAnalyzer.reset();
    fileAnalyzer.reset();
    analysisThread.stop();
}

void TelescopeProcessor::prepareEngine (const juce::dsp::ProcessSpec& spec)
{
    // Periscope: at least 4096, so a host that announces 0 still gets mono measured (processAudio pushes in
    // pieces of this size).
    monoScratch.assign ((size_t) juce::jmax (4096u, spec.maximumBlockSize), 0.0f);
    analysisThread.prepare (spec.sampleRate);
}

void TelescopeProcessor::releaseResources()
{
    analysisThread.stop();
}

// TAP READ-ONLY. Todo lo de acá es RT-safe: punteros de lectura, un push lock-free drop-on-full y, en el
// caso mono, una copia a un scratch ya dimensionado en prepareEngine. Sin new, sin locks, sin I/O.
// Y sobre todo: NO se escribe el buffer. Es el contrato del plugin.
void TelescopeProcessor::processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int n   = buffer.getNumSamples();
    const int nch = buffer.getNumChannels();
    if (n <= 0 || nch <= 0) return;

    // Periscope: decided by the INPUT channels. With mono in and stereo out the buffer still has two channels,
    // and the base class has zeroed the second one, so measuring channels 0 and 1 gave "left and silence".
    if (nch >= 2 && getTotalNumInputChannels() >= 2)
    {
        bus.push (buffer.getReadPointer (0), buffer.getReadPointer (1), n);
    }
    else if (! monoScratch.empty())
    {
        // Fuente mono: se mide el mismo canal en L y R (correlación +1, ancho 0 — que es la verdad).
        // Periscope: a block larger than announced is pushed in pieces of the scratch size instead of dropped.
        const float* m = buffer.getReadPointer (0);
        const int step = (int) monoScratch.size();
        for (int pos = 0; pos < n; pos += step)
        {
            const int len = juce::jmin (step, n - pos);
            std::copy (m + pos, m + pos + len, monoScratch.begin());
            bus.push (m + pos, monoScratch.data(), len);
        }
    }
}

juce::AudioProcessorEditor* TelescopeProcessor::createEditor()
{
    return new TelescopeEditor (*this);
}

// ========================================================================================================
// ===== 54: TONAL BALANCE · la referencia =====
//
// El path persiste; los NÚMEROS no. Al abrir una sesión guardada, si el archivo sigue estando se vuelve a
// analizar (tarda un cuarto de segundo por minuto de audio: no hay nada que ahorrar guardando la curva) y
// si no está, la lente lo dice con el nombre. Guardar 30 números en el preset y dibujarlos como si fueran
// el archivo sería mostrar una referencia que ya no existe, sin que nadie pueda notarlo.
// ========================================================================================================
void TelescopeProcessor::syncReference (bool onlyIfChanged)
{
    if (fileAnalyzer == nullptr)
    {
        fileAnalyzer = std::make_unique<FileAnalyzer>();
        // El resultado se le empuja al motor DESDE EL THREAD DEL ANALIZADOR: el módulo Reference lo
        // protege con su propia CriticalSection, así que no hace falta rebotar por el message thread
        // (que puede no estar corriendo: en un host sin ventana abierta nadie bombea mensajes).
        // Accessible fork: capture the analyzer itself, not the unique_ptr. During reset() the pointer is
        // already null while ~FileAnalyzer waits for this callback; the analyzer is alive until it returns.
        fileAnalyzer->onFinished = [this, fa = fileAnalyzer.get()] { analysisThread.setReference (fa->result()); };
    }

    const auto path = referencePath();
    if (path.isEmpty()) { refStartedKey.clear(); refMissing.clear(); analysisThread.clearReference(); return; }

    // F4 (T6): el tramo, si el estado pide uno. Sin tramo, la llamada es la de siempre (el archivo entero).
    double from = 0.0, to = 0.0;
    const bool ranged = referenceRange (from, to);

    // Periscope: REAPER restores the state on every undo. The same file and range, still running or analysed
    // to the end, keeps its result: no new analysis, no "analysing" in the lens, no disk access on this thread.
    // The engine still has that reference, because only clearing it (which empties the key) removes it.
    const auto key = path + (ranged ? "|" + juce::String (from, 6) + "|" + juce::String (to, 6) : juce::String());
    if (onlyIfChanged && key == refStartedKey && refMissing.isEmpty()
        && (fileAnalyzer->busy() || fileAnalyzer->result().valid))
        return;

    refStartedKey.clear();
    const juce::File f (path);
    if (! f.existsAsFile())
    {
        refMissing = f.getFileName();
        analysisThread.clearReference();
        return;
    }

    refMissing.clear();
    refStartedKey = key;
    if (ranged) fileAnalyzer->start (f, from, to);
    else        fileAnalyzer->start (f);
}

void TelescopeProcessor::loadReference (const juce::File& f)
{
    apvts.state.setProperty (kRefPath, f.getFullPathName(), nullptr);
    apvts.state.removeProperty (kRefFrom, nullptr);   // F4 (T6): el tramo era del archivo anterior
    apvts.state.removeProperty (kRefTo, nullptr);
    syncReference();
}

// ===== F4 (T6): el tramo de la referencia (ver el header) =====
void TelescopeProcessor::setReferenceRange (double fromS, double toS)
{
    if (! std::isfinite (fromS) || ! std::isfinite (toS)) return;
    if (toS < fromS) std::swap (fromS, toS);
    fromS = juce::jmax (0.0, fromS);
    if (toS - fromS < kMinReferenceSpanS) return;   // un clic suelto no es un tramo
    apvts.state.setProperty (kRefFrom, fromS, nullptr);
    apvts.state.setProperty (kRefTo,   toS,   nullptr);
    syncReference();
}

void TelescopeProcessor::clearReferenceRange()
{
    if (! apvts.state.hasProperty (kRefFrom) && ! apvts.state.hasProperty (kRefTo)) return;
    apvts.state.removeProperty (kRefFrom, nullptr);
    apvts.state.removeProperty (kRefTo, nullptr);
    syncReference();
}

bool TelescopeProcessor::referenceRange (double& fromS, double& toS) const
{
    if (! apvts.state.hasProperty (kRefFrom) || ! apvts.state.hasProperty (kRefTo)) return false;
    fromS = (double) apvts.state.getProperty (kRefFrom);
    toS   = (double) apvts.state.getProperty (kRefTo);
    return std::isfinite (fromS) && std::isfinite (toS) && toS - fromS >= kMinReferenceSpanS;
}

FileAnalyzer::Span TelescopeProcessor::referenceSpan() const
{
    return fileAnalyzer != nullptr ? fileAnalyzer->resultSpan() : FileAnalyzer::Span{};
}

void TelescopeProcessor::clearReference()
{
    apvts.state.setProperty (kRefPath, juce::String(), nullptr);
    apvts.state.removeProperty (kRefFrom, nullptr);   // F4 (T6)
    apvts.state.removeProperty (kRefTo, nullptr);
    refMissing.clear();
    refStartedKey.clear();
    if (fileAnalyzer != nullptr) fileAnalyzer->cancel();
    analysisThread.clearReference();
}

juce::String TelescopeProcessor::referenceName() const
{
    if (refMissing.isNotEmpty()) return refMissing;
    if (fileAnalyzer == nullptr) return {};
    return fileAnalyzer->currentName();
}

// EL ERROR Y EL AVISO SON DE LA REFERENCIA QUE ESTÁ CARGADA, no del último archivo que pasó por el
// analizador (LOW 3 del revisor del 54). `FileAnalyzer::result()` guarda su resultado hasta el próximo
// análisis, así que después de QUITAR la referencia la lente seguía mostrando el aviso de un archivo que
// ya no está — un cartel verdadero sobre algo que dejó de existir es un cartel falso. Sin path en el
// estado no hay referencia, y sin referencia no hay nada que advertir.
juce::String TelescopeProcessor::referenceError() const
{
    if (fileAnalyzer == nullptr || fileAnalyzer->busy() || referencePath().isEmpty()) return {};
    return fileAnalyzer->result().error;
}

juce::String TelescopeProcessor::referenceWarning() const
{
    if (fileAnalyzer == nullptr || fileAnalyzer->busy() || referencePath().isEmpty()) return {};
    return fileAnalyzer->result().warning;
}

const FileAnalysis TelescopeProcessor::referenceAnalysis() const
{
    return fileAnalyzer != nullptr ? fileAnalyzer->result() : FileAnalysis{};
}
}

// Entry point de JUCE.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new telescope::TelescopeProcessor(); }
