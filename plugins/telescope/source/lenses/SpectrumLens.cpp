#include "lenses/SpectrumLens.h"
#include "lenses/Look.h"
#include "PluginProcessor.h"
#include "lenses/LensReadout.h"
#include "a11y/A11yWidgets.h"
#include "a11y/SpokenFormat.h"
#include "ui-kit/Fonts.h"
#include "ui-kit/Theme.h"
#include <algorithm>
#include <cmath>

namespace telescope
{
namespace
{
namespace th = telescope::look::tint;   // F2: el tema vigente (Look.h)

// Las etiquetas de frecuencia que sí se leen. La rejilla tiene los 30 centros de ⅓ de octava; rotularlos
// todos sería una pared de números.
constexpr double kLabelledHz[] = { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 };

juce::String shortHz (double hz)
{
    if (hz >= 1000.0)
    {
        const double k = hz / 1000.0;
        return juce::String (k, k < 10.0 && std::abs (k - std::round (k)) > 0.01 ? 1 : 0) + "k";
    }
    return juce::String ((int) std::lround (hz));
}

const char* windowLabel (int w)
{
    return w == Spectrum::hann ? "HANN" : (w == Spectrum::blackmanHarris4 ? "BH4" : "KAISER");
}

const char* channelLabel (int c)
{
    switch (c)
    {
        case Spectrum::left:  return "L";
        case Spectrum::right: return "R";
        case Spectrum::mid:   return "M";
        case Spectrum::side:  return "S";
        default:              return "L+R";
    }
}

const char* bandsLabel (int b)
{
    return b == Spectrum::bandsThird ? "1/3 OCT" : (b == Spectrum::bandsBark ? "BARK" : "FFT");
}
}

const juce::ValueTree& SpectrumLens::stateTree() const { return processor.apvts.state; }

SpectrumLens::SpectrumLens (TelescopeProcessor& p) : Lens (30), processor (p)
{
    setSettleHold (30);
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

//======================================================================================== geometría
SpectrumLens::Zones SpectrumLens::zonesFor (int w, int h) const
{
    Zones z;
    auto body = juce::Rectangle<int> (0, 0, w, h).reduced (th::padIn);

    // Dos filas de controles: cinco arriba (lo que define el ANÁLISIS) y cuatro abajo (lo que define la
    // PRESENTACIÓN). Agruparlos por lo que hacen es más rápido de leer que nueve botones en fila.
    const int rowH = juce::jlimit (22, 30, h / 22);
    z.footer = body.removeFromBottom (rowH * 2 + 6);
    body.removeFromBottom (th::padIn / 2);

    auto foot = z.footer;
    auto top  = foot.removeFromTop (rowH);
    foot.removeFromTop (6);
    auto bottom = foot.removeFromTop (rowH);

    const auto share = [] (juce::Rectangle<int>& row, int n, juce::Rectangle<int>* dst)
    {
        const int gap = 6;
        const int bw  = (row.getWidth() - gap * (n - 1)) / n;
        for (int i = 0; i < n; ++i)
        {
            dst[i] = row.removeFromLeft (bw);
            if (i < n - 1) row.removeFromLeft (gap);
        }
    };
    juce::Rectangle<int> analysis[6], display[6];
    share (top, 6, analysis);   // Periscope: + Masking
    share (bottom, 6, display);   // 57b: el quinto es el suavizado de pantalla · Periscope: + Weighting

    z.button[fftSize]   = analysis[0];
    z.button[window]    = analysis[1];
    z.button[overlap]   = analysis[2];
    z.button[channel]   = analysis[3];
    z.button[bandsMode] = analysis[4];
    z.button[maskCtl]   = analysis[5];
    z.button[slope]       = display[0];
    z.button[weightCurve] = display[1];
    z.button[average]     = display[2];
    z.button[hold]        = display[3];
    z.button[range]       = display[4];
    z.button[smooth]      = display[5];

    z.freqAxis = body.removeFromBottom (kAxisH);
    z.dbScale  = body.removeFromLeft (kScaleW);
    z.plot     = body;
    z.freqAxis = z.freqAxis.withLeft (z.plot.getX()).withRight (z.plot.getRight());
    return z;
}

void SpectrumLens::resized()
{
    zones = zonesFor (getWidth(), getHeight());
    const auto n = (size_t) juce::jmax (0, zones.plot.getWidth());
    for (int s = 0; s < SpectrumFrame::kMaxSpectra; ++s)
    {
        colDb[s].assign (n, SpectrumFrame::kFloorDb);
        colHold[s].assign (n, SpectrumFrame::kFloorDb);
        dispDb[s].assign (n, SpectrumFrame::kFloorDb);
    }
    Lens::resized();
}

//======================================================================================== ejes
double SpectrumLens::freqAtX (int x) const
{
    if (zones.plot.getWidth() <= 1) return kMinHz;
    const double t = juce::jlimit (0.0, 1.0, (double) (x - zones.plot.getX()) / (double) zones.plot.getWidth());
    return kMinHz * std::pow (kMaxHz / kMinHz, t);
}

float SpectrumLens::xForFreq (double hz) const
{
    const double t = std::log (juce::jlimit (kMinHz, kMaxHz, hz) / kMinHz) / std::log (kMaxHz / kMinHz);
    return (float) zones.plot.getX() + (float) t * (float) zones.plot.getWidth();
}

float SpectrumLens::yForDb (float db) const
{
    const float rangeDb = (float) sets.rangeDb();   // cacheado: ver la nota del header
    const float t = juce::jlimit (0.0f, 1.0f, (0.0f - db) / rangeDb);   // 0 dB arriba
    return (float) zones.plot.getY() + t * (float) zones.plot.getHeight();
}

//======================================================================================== capa estática
void SpectrumLens::renderStatic (juce::Graphics& g, int width, int height)
{
    zones = zonesFor (width, height);
    sets = processor.spectrumSettings();
    const auto& s = sets;
    lastRangeDb = s.rangeDb();

    g.setColour (th::surf.withAlpha (0.65f));
    g.fillRoundedRectangle (zones.plot.toFloat(), 3.0f);
    g.setColour (look::gridMinor);
    g.drawRoundedRectangle (zones.plot.toFloat().reduced (0.5f), 3.0f, 1.0f);

    // ---- rejilla de frecuencia: los 30 centros de ⅓ de octava, y las décadas más marcadas ----
    for (const double hz : kThirdOctaveHz)
    {
        if (hz < kMinHz || hz > kMaxHz) continue;
        const bool decade = std::abs (std::log10 (hz) - std::round (std::log10 (hz))) < 1.0e-6;
        g.setColour (decade ? th::line : th::lineSoft);
        look::fillSnapped (g, { (float) (juce::roundToInt (xForFreq (hz))), (float) (zones.plot.getY()), 1.0f, (float) (zones.plot.getHeight()) });
    }

    g.setFont (look::mono (9.0f));
    g.setColour (look::txtTertiary);
    for (const double hz : kLabelledHz)
    {
        const int x = juce::roundToInt (xForFreq (hz));
        g.drawText (shortHz (hz), x - 20, zones.freqAxis.getY() + 1, 40, 12,
                    juce::Justification::centred, true);
    }

    // ---- rejilla de dB: 0 arriba, hacia abajo el rango elegido ----
    const int step = s.rangeDb() == 60 ? 6 : (s.rangeDb() == 90 ? 10 : 20);
    for (int db = 0; db >= -s.rangeDb(); db -= step)
    {
        const int y = juce::roundToInt (yForDb ((float) db));
        g.setColour (db == 0 ? th::line : th::lineSoft);
        look::fillSnapped (g, { (float) (zones.plot.getX()), (float) (y), (float) (zones.plot.getWidth()), 1.0f });
        g.setColour (look::txtTertiary);
        g.drawText (juce::String (db), zones.dbScale.getX(), y - 6, kScaleW - 8, 12,
                    juce::Justification::centredRight, true);
    }

    g.setColour (th::mut);
    g.setFont (look::label (10.0f));
    g.drawText ("dBFS", zones.dbScale.getX(), zones.plot.getY() + 8, kScaleW - 8, 12,
                juce::Justification::centredRight, true);
}

//======================================================================================== datos
// De los bins del frame a UNA entrada por columna de píxel. Cada columna toma el máximo de los bins que
// le tocan; donde una columna cae entre dos bins (graves con FFT chica), interpola LINEALMENTE EN dB, que
// es lo que hace que la curva no escalone en los graves.
void SpectrumLens::rebuildColumns (const SpectrumFrame& f)
{
    const int width = zones.plot.getWidth();
    if (width <= 0 || f.numBins <= 1) return;

    const double binHz = f.binHz();
    const int    nSpec = spectrumNumSpectra (f.channelMode);

    // Periscope 1.1.0: W of the chosen weighting (Slope included) at each column centre. Recomputed only when the
    // curve, the slope, the sample rate (K depends on it) or the width change: K costs a complex biquad per column.
    if (colWeightWidth != width || colWeightCurve != sets.weighting || colWeightSlope != sets.slopeDbPerOct
        || colWeightSr != f.sr)
    {
        colWeight.resize ((size_t) width);
        for (int x = 0; x < width; ++x)
            colWeight[(size_t) x] = weighting::db (sets.weighting,
                                                   std::sqrt (freqAtX (zones.plot.getX() + x) * freqAtX (zones.plot.getX() + x + 1)),
                                                   f.sr, sets.slopeDbPerOct);
        colWeightWidth = width;
        colWeightCurve = sets.weighting;
        colWeightSlope = sets.slopeDbPerOct;
        colWeightSr    = f.sr;
    }

    for (int s = 0; s < SpectrumFrame::kMaxSpectra; ++s)
    {
        if (s >= nSpec)
        {
            std::fill (colDb[s].begin(), colDb[s].end(), SpectrumFrame::kFloorDb);
            std::fill (colHold[s].begin(), colHold[s].end(), SpectrumFrame::kFloorDb);
            continue;
        }

        for (int x = 0; x < width; ++x)
        {
            const double f0 = freqAtX (zones.plot.getX() + x);
            const double f1 = freqAtX (zones.plot.getX() + x + 1);
            const double centre = std::sqrt (f0 * f1);         // el centro geométrico del píxel

            const int k0 = (int) std::ceil (f0 / binHz);
            const int k1 = (int) std::ceil (f1 / binHz);

            float db = SpectrumFrame::kFloorDb, hd = SpectrumFrame::kFloorDb;
            if (k1 > k0)
            {
                for (int k = juce::jmax (0, k0); k < juce::jmin (f.numBins, k1); ++k)
                {
                    db = juce::jmax (db, f.magDb[s][k]);
                    hd = juce::jmax (hd, f.holdDb[s][k]);
                }
            }
            else
            {
                const double pos = centre / binHz;
                const int    ka  = juce::jlimit (0, f.numBins - 1, (int) std::floor (pos));
                const int    kb  = juce::jlimit (0, f.numBins - 1, ka + 1);
                const auto   t   = (float) juce::jlimit (0.0, 1.0, pos - (double) ka);
                db = f.magDb[s][ka]  + t * (f.magDb[s][kb]  - f.magDb[s][ka]);
                hd = f.holdDb[s][ka] + t * (f.holdDb[s][kb] - f.holdDb[s][ka]);
            }

            // Periscope: the weighting replaces spectrumDisplayDb's slope; with Slope it is the same tilt.
            colDb[s][(size_t) x]   = db + colWeight[(size_t) x];
            colHold[s][(size_t) x] = hd + colWeight[(size_t) x];
        }

        std::copy (f.bands[s],     f.bands[s]     + SpectrumFrame::kNumThird, bandDb[s]);
        std::copy (f.bandsHold[s], f.bandsHold[s] + SpectrumFrame::kNumThird, bandHold[s]);
        std::copy (f.bark[s],      f.bark[s]      + SpectrumFrame::kNumBark,  barkDb[s]);
    }
}

// EL PISO DE SILENCIO. El plot va de 0 dB a −rango; por debajo, la curva queda clavada en el borde. En
// silencio la curva y el hold siguen bajando —medido: entre −217 y −187 dB, con el piso del plot en −90— y
// ese movimiento no se ve, pero contaba como cambio y la lente no paraba. Cuenta si alguno de los dos
// valores está por encima del piso menos un margen: los 20 dB cubren el suavizado por fracción de octava,
// que promedia en dB antes de recortar al plot (un vecino hundido arrastra un poco a uno visible).
bool SpectrumLens::visiblyMoved (float now, float before) const noexcept
{
    const float floorDb = -(float) sets.rangeDb() - 20.0f;
    return std::abs (now - before) > 0.01f && juce::jmax (now, before) > floorDb;
}

bool SpectrumLens::advanceFrame()
{
    // Los ajustes que cambian el dibujo pueden cambiar sin un click en esta lente (un preset, el estado que
    // restaura el host): con la lente en pausa también tienen que verse.
    const auto prevSets = sets;
    sets = processor.spectrumSettings();
    const int  smooth = processor.spectrumSmoothIndex();
    const bool settingsChanged = ! (sets == prevSets) || smooth != lastSmoothIdx;
    lastSmoothIdx = smooth;

    // Referencia, no copia: el SpectrumFrame son ~260 KB y sólo hace falta hasta el final de esta función.
    const auto& f = processor.spectrum().read();
    const bool  geometry = f.fftSize != fftSizeSeen || f.channelMode != channelSeen;
    const bool  fresh = f.frameIndex != lastFrameIndex || geometry;
    bool holdMoved = false, maskMoved = false;
    if (fresh)
    {
        lastFrameIndex = f.frameIndex;
        fftSizeSeen = f.fftSize;
        numBinsSeen = f.numBins;
        channelSeen = f.channelMode;
        srSeen      = f.sr;
        for (int s = 0; s < SpectrumFrame::kMaxSpectra; ++s) holdBefore[s] = colHold[s];
        rebuildColumns (f);

        // Periscope: the masking of this frame; a change shows on the overlay, so it counts as movement.
        const int np = juce::jlimit (0, SpectrumFrame::kMaxMaskParts, f.maskParts);
        maskMoved = np != maskParts || ! std::equal (f.maskState, f.maskState + np, maskState);
        maskParts = np;
        std::copy (f.maskLoHz, f.maskLoHz + np, maskLoHz);
        std::copy (f.maskHiHz, f.maskHiHz + np, maskHiHz);
        std::copy (f.maskState, f.maskState + np, maskState);

        // Accessibility: a copy of the raw bins for the text readings (probe and peaks). The picture's columns depend
        // on the window width in pixels, while the screen reader needs numbers that do not depend on scale.
        a11ySpectra  = spectrumNumSpectra (f.channelMode);
        a11yBins     = juce::jlimit (0, SpectrumFrame::kMaxBins, f.numBins);
        a11yBinHz    = f.binHz();
        a11yHasFrame = a11yBins > 1 && a11yBinHz > 0.0;
        ++a11yFrameSerial;
        for (int s = 0; s < a11ySpectra; ++s)
        {
            a11yMag[s].assign (f.magDb[s], f.magDb[s] + a11yBins);
            a11yHold[s].assign (f.holdDb[s], f.holdDb[s] + a11yBins);
        }

        // El HOLD se dibuja tal cual (no se suaviza): se mueve cuando el motor lo baja — si se ve.
        for (int s = 0; s < SpectrumFrame::kMaxSpectra && ! holdMoved; ++s)
        {
            if (holdBefore[s].size() != colHold[s].size()) { holdMoved = true; break; }
            for (size_t x = 0; x < colHold[s].size(); ++x)
                if (visiblyMoved (colHold[s][x], holdBefore[s][x])) { holdMoved = true; break; }
        }
    }

    // Si cambió el rango de dB hay que rehornear la rejilla: es capa estática.
    if (sets.rangeDb() != lastRangeDb) invalidateStatic();

    // La curva SUBE de una y BAJA suave: es un medidor, y un pico que se pierde entre dos frames es un
    // pico que no existió. Con reduced-motion no hay suavizado: se dibuja el frame tal cual.
    const bool reduced = prefersReducedMotion();
    bool moved = false;
    for (int s = 0; s < SpectrumFrame::kMaxSpectra; ++s)
        for (size_t x = 0; x < dispDb[s].size(); ++x)
        {
            const float target = colDb[s][x];
            const float before = dispDb[s][x];
            dispDb[s][x] = (reduced || target > before) ? target : before + (target - before) * kRelease;
            moved = moved || visiblyMoved (dispDb[s][x], before);
        }

    // Antes era `fresh || moved`: con el transporte parado el motor sigue publicando una FFT por hop —de
    // silencio, idéntica a la anterior— y la lente repintaba 30 veces por segundo para siempre (prompt 96).
    // Ahora cuenta lo que SE VE: la curva, el hold, la geometría del cuadro y los ajustes.
    return moved || holdMoved || maskMoved || geometry || settingsChanged;
}

//======================================================================================== capa viva
void SpectrumLens::paintLive (juce::Graphics& g)
{
    if (zones.plot.isEmpty()) { zones = zonesFor (getWidth(), getHeight()); resized(); }

    const auto& s = sets;
    const int   nSpec = spectrumNumSpectra (channelSeen);
    const juce::Colour hues[SpectrumFrame::kMaxSpectra] = { th::green, th::magenta };

    if (s.bandsMode == Spectrum::bandsFft)
    {
        // 57b — la curva se rasteriza a mano sobre una caché a escala FÍSICA (ver el bloque grande de
        // abajo) y vuelve al plano lógico de un solo blit.
        const float ps = look::physicalScale (g);
        curveCache.prepare (ps, zones.plot.getWidth(), zones.plot.getHeight());
        rasteriseCurves (ps);
        curveCache.blit (g, zones.plot.getX(), zones.plot.getY());
    }
    else
    {
        g.saveState();
        g.reduceClipRegion (zones.plot);
        for (int i = 0; i < nSpec; ++i) paintBars (g, i, hues[i]);
        g.restoreState();
    }

    paintMasking (g);
    paintReadout (g);

    // ---- la tira de controles ----
    const juce::String avgValue = s.avgMode == Spectrum::avgNone ? tr (strings::Key::off)
                                : (s.avgMode == Spectrum::avgInfinite ? juce::String::fromUTF8 ("\xe2\x88\x9e")
                                                                      : juce::String (s.avgSeconds, 1) + " s");
    const juce::String holdValue = ! s.peakHold ? tr (strings::Key::off)
                                 : (s.holdDecayDbPerSec <= 0.0f ? juce::String::fromUTF8 ("\xe2\x88\x9e")
                                                                : juce::String ((int) s.holdDecayDbPerSec) + " dB/s");

    paintButton (g, zones.button[fftSize],   tr (strings::Key::fftSize),  juce::String (s.fftSize()),      hovered == fftSize);
    paintButton (g, zones.button[window],    tr (strings::Key::windowFn), windowLabel (s.window),          hovered == window);
    paintButton (g, zones.button[overlap],   tr (strings::Key::overlap),  juce::String ((int) std::lround (s.overlap() * 100.0f)) + " %", hovered == overlap);
    paintButton (g, zones.button[channel],   tr (strings::Key::channel),  channelLabel (s.channel),        hovered == channel);
    paintButton (g, zones.button[bandsMode], tr (strings::Key::bands),    bandsLabel (s.bandsMode),        hovered == bandsMode);
    // Periscope: with a weighting curve the slope is not used, and the button says so.
    paintButton (g, zones.button[slope],     tr (strings::Key::slope),
                 s.weighting == weighting::slope ? juce::String (s.slopeDbPerOct, 1) + " dB/oct" : tr (strings::Key::off),
                 hovered == slope);
    paintButton (g, zones.button[weightCurve], tr (strings::Key::weighting), weighting::name (s.weighting), hovered == weightCurve);
    paintButton (g, zones.button[maskCtl], tr (strings::Key::masking),
                 s.masking ? tr (strings::Key::on) : tr (strings::Key::off), hovered == maskCtl);
    paintButton (g, zones.button[average],   tr (strings::Key::average),  avgValue,                        hovered == average);
    paintButton (g, zones.button[hold],      tr (strings::Key::hold),     holdValue,                       hovered == hold);
    paintButton (g, zones.button[range],     tr (strings::Key::range),    juce::String (s.rangeDb()) + " dB", hovered == range);

    const int smoothDenom = TelescopeProcessor::kSpectrumSmoothDenom[
                                juce::jlimit (0, TelescopeProcessor::kNumSpectrumSmoothOptions - 1,
                                              processor.spectrumSmoothIndex())];
    paintButton (g, zones.button[smooth], tr (strings::Key::smoothing),
                 smoothDenom <= 0 ? tr (strings::Key::off) : "1/" + juce::String (smoothDenom) + " oct",
                 hovered == smooth);
}

//======================================================================================== 57b · el rasterizador
// ========================================================================================================
// EL RASTERIZADOR PROPIO DE SPECTRUM, y por qué esta lente dejó de dibujar con JUCE.
//
// ============================== QUÉ ESTABA MAL, dicho por quien lo vio ===================================
//
// «Spectrum se ve viejo, pixelado» (Joaquín, 9-sep, contra Insight). Y era literal: la curva se dibujaba
// con UN fillRect de 1 px de ancho POR COLUMNA para el área y otro para el trazo. Rectángulos enteros, sin
// antialiasing: en cada tramo inclinado la línea sube en escalones de un píxel entero. El comentario que
// estaba acá explicaba por qué se había hecho así —las alternativas con Path y gradiente de JUCE costaban
// 6 a 10 ms— y el dato sigue siendo cierto. Lo que cambió es que ya no hace falta elegir entre las dos:
// se escribe el píxel a mano.
//
// ===================================== CÓMO SE DIBUJA AHORA =============================================
//
//   1 · LA CURVA PASA A RESOLUCIÓN DE DISPOSITIVO. `dispDb` tiene una entrada por columna LÓGICA; entre
//       ellas se interpola con Hermite MONÓTONA (Fritsch-Carlson 1980), que es la que no se pasa de los
//       valores que interpola. Importa justo en graves, donde un bin abarca varias columnas y la curva
//       viene en escalones: una cúbica común les inventaría un sobrepico a cada escalón — un pico que no
//       midió nadie, en la lente donde la gente busca picos.
//   2 · SUAVIZADO DE PANTALLA, como SPAN: promedio móvil sobre un ancho fijo en OCTAVAS. Sobre este eje
//       (log de frecuencia, lineal en x) una fracción de octava ES un ancho fijo en píxeles, así que sale
//       de una suma corrida. Se aplica SÓLO a lo que se dibuja; el readout y el hold siguen crudos.
//   3 · EL TRAZO, con cobertura vertical exacta. Por columna se toma el SEGMENTO entre y[x] e y[x+1] y se
//       mide la distancia de cada fila a ese segmento: donde la curva es empinada el trazo se ensancha
//       solo, que es exactamente lo que hace un rasterizador de verdad y lo que el fillRect no hacía.
//   4 · GLOW BARATO. El prompt lo pide como tres pasadas (7 px al 12 %, 3 al 30 %, 1.5 al 100 %). Acá son
//       las MISMAS tres anchuras expresadas como UN perfil tabulado: el resultado es el mismo píxel y se
//       escribe una vez en vez de tres. Nada de blur por frame.
//   5 · EL RELLENO SE DEGRADA DESDE LA CURVA, no por fila absoluta: alpha = 0.42·(1−d)^1.6 con d la
//       distancia relativa a la curva. Sale de una tabla de 256 entradas; el bucle interior es una resta,
//       una multiplicación y un store.
//
// ========================== POR QUÉ EL RELLENO VA POR FILAS Y EL TRAZO POR COLUMNAS =======================
//
// El relleno son ~1.4 millones de píxeles por espectro: recorrido por columnas, cada store cae en otra
// línea de caché y se paga memoria, no cuentas. Por filas se escriben 16 píxeles por línea de caché. El
// trazo, en cambio, son ~12 filas por columna (~23 000 píxeles): ahí el recorrido por columnas no se nota
// y es el único que conoce el segmento.
//
// LA IMAGEN ES TRANSPARENTE Y PREMULTIPLICADA. Transparente porque la rejilla y el pozo son capa ESTÁTICA
// y viven debajo: una caché opaca los taparía. Premultiplicada porque es lo que guarda juce::Image::ARGB,
// y escribir ARGB derecho ahí sale mal en los bordes translúcidos, que es todo el trazo.
// ========================================================================================================
namespace
{
// Un color ya premultiplicado por su alpha, listo para escribir en una juce::Image::ARGB.
inline juce::uint32 premul (juce::Colour c, float a) noexcept
{
    const auto A = (juce::uint32) juce::jlimit (0, 255, (int) std::lround (a * 255.0f));
    const auto ch = [A] (juce::uint8 v) { return (juce::uint32) ((v * A + 127u) / 255u); };
    return (A << 24) | (ch (c.getRed()) << 16) | (ch (c.getGreen()) << 8) | ch (c.getBlue());
}

// `src` SOBRE `dst`, los dos premultiplicados. dst = src + dst·(1−αsrc).
inline juce::uint32 overPremul (juce::uint32 src, juce::uint32 dst) noexcept
{
    const juce::uint32 ia = 255u - (src >> 24);
    if (ia == 0u) return src;
    const auto ch = [ia] (juce::uint32 s, juce::uint32 d) { return s + ((d * ia + 127u) / 255u); };
    return (juce::jmin (255u, ch (src >> 24, dst >> 24)) << 24)
         | (juce::jmin (255u, ch ((src >> 16) & 0xffu, (dst >> 16) & 0xffu)) << 16)
         | (juce::jmin (255u, ch ((src >> 8) & 0xffu, (dst >> 8) & 0xffu)) << 8)
         |  juce::jmin (255u, ch (src & 0xffu, dst & 0xffu));
}

// EL PERFIL DEL TRAZO, tabulado. Son las tres anchuras del glow en una sola curva (ver el punto 4 del
// bloque de arriba): núcleo de 1.5 px al 100 %, halo de 3 px al 30 %, resplandor de 7 px al 12 %, con
// medio píxel de rampa en cada borde para que ninguno de los tres deje un anillo duro.
constexpr int   kStrokeLutN = 72;      // hasta 4.5 px de distancia, en pasos de 1/16 px
constexpr float kStrokeLutStep = 16.0f;

float strokeProfile (float d) noexcept
{
    const auto band = [d] (float halfWidth, float level)
    {
        return level * juce::jlimit (0.0f, 1.0f, (halfWidth + 0.5f - d) * 2.0f);
    };
    return juce::jmax (band (0.75f, 1.0f), juce::jmax (band (1.5f, 0.30f), band (3.5f, 0.12f)));
}

// INTERPOLACIÓN HERMITE MONÓTONA (Fritsch-Carlson): las pendientes se limitan para que el trozo entre dos
// puntos NUNCA se salga del intervalo que forman. Es la diferencia entre suavizar una curva y agregarle
// picos que no midió nadie. `v` está en una grilla de paso 1.
void monotoneSlopes (const float* v, int n, float* m) noexcept
{
    if (n <= 1) { if (n == 1) m[0] = 0.0f; return; }
    for (int i = 0; i < n; ++i)
    {
        const float dPrev = i > 0     ? v[i] - v[i - 1] : v[1] - v[0];
        const float dNext = i < n - 1 ? v[i + 1] - v[i] : v[n - 1] - v[n - 2];
        m[i] = 0.5f * (dPrev + dNext);
    }
    for (int i = 0; i < n - 1; ++i)
    {
        const float d = v[i + 1] - v[i];
        if (std::abs (d) < 1.0e-7f) { m[i] = 0.0f; m[i + 1] = 0.0f; continue; }
        const float a = m[i] / d, b = m[i + 1] / d;
        const float q = a * a + b * b;
        if (q > 9.0f)
        {
            const float tau = 3.0f / std::sqrt (q);
            m[i]     = tau * a * d;
            m[i + 1] = tau * b * d;
        }
    }
}

inline float hermite (float v0, float v1, float m0, float m1, float t) noexcept
{
    const float t2 = t * t, t3 = t2 * t;
    return (2.0f * t3 - 3.0f * t2 + 1.0f) * v0 + (t3 - 2.0f * t2 + t) * m0
         + (-2.0f * t3 + 3.0f * t2) * v1 + (t3 - t2) * m1;
}
}

// Arma la curva de UN espectro en columnas de DISPOSITIVO: interpola, suaviza y la pasa a y de píxel.
void SpectrumLens::buildDeviceCurve (int slot, int deviceW, float physScale, int smoothRadius)
{
    const int n = (int) dispDb[slot].size();
    devY[slot].assign ((size_t) deviceW, (float) curveCache.deviceH());
    devHoldY[slot].assign ((size_t) deviceW, (float) curveCache.deviceH());
    if (n <= 1 || deviceW <= 0) return;

    devSlope.assign ((size_t) n, 0.0f);
    devScratch.assign ((size_t) deviceW, 0.0f);

    const float invS  = 1.0f / juce::jmax (1.0e-3f, physScale);
    const float plotY = (float) zones.plot.getY();
    const float dev   = [&] { return (float) curveCache.deviceH() / juce::jmax (1.0f, (float) zones.plot.getHeight()); }();

    // ---- (a) dB por columna de dispositivo, Hermite monótona sobre las columnas lógicas ----
    const auto resample = [&] (const std::vector<float>& src, std::vector<float>& dst)
    {
        monotoneSlopes (src.data(), n, devSlope.data());
        for (int x = 0; x < deviceW; ++x)
        {
            const float u = juce::jlimit (0.0f, (float) (n - 1), ((float) x + 0.5f) * invS - 0.5f);
            const int   i = juce::jlimit (0, n - 2, (int) u);
            const float t = juce::jlimit (0.0f, 1.0f, u - (float) i);
            dst[(size_t) x] = hermite (src[(size_t) i], src[(size_t) (i + 1)],
                                       devSlope[(size_t) i], devSlope[(size_t) (i + 1)], t);
        }
    };

    resample (dispDb[slot], devScratch);

    // ---- (b) suavizado de pantalla: promedio móvil de ancho fijo en octavas (ver el bloque de arriba) ----
    if (smoothRadius > 0)
    {
        std::vector<float>& v = devScratch;
        double run = 0.0;
        const int w = smoothRadius;
        std::vector<float> out ((size_t) deviceW, 0.0f);
        for (int x = -w; x <= w; ++x) run += v[(size_t) juce::jlimit (0, deviceW - 1, x)];
        for (int x = 0; x < deviceW; ++x)
        {
            out[(size_t) x] = (float) (run / (double) (2 * w + 1));
            run -= v[(size_t) juce::jlimit (0, deviceW - 1, x - w)];
            run += v[(size_t) juce::jlimit (0, deviceW - 1, x + w + 1)];
        }
        v.swap (out);
    }

    // ---- (c) a y de píxel de dispositivo, relativa al plot ----
    for (int x = 0; x < deviceW; ++x)
        devY[slot][(size_t) x] = (yForDb (devScratch[(size_t) x]) - plotY) * dev;

    if (sets.peakHold)
    {
        resample (colHold[slot], devScratch);   // el HOLD no se suaviza: es una marca, no una curva
        for (int x = 0; x < deviceW; ++x)
            devHoldY[slot][(size_t) x] = (yForDb (devScratch[(size_t) x]) - plotY) * dev;
    }
}

// Pinta LOS DOS espectros sobre la caché, a resolución de dispositivo.
void SpectrumLens::rasteriseCurves (float physScale)
{
    const int W = curveCache.deviceW(), H = curveCache.deviceH();
    if (W <= 1 || H <= 1) return;

    auto& img = curveCache.image();
    img.clear (img.getBounds());   // transparente: la rejilla vive DEBAJO, en la capa estática

    const int nSpec = spectrumNumSpectra (channelSeen);
    const juce::Colour hues[SpectrumFrame::kMaxSpectra] = { th::green, th::magenta };

    // El radio del suavizado: una fracción de octava sobre un eje log ES un ancho fijo en píxeles.
    const int denom = TelescopeProcessor::kSpectrumSmoothDenom[
                          juce::jlimit (0, TelescopeProcessor::kNumSpectrumSmoothOptions - 1,
                                        processor.spectrumSmoothIndex())];
    const double octaves = std::log2 (kMaxHz / kMinHz);
    const int smoothRadius = denom <= 0 ? 0
                           : juce::jmax (1, (int) std::lround ((double) W / (octaves * (double) denom) * 0.5));

    // Las tablas: 256 alphas del relleno y el perfil del trazo. Se arman por espectro (dos hues) y no por
    // píxel — que es de lo que vive el presupuesto de esta lente.
    std::array<juce::uint32, 256> fillLut {};
    std::array<juce::uint32, (size_t) kStrokeLutN> strokeLut {}, holdLut {};

    const juce::Image::BitmapData bd (img, juce::Image::BitmapData::readWrite);
    if (bd.pixelStride != 4) return;

    for (int s = 0; s < nSpec; ++s)
    {
        buildDeviceCurve (s, W, physScale, smoothRadius);

        const auto hue = hues[s];
        // El relleno arranca en 0.42 CONTRA LA CURVA y se apaga hacia abajo con (1−d)^1.6 (57b). Con L+R
        // los dos rellenos se SUPERPONEN, y dos capas al 42 % dan un velo lechoso que se come el pozo:
        // ahí cada una lleva su parte, así que el velo combinado es el mismo que el de un espectro solo.
        const float fillPeak = 0.42f / (float) juce::jmax (1, nSpec);
        for (int i = 0; i < 256; ++i)
        {
            const float d = (float) i / 255.0f;
            fillLut[(size_t) i] = premul (hue, fillPeak * std::pow (1.0f - d, 1.6f));
        }
        for (int i = 0; i < kStrokeLutN; ++i)
        {
            const float d = (float) i / kStrokeLutStep;
            strokeLut[(size_t) i] = premul (hue, strokeProfile (d));
            holdLut[(size_t) i]   = premul (hue, 0.45f * juce::jlimit (0.0f, 1.0f, (1.0f - d) * 2.0f));
        }

        const float* yc = devY[s].data();

        // ---- RELLENO, por filas (ver el bloque de arriba) ----
        {
            float topY = (float) H;
            for (int x = 0; x < W; ++x) topY = juce::jmin (topY, yc[x]);
            const int y0 = juce::jlimit (0, H - 1, (int) std::floor (topY));

            // 255/(H − yc): la constante por columna que convierte "cuánto bajé" en índice de la tabla.
            devScratch.assign ((size_t) W, 0.0f);
            for (int x = 0; x < W; ++x)
                devScratch[(size_t) x] = 255.0f / juce::jmax (1.0f, (float) H - yc[x]);
            const float* kScale = devScratch.data();

            for (int y = y0; y < H; ++y)
            {
                auto* row = (juce::uint32*) bd.getLinePointer (y);
                const float fy = (float) y + 0.5f;
                if (s == 0)
                {
                    // El primer espectro cae sobre una imagen recién limpiada: es un store y nada más.
                    for (int x = 0; x < W; ++x)
                    {
                        const float d = fy - yc[x];
                        if (d < 0.0f) continue;
                        row[x] = fillLut[(size_t) juce::jlimit (0, 255, (int) (d * kScale[x]))];
                    }
                }
                else
                {
                    for (int x = 0; x < W; ++x)
                    {
                        const float d = fy - yc[x];
                        if (d < 0.0f) continue;
                        row[x] = overPremul (fillLut[(size_t) juce::jlimit (0, 255, (int) (d * kScale[x]))],
                                             row[x]);
                    }
                }
            }
        }

        // ---- TRAZO + GLOW, por columnas: el segmento entre y[x] e y[x+1] con cobertura exacta ----
        const float reach = (float) (kStrokeLutN - 1) / kStrokeLutStep;
        for (int x = 0; x < W; ++x)
        {
            const float a = yc[x], b = yc[juce::jmin (W - 1, x + 1)];
            const float lo = juce::jmin (a, b), hi = juce::jmax (a, b);
            const int   y0 = juce::jmax (0,     (int) std::floor (lo - reach));
            const int   y1 = juce::jmin (H - 1, (int) std::ceil  (hi + reach));
            for (int y = y0; y <= y1; ++y)
            {
                const float fy = (float) y + 0.5f;
                const float d  = juce::jmax (0.0f, juce::jmax (lo - fy, fy - hi));
                const int   i  = (int) (d * kStrokeLutStep);
                if (i >= kStrokeLutN) continue;
                auto* px = (juce::uint32*) bd.getLinePointer (y) + x;
                *px = overPremul (strokeLut[(size_t) i], *px);
            }
        }

        // ---- PEAK HOLD: línea fina antialiaseada, sin relleno ni glow. Es una referencia, no el dato ----
        if (sets.peakHold)
        {
            const float* yh = devHoldY[s].data();
            for (int x = 0; x < W; ++x)
            {
                const float a = yh[x], b = yh[juce::jmin (W - 1, x + 1)];
                const float lo = juce::jmin (a, b), hi = juce::jmax (a, b);
                const int   y0 = juce::jmax (0,     (int) std::floor (lo - 1.0f));
                const int   y1 = juce::jmin (H - 1, (int) std::ceil  (hi + 1.0f));
                for (int y = y0; y <= y1; ++y)
                {
                    const float fy = (float) y + 0.5f;
                    const float d  = juce::jmax (0.0f, juce::jmax (lo - fy, fy - hi));
                    const int   i  = (int) (d * kStrokeLutStep);
                    if (i >= kStrokeLutN) continue;
                    auto* px = (juce::uint32*) bd.getLinePointer (y) + x;
                    *px = overPremul (holdLut[(size_t) i], *px);
                }
            }
        }
    }
}

// ⅓ de octava y Bark: barras entre los bordes REALES de cada banda, así el ancho de la barra ES el ancho
// de la banda. Barras de ancho igual sobre un eje log serían un gráfico que miente sobre el eje.
//
// EL SLOPE NO SE APLICA ACÁ, y eso no es un olvido. El slope existe para compensar que un bin de FFT mide
// una franja de ancho FIJO (Δf = sr/N): sobre ruido rosa, cuya densidad cae 3 dB por octava, la curva
// cruda baja. Una banda de ⅓ de octava, en cambio, ya integra un ancho PROPORCIONAL a la frecuencia — o
// sea que ya hace por construcción lo que el slope hace a mano. Aplicar los dos inclinaría el rosa 3 dB
// por octava hacia ARRIBA, que es exactamente el error que este comentario existe para que nadie repita.
void SpectrumLens::paintBars (juce::Graphics& g, int slot, juce::Colour hue) const
{
    const auto& s = sets;
    const bool third = s.bandsMode == Spectrum::bandsThird;
    const int  n = third ? SpectrumFrame::kNumThird : SpectrumFrame::kNumBark;
    const int  nSpec = spectrumNumSpectra (channelSeen);

    constexpr double kSixthDown = 0.8908987181403393, kSixthUp = 1.1224620483093730;
    const double binHz = fftSizeSeen > 0 ? srSeen / (double) fftSizeSeen : 0.0;

    for (int b = 0; b < n; ++b)
    {
        const double lo = third ? kThirdOctaveHz[b] * kSixthDown : kBarkEdgesHz[b];
        const double hi = third ? kThirdOctaveHz[b] * kSixthUp   : kBarkEdgesHz[b + 1];
        if (hi <= kMinHz || lo >= kMaxHz) continue;

        const float bw = bandWeight (third ? kThirdOctaveHz[b] : std::sqrt (lo * hi));   // Periscope: 0 with Slope
        const float db = (third ? bandDb[slot][b] : barkDb[slot][b]) + bw;
        const float xa = xForFreq (lo), xb = xForFreq (hi);
        const float y  = yForDb (db);

        // Con L+R las dos barras van LADO A LADO dentro de la banda: superpuestas, la segunda tapa a la
        // primera y el modo estéreo no mostraría nada del primer canal.
        const float full = juce::jmax (2.0f, xb - xa - 2.0f);
        const float w    = nSpec > 1 ? full * 0.5f : full;
        const float x    = xa + 1.0f + (nSpec > 1 ? (float) slot * w : 0.0f);

        // Banda SIN NINGÚN BIN adentro (con FFT de 4 096 a 48 k, la de 40 Hz mide 9.3 Hz y el bin 11.7).
        // No es que no haya energía: es que a esta resolución la banda no se puede medir. Se marca
        // distinto de una barra en cero — dibujar cero sería decir "no hay nada", que no es lo mismo.
        if (binHz > 0.0 && std::ceil (hi / binHz) <= std::ceil (lo / binHz))
        {
            g.setColour (look::tick.withAlpha (0.55f));
            g.fillRect (juce::Rectangle<float> (x, (float) zones.plot.getBottom() - 3.0f, w, 3.0f));
            continue;
        }

        g.setColour (hue.withAlpha (0.30f));
        g.fillRect (juce::Rectangle<float> (x, y, w, (float) zones.plot.getBottom() - y));
        g.setColour (hue.withAlpha (0.9f));
        g.fillRect (juce::Rectangle<float> (x, y, w, 1.5f));

        if (third && s.peakHold)
        {
            const float yh = yForDb (bandHold[slot][b] + bw);
            g.setColour (hue.withAlpha (0.55f));
            g.fillRect (juce::Rectangle<float> (x, yh, w, 1.0f));
        }
    }
}

//======================================================================================== lectura
SpectrumLens::Readout SpectrumLens::readoutAtX (int x) const
{
    Readout r;
    if (! zones.plot.contains (x, zones.plot.getCentreY())) return r;

    const int col = juce::jlimit (0, juce::jmax (0, zones.plot.getWidth() - 1), x - zones.plot.getX());
    if ((int) colDb[0].size() <= col) return r;

    const auto& s = sets;
    r.valid  = true;
    r.freqHz = std::sqrt (freqAtX (zones.plot.getX() + col) * freqAtX (zones.plot.getX() + col + 1));
    r.note   = noteFor (r.freqHz);

    if (s.bandsMode == Spectrum::bandsFft)
    {
        r.db = colDb[0][(size_t) col];
    }
    else
    {
        // En modo de bandas lo que hay bajo el cursor es una BANDA, no un bin: se reporta la banda.
        const bool third = s.bandsMode == Spectrum::bandsThird;
        const int  n = third ? SpectrumFrame::kNumThird : SpectrumFrame::kNumBark;
        constexpr double kSixthDown = 0.8908987181403393, kSixthUp = 1.1224620483093730;
        r.db = SpectrumFrame::kFloorDb;
        for (int b = 0; b < n; ++b)
        {
            const double lo = third ? kThirdOctaveHz[b] * kSixthDown : kBarkEdgesHz[b];
            const double hi = third ? kThirdOctaveHz[b] * kSixthUp   : kBarkEdgesHz[b + 1];
            if (r.freqHz >= lo && r.freqHz < hi)
            {
                r.db = (third ? bandDb[0][b] : barkDb[0][b])   // sin slope: ver la nota de paintBars
                     + bandWeight (third ? kThirdOctaveHz[b] : std::sqrt (lo * hi));
                break;
            }
        }
    }
    return r;
}

void SpectrumLens::paintReadout (juce::Graphics& g) const
{
    if (cursorX < 0) return;
    const auto r = readoutAtX (cursorX);
    if (! r.valid) return;

    // ===== 56 ===== crosshair de verdad, no una línea vertical suelta: la vertical dice DÓNDE está el
    // cursor y la horizontal, con el punto, dice a QUÉ ALTURA cae el valor que dice el readout. Sin la
    // horizontal hay que estimar el nivel a ojo contra la rejilla, que es lo que el readout venía a
    // resolver. Snappeado a píxel físico (Look.h) para que la cruz no salga en gris sucio en Retina.
    look::drawCrosshair (g, zones.plot, cursorX, juce::roundToInt (yForDb (r.db)));

    const juce::String text = shortHz (r.freqHz) + " Hz  \xc2\xb7  " + r.note.name + "  "
                            + (r.note.cents >= 0 ? "+" : "") + juce::String (r.note.cents)
                            + juce::String::fromUTF8 (" \xc2\xa2") + "  \xc2\xb7  "
                            + juce::String (r.db, 1) + " dB";

    const auto m = look::metricsFor (getWidth());
    g.setFont (look::tabularFont (m.textSmall));
    const int tw = juce::jmax (150, (int) std::ceil (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text)) + 16);
    // La caja se ACOTA al plot antes de ubicarse: con la lente angosta el texto puede ser más ancho que el
    // área y la cuenta directa la sacaba por el borde izquierdo (nit del revisor del 50). Ver LensReadout.h.
    // 56: la GEOMETRÍA sigue siendo la de LensReadout.h (única fuente de esa cuenta); el DIBUJO ahora sale
    // de Look.h, así las cinco lentes que tienen readout usan exactamente la misma cajita.
    look::drawReadoutBox (g, readoutBoxFor (zones.plot, cursorX, tw), text, m);
}

void SpectrumLens::paintButton (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& label,
                                const juce::String& value, bool hovered_) const
{
    const auto hue = th::green;
    const auto r = area.toFloat();

    g.setColour (th::surf2);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (hue.withAlpha (hovered_ ? 0.55f : 0.24f));
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    if (hovered_)
    {
        g.setColour (hue.withAlpha (th::state::hoverGlow));
        g.fillRoundedRectangle (r, 3.0f);
    }

    auto inner = area.reduced (7, 0);
    look::drawLabelValue (g, inner, label, look::label (9.0f), look::txtTertiary,
                         value, look::mono (11.0f), hue);
}

//======================================================================================== interacción
void SpectrumLens::cycleControl (int control)
{
    auto s = processor.spectrumSettings();
    switch (control)
    {
        case fftSize:
            s.fftOrder = s.fftOrder >= Spectrum::kMaxFftOrder ? Spectrum::kMinFftOrder : s.fftOrder + 1;
            break;
        case window:    s.window       = (s.window + 1) % Spectrum::kNumWindows; break;
        case overlap:   s.overlapIndex = (s.overlapIndex + 1) % Spectrum::kNumOverlaps; break;
        case channel:   s.channel      = (s.channel + 1) % Spectrum::kNumChannels; break;
        case bandsMode: s.bandsMode    = (s.bandsMode + 1) % Spectrum::kNumBandModes; break;
        case slope:
            // Periscope: while a weighting curve is on, the first click brings the slope back as it was.
            if (s.weighting != weighting::slope) { s.weighting = weighting::slope; break; }
            s.slopeDbPerOct += Spectrum::kSlopeStep;
            if (s.slopeDbPerOct > Spectrum::kMaxSlopeDbPerOct + 1.0e-4f) s.slopeDbPerOct = Spectrum::kMinSlopeDbPerOct;
            break;
        case weightCurve: s.weighting = (s.weighting + 1) % weighting::kNumCurves; break;
        case maskCtl:     s.masking = ! s.masking; break;
        case average:   s.avgMode      = (s.avgMode + 1) % Spectrum::kNumAverages; break;
        case hold:
            // OFF → 12 dB/s → 30 → ∞ (0) → OFF. Lo que un peak hold necesita elegir es cuánto TARDA en
            // olvidarse, y el infinito (que es lo que uno quiere para un barrido) tiene que estar.
            if (! s.peakHold)                        { s.peakHold = true;  s.holdDecayDbPerSec = 12.0f; }
            else if (s.holdDecayDbPerSec >= 30.0f)   { s.holdDecayDbPerSec = 0.0f; }
            else if (s.holdDecayDbPerSec <= 0.0f)    { s.peakHold = false; s.holdDecayDbPerSec = 12.0f; }
            else                                     { s.holdDecayDbPerSec = 30.0f; }
            break;
        case range:     s.rangeDbIndex = (s.rangeDbIndex + 1) % Spectrum::kNumRanges; break;
        // 57b — el suavizado es de DIBUJO: no va en los settings del espectro (no viaja al motor).
        case smooth:
            processor.setSpectrumSmoothIndex ((processor.spectrumSmoothIndex() + 1)
                                              % TelescopeProcessor::kNumSpectrumSmoothOptions);
            repaint();
            return;
        default: return;
    }
    processor.setSpectrumSettings (s);
    sets = processor.spectrumSettings();   // el módulo puede haber acotado algo: se cachea lo REAL
    invalidateStatic();                    // la rejilla depende del rango y de las etiquetas
    repaint();
}

void SpectrumLens::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < kNumControls; ++i)
        if (zones.button[i].contains (e.getPosition())) { cycleControl (i); return; }
}

void SpectrumLens::mouseMove (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    const int was = hovered, wasX = cursorX;

    hovered = -1;
    for (int i = 0; i < kNumControls; ++i)
        if (zones.button[i].contains (p)) { hovered = i; break; }

    cursorX = zones.plot.contains (p) ? p.x : -1;
    if (hovered != was || cursorX != wasX) repaint();
}

void SpectrumLens::mouseExit (const juce::MouseEvent&)
{
    if (hovered != -1 || cursorX != -1) { hovered = -1; cursorX = -1; repaint(); }
}

//======================================================================================== accessibility
// Text readings for the screen reader. A sighted user sees the whole curve at once: first its overall shape and
// tilt, then what sticks out of it, and only then a number at one frequency. Speech gives one thing at a time,
// so the readings hold the same three levels: the overview (signal tilt, shape), what sticks out (peaks and
// dips) and the probe, a cursor for one frequency that comes first because it is used most. Every level is the
// one the plot shows, so a sighted helper reads the same numbers:
//
//   SLOPE      levels are spoken with the slope, as drawn: tilted around 1 kHz (with 0 they are as a meter reads
//              a tone). THE LINE is flat on the plot, at the median height of the curve; the shape row says its
//              level. A mix that follows it sounds balanced. The probe says how far above or below the line it
//              is; the signal tilt row says how the curve leans on the plot; the shape row lists the broad
//              stretches above and below the line.
//   SMOOTHING  the probe reads the curve smoothed as it is drawn, and the smoothing sets the size of the peaks
//              and dips that are listed: off lists narrow resonances, 1/6 octave lists broad humps and holes.
//              The line and the shape always use a broad 1/3 octave view, like a first glance: they do not
//              change when the user zooms into details.
//   AVERAGE    how much the numbers move between key presses; the Pause of the Loudness and Dynamics lenses
//              holds all of them at one moment.
//
// Third-octave bands are spoken as before: band power, which is already flat for pink noise.
namespace
{
constexpr int    kProbeMinMidi = 16;    // 20.6 Hz, the bottom edge of the plot
constexpr int    kProbeMaxMidi = 135;   // 19.9 kHz, the top edge
constexpr int    kGridPerOct   = 96;
constexpr double kGridLoHz     = 20.0;
constexpr juce::uint32 kShapeHoldMs = 1000;   // while music plays, the peak list holds still this long

// Readings rows: probe, signal tilt, shape, then the peaks and dips.
enum ReadingRow { rrProbe = 0, rrTilt, rrShape, rrMasking, kFirstFeatureRow };   // Periscope: + masking

double midiHz (int midi)   { return 440.0 * std::pow (2.0, (double) (midi - 69) / 12.0); }
int    gridSize()          { return (int) std::floor (kGridPerOct * std::log2 (20000.0 / kGridLoHz)) + 1; }
double gridHz (double i)   { return kGridLoHz * std::pow (2.0, i / kGridPerOct); }
double gridPos (double hz) { return kGridPerOct * std::log2 (hz / kGridLoHz); }

// Settings rows: the buttons under the plot, no more (sighted users have exactly these), with the ones that matter
// most for listening first.
constexpr int kSettingRows[] = { SpectrumLens::weightCurve, SpectrumLens::maskCtl, SpectrumLens::slope, SpectrumLens::smooth, SpectrumLens::average, SpectrumLens::hold,
                                 SpectrumLens::range, SpectrumLens::fftSize, SpectrumLens::window,
                                 SpectrumLens::overlap, SpectrumLens::channel, SpectrumLens::bandsMode };
constexpr int kNumSettingRows = (int) (sizeof (kSettingRows) / sizeof (kSettingRows[0]));

// Smoothing as the denominator of the octave fraction, 0 = off.
int smoothDenom (int index)
{
    return TelescopeProcessor::kSpectrumSmoothDenom[juce::jlimit (0, TelescopeProcessor::kNumSpectrumSmoothOptions - 1, index)];
}

float medianOf (const std::vector<float>& v, int lo, int hi, std::vector<float>& scratch)
{
    scratch.assign (v.begin() + lo, v.begin() + hi + 1);
    const auto mid = scratch.begin() + (std::ptrdiff_t) (scratch.size() / 2);
    std::nth_element (scratch.begin(), mid, scratch.end());
    return *mid;
}

// Moving average in dB over 2 * radius + 1 points, edges held (as buildDeviceCurve smooths the drawn curve).
void movingAverage (const std::vector<float>& src, std::vector<float>& dst, int radius)
{
    const int n = (int) src.size();
    dst.assign ((size_t) n, 0.0f);
    if (n == 0) return;
    if (radius <= 0) { dst = src; return; }
    double run = 0.0;
    for (int x = -radius; x <= radius; ++x) run += src[(size_t) juce::jlimit (0, n - 1, x)];
    for (int x = 0; x < n; ++x)
    {
        dst[(size_t) x] = (float) (run / (double) (2 * radius + 1));
        run -= src[(size_t) juce::jlimit (0, n - 1, x - radius)];
        run += src[(size_t) juce::jlimit (0, n - 1, x + radius + 1)];
    }
}

// Periscope: roundHz, hzNumber and hzRange now live in a11y/SpokenFormat.h, shared with Tonal balance.
using spoken::roundHz;
using spoken::hzNumber;
using spoken::hzRange;
}

int SpectrumLens::a11ySettingItem (int row) const
{
    return kSettingRows[juce::jlimit (0, kNumSettingRows - 1, row)];
}

int SpectrumLens::a11yFeatureRows() const
{
    const auto& sh = a11yShape();
    return juce::jmax (1, (int) (sh.peaks.size() + sh.dips.size()));
}

juce::String SpectrumLens::a11yGroupName (int group) const
{
    switch (group)
    {
        case 0:  return a11ytext::T ("Readings");
        case 1:  return a11ytext::T ("Third-octave bands");
        default: return a11ytext::T ("Settings");
    }
}

int SpectrumLens::a11yRowCount (int group) const
{
    switch (group)
    {
        case 0:  return kFirstFeatureRow + a11yFeatureRows();
        case 1:  return SpectrumFrame::kNumThird;
        case 2:  return kNumSettingRows;
        default: return 0;
    }
}

juce::String SpectrumLens::a11yRowName (int group, int row) const
{
    if (group == 0)
    {
        if (row == rrProbe) return a11ytext::T ("Probe");
        if (row == rrTilt)  return a11ytext::T ("Signal tilt");
        if (row == rrShape) return a11ytext::T ("Shape against the line");
        if (row == rrMasking) return a11ytext::T ("Masking");
        const auto& sh = a11yShape();
        const int k = row - kFirstFeatureRow, peaks = (int) sh.peaks.size(), dips = (int) sh.dips.size();
        if (k < peaks)        return a11ytext::TF ("Peak {0}", { juce::String (k + 1) });
        if (k - peaks < dips) return a11ytext::TF ("Dip {0}", { juce::String (k - peaks + 1) });
        return a11ytext::T ("Peaks and dips");
    }
    if (group == 1) return spoken::hz (kThirdOctaveHz[juce::jlimit (0, SpectrumFrame::kNumThird - 1, row)]);

    const int item = a11ySettingItem (row);
    static const char* const names[kNumControls] = { "FFT size", "Window", "Overlap", "Channel", "Display",
                                                     "Slope", "Average", "Peak hold", "Range", "Smoothing",
                                                     "Weighting", "Masking" };
    return a11ytext::T (names[juce::jlimit (0, kNumControls - 1, item)]);
}

juce::String SpectrumLens::a11yRowValue (int group, int row) const
{
    if (group == 2) return a11yControlValue (a11ySettingItem (row));
    if (group == 0 && row == rrMasking && ! sets.masking) return a11ytext::T ("Off");
    if (! a11yHasFrame) return a11ytext::T ("no data yet");
    if (group == 1) return a11yBandValue (juce::jlimit (0, SpectrumFrame::kNumThird - 1, row));

    if (row == rrProbe)
    {
        const double hz = midiHz (probeMidi);
        auto t = spoken::hz (hz) + ", " + spoken::note (noteFor (hz)) + ", " + a11yLevels (hz, true);
        // Periscope: in bar mode the screen and the mouse readout show the bar under the cursor, not the FFT curve;
        // say that bar too, from the same data and the same rule as readoutAtX (first channel, weighted).
        if (sets.bandsMode != Spectrum::bandsFft)
        {
            const bool third = sets.bandsMode == Spectrum::bandsThird;
            const int  nb = third ? SpectrumFrame::kNumThird : SpectrumFrame::kNumBark;
            constexpr double kSixthDown = 0.8908987181403393, kSixthUp = 1.1224620483093730;
            for (int b = 0; b < nb; ++b)
            {
                const double blo = third ? kThirdOctaveHz[b] * kSixthDown : kBarkEdgesHz[b];
                const double bhi = third ? kThirdOctaveHz[b] * kSixthUp   : kBarkEdgesHz[b + 1];
                if (hz < blo || hz >= bhi) continue;
                const float bar = (third ? bandDb[0][b] : barkDb[0][b]) + bandWeight (third ? kThirdOctaveHz[b] : std::sqrt (blo * bhi));
                t << ", " << a11ytext::TF ("bar {0}", { spoken::db (bar, sets.rangeDb()) });
                break;
            }
        }
        const auto line = a11yLineDeviation (hz);
        if (line.isNotEmpty()) t << ", " << line;
        const auto mask = a11yMaskAt (hz);   // Periscope: as the overlay shows it under the probe
        if (mask.isNotEmpty()) t << ", " << mask;
        return t;
    }
    if (row == rrTilt)  return a11yTiltValue();
    if (row == rrShape) return a11yShapeValue();
    if (row == rrMasking) return a11yMaskingValue();

    const auto& sh = a11yShape();
    const int k = row - kFirstFeatureRow, peaks = (int) sh.peaks.size(), dips = (int) sh.dips.size();
    if (k < peaks)        return a11yFeatureValue (sh.peaks[(size_t) k], false);
    if (k - peaks < dips) return a11yFeatureValue (sh.dips[(size_t) (k - peaks)], true);
    return a11ytext::TF ("none standing out by {0} dB or more", { spoken::trimmed (sh.threshold, 1) });
}

// Periscope: the Q key on a peak or dip row.
juce::String SpectrumLens::a11yRowQ (int group, int row) const
{
    if (group != 0 || row < kFirstFeatureRow || ! a11yHasFrame) return {};
    const auto& sh = a11yShape();
    const int k = row - kFirstFeatureRow, peaks = (int) sh.peaks.size(), dips = (int) sh.dips.size();
    if (k < peaks)        return a11yFeatureQ (sh.peaks[(size_t) k]);
    if (k - peaks < dips) return a11yFeatureQ (sh.dips[(size_t) (k - peaks)]);
    return {};
}

// Periscope: the bell Q an EQ would need for this hump or hole. Its edges are where it crosses half its height in
// dB, which is how the RBJ cookbook defines the width of a peaking band (Equalizer APO and most EQs use it), so
// Q = sqrt (lo * hi) / (hi - lo): one octave gives 1.41. FabFilter Pro-Q counts about 1.4 times higher.
juce::String SpectrumLens::a11yFeatureQ (const Feature& f) const
{
    if (f.loHz <= 0.0) return {};
    // The narrowest width the speech grid can tell is one grid step either side; narrower reads as "or more".
    const double minRatio = std::pow (2.0, 2.0 / kGridPerOct);
    const bool   atGrid   = f.hiHz <= f.loHz * minRatio;
    const double lo = f.loHz, hi = atGrid ? f.loHz * minRatio : f.hiHz;
    const double q  = std::sqrt (lo * hi) / (hi - lo);
    // Two decimals below 1, so broad humps (0.27, 0.18) stay apart (review after 1.3.0).
    const auto   qs = spoken::trimmed ((float) q, q < 1.0 ? 2 : (q < 10.0 ? 1 : 0));
    // The feature's frequency first: the peak list is rebuilt while music plays, and the answer must say which one.
    auto t = spoken::hz (f.hz) + ", " + (atGrid ? a11ytext::TF ("Q {0} or more", { qs }) : a11ytext::TF ("Q {0}", { qs }));

    // Smoothing widens every hump to at least its own width, so a hump that narrow may be narrower in fact.
    const int denom = smoothDenom (processor.spectrumSmoothIndex());
    if (denom > 0 && std::log2 (hi / lo) <= 1.25 / denom) t << ", " << a11ytext::T ("limited by the smoothing");
    // In the bass the FFT bin is wider than the speech grid: a hump about the window's main lobe wide is the FFT's
    // width, not the sound's (a room mode reads Q 1.7 at 60 Hz with FFT 4096). Say so.
    else if (! atGrid && fftSizeSeen > 0 && hi - lo <= 4.0 * srSeen / (double) fftSizeSeen)
        t << ", " << a11ytext::T ("limited by the FFT size");
    return t;
}

bool SpectrumLens::a11yIsAdjustable (int group, int row) const
{
    return group == 2 || (group == 0 && row == rrProbe);
}

bool SpectrumLens::a11yAdjust (int group, int row, int step)
{
    if (group == 2) return stepControl (a11ySettingItem (row), step);
    if (group != 0 || row != rrProbe) return false;

    // The probe moves by notes: step is a semitone, coarse step an octave.
    const int size  = std::abs (step) >= a11yui::kExtreme ? 1000 : (std::abs (step) >= a11yui::kCoarse ? 12 : 1);
    const int next  = juce::jlimit (kProbeMinMidi, kProbeMaxMidi, probeMidi + (step > 0 ? size : -size));
    if (next == probeMidi) return false;
    probeMidi = next;

    // The crosshair on the plot follows the probe, so a sighted helper sees where the probe is looking.
    cursorX = juce::roundToInt (xForFreq (midiHz (probeMidi)));
    repaint();
    return true;
}

bool SpectrumLens::a11yIsActivatable (int group, int row) const
{
    if (group != 0 || row < kFirstFeatureRow || ! a11yHasFrame) return false;
    const auto& sh = a11yShape();
    return row - kFirstFeatureRow < (int) (sh.peaks.size() + sh.dips.size());
}

juce::String SpectrumLens::a11yActivate (int group, int row)
{
    if (! a11yIsActivatable (group, row)) return {};

    // Enter on a peak or a dip puts the probe on it, so the user can walk around it by semitones and hear how
    // wide it is. The editor then moves the cursor to the probe row (a11yTakeCursorRequest).
    const auto& sh = a11yShape();
    const int k = row - kFirstFeatureRow, peaks = (int) sh.peaks.size();
    const double hz = k < peaks ? sh.peaks[(size_t) k].hz : sh.dips[(size_t) (k - peaks)].hz;
    probeMidi = juce::jlimit (kProbeMinMidi, kProbeMaxMidi, juce::roundToInt (69.0 + 12.0 * std::log2 (hz / 440.0)));
    cursorX = juce::roundToInt (xForFreq (midiHz (probeMidi)));
    cursorToProbe = true;
    repaint();
    return {};
}

bool SpectrumLens::a11yTakeCursorRequest (int& group, int& row)
{
    if (! cursorToProbe) return false;
    cursorToProbe = false;
    group = 0;
    row = rrProbe;
    return true;
}

// F1 on a row: what it is for and when to use it. Each text is one catalog key, whole.
juce::String SpectrumLens::a11yRowHelp (int group, int row) const
{
    static const char* const helps[] = {
        // 0: probe
        "Probe: the level at one note, as the plot shows it, with the Slope tilt. Plus and minus move it a semitone, "
        "Alt with Page Up or Page Down an octave. With Smoothing on, the level is the height of the smoothed curve, "
        "and the hold is the unsmoothed peak, the number the mouse box shows. It also says how far it is from the "
        "line: a flat line at the middle height of the curve. A mix that follows the line sounds balanced.",
        // 1: signal tilt
        "Signal tilt: how the curve leans on the plot from 50 Hz to 10 kHz. The Slope setting makes a balanced mix "
        "flat, so falling means darker than the line, more lows, and rising means brighter, more highs. With Slope "
        "at 0 it is the signal's own tilt: pink noise falls 3 dB per octave, a typical mix about 4.5.",
        // 2: peaks and dips
        "Peaks and dips: places where the curve stands out from the neighbouring frequencies, the biggest first. "
        "Smoothing sets their size: off lists narrow resonances, 1/6 octave lists broad humps and holes. Enter "
        "moves the probe there. Q speaks the EQ Q of the hump or hole.",
        // 3: slope
        "Slope tilts the picture around 1 kHz, and every level spoken in this lens with it: above 1 kHz levels go "
        "up, below they go down, by this much per octave. 3 dB per octave makes pink noise flat, 4.5 a typical "
        "finished mix. At 0 a tone reads its true level, as in CQT and Spectrogram. The third-octave bands are not "
        "tilted, as on screen.",
        // 4: smoothing
        "Smoothing sets how big a detail counts. Off: narrow resonances and many small peaks. 1/24 and 1/12 "
        "octave: medium details. 1/6 octave: broad humps and holes. 1/3 octave: only the overall balance, as for "
        "comparing mixes. The probe reads the smoothed curve too.",
        // 5: average
        "Average sets how the curve settles. Off: every frame, the numbers jump. A time in seconds: a running "
        "average over that time. Infinite: everything since the start or the last reset, for a whole song.",
        // 6: peak hold
        "Peak hold remembers the highest level at each frequency, then lets it fall at the chosen speed, or keeps "
        "it with Infinite. A resonance shows in both the level and the hold; a peak only in the hold was a short hit.",
        // 7: range
        "Range: how far below 0 dB the plot goes. Anything lower is spoken as below the range and is not searched "
        "for peaks. It also sets what counts as standing out: a twentieth of the range, 3 dB at 60, 6 dB at 120, "
        "because a wider range makes every bump smaller on screen.",
        // 8: FFT size
        "FFT size: more points give finer detail, above all in the lows, but react more slowly.",
        // 9: window
        "Window: the shape of the FFT window. Hann suits most work. Blackman-Harris and Kaiser keep quiet details "
        "next to loud peaks apart better.",
        // 10: overlap
        "Overlap: how often a new frame is computed. More overlap moves more smoothly and uses more processor time.",
        // 11: channel
        "Channel: what is analysed. Left, right, mid, side, or left and right as two curves.",
        // 12: display
        "Display: on screen, a curve or bars in third-octave or Bark bands. Speech does not change: the bands are "
        "always spoken in third octaves.",
        // 13: shape
        "Shape against the line: first the height of the line, where the curve lies on the plot, then the "
        "stretches of the curve that lie clearly above or below it, from low to high, with their average distance. "
        "It uses a broad 1/3 octave view whatever the Smoothing, like a first glance at the picture.",
        // 14: weighting (Periscope 1.1.0)
        "Weighting shows the spectrum through a standard curve instead of the Slope tilt. A, B, C "
        "and D are the curves of sound level meters, K is the loudness curve of LUFS, ITU-R 468 weighs noise as "
        "the ear hears it, ISO 226 is the equal-loudness contour at 80 phon, and the inverse X and N curves show "
        "what cinema rooms take away, with pink noise drawn flat. Every curve is 0 dB at 1 kHz. All levels spoken in this lens follow it, the bands "
        "too. Choose Slope to tilt by the Slope setting again.",
        // 15: masking reading (Periscope 1.1.0)
        "Masking: the frequency ranges that louder sounds nearby cover up, so the ear does not hear them in "
        "the mix, and the ranges below the threshold of hearing. On screen they are darkened. It follows the "
        "curve as drawn, so the Average setting sets how fast it changes; with Average off it is moment by "
        "moment. A lot of masking in one range means the parts there fight for the same place.",
        // 16: masking setting
        "Masking turns on the auditory masking estimate, the model that lossy audio codecs use to decide what "
        "the ear will not miss. It is an estimate, not a full model of hearing. The result is in the Masking "
        "row of the readings, and the probe and the peaks say when their frequency is masked."
    };

    int index = -1;
    if (group == 0) index = row == rrShape ? 13 : (row == rrMasking ? 15 : juce::jmin (row, 2));
    else if (group == 2)
    {
        switch (a11ySettingItem (row))
        {
            case slope:     index = 3;  break;
            case weightCurve: index = 14; break;
            case maskCtl:     index = 16; break;
            case smooth:    index = 4;  break;
            case average:   index = 5;  break;
            case hold:      index = 6;  break;
            case range:     index = 7;  break;
            case fftSize:   index = 8;  break;
            case window:    index = 9;  break;
            case overlap:   index = 10; break;
            case channel:   index = 11; break;
            case bandsMode: index = 12; break;
            default: break;
        }
    }
    return index >= 0 ? a11ytext::T (helps[index]) : juce::String();
}

// ---- the curve for speech ----
const SpectrumLens::A11yCurve& SpectrumLens::a11yCurve() const
{
    const int smoothIdx = processor.spectrumSmoothIndex();

    // Periscope 1.1.0: W of the chosen weighting on the speech grid. It changes with the settings, not with the
    // frames, so it is kept apart from the frame cache below.
    {
        const int n = gridSize();
        if ((int) curve.weight.size() != n || curve.weightCurve != sets.weighting
            || curve.weightSlope != sets.slopeDbPerOct || curve.weightSr != srSeen)
        {
            curve.weight.resize ((size_t) n);
            for (int i = 0; i < n; ++i)
                curve.weight[(size_t) i] = weighting::db (sets.weighting, gridHz (i), srSeen, sets.slopeDbPerOct);
            curve.weightCurve = sets.weighting;
            curve.weightSlope = sets.slopeDbPerOct;
            curve.weightSr    = srSeen;
        }
    }
    if (curveFrame == a11yFrameSerial && curveSmooth == smoothIdx) return curve;
    curveFrame  = a11yFrameSerial;
    curveSmooth = smoothIdx;

    const int n = gridSize();
    if ((int) curve.oct.size() != n)
    {
        curve.oct.resize ((size_t) n);
        for (int i = 0; i < n; ++i) curve.oct[(size_t) i] = (float) std::log2 (gridHz (i) / 1000.0);
    }
    curve.top.assign ((size_t) n, SpectrumFrame::kFloorDb);
    curve.topBin.assign ((size_t) n, -1);
    for (int s = 0; s < SpectrumFrame::kMaxSpectra; ++s)
    {
        curve.rawDb[s].assign ((size_t) n, SpectrumFrame::kFloorDb);
        curve.holdDb[s].assign ((size_t) n, SpectrumFrame::kFloorDb);
        curve.smoothDb[s].assign ((size_t) n, SpectrumFrame::kFloorDb);
    }
    if (! a11yHasFrame) return curve;

    // Each grid cell takes the loudest bin inside it, like a picture column; where the cell is narrower than a bin
    // (low end, small FFT) it interpolates in dB between the two neighbours, as rebuildColumns does.
    const double half = std::pow (2.0, 0.5 / kGridPerOct);
    std::vector<float> loudest ((size_t) n, SpectrumFrame::kFloorDb);
    for (int s = 0; s < a11ySpectra; ++s)
    {
        const auto& mag = a11yMag[s];
        const auto& hd  = a11yHold[s];
        if ((int) mag.size() < a11yBins || (int) hd.size() < a11yBins) continue;
        for (int i = 0; i < n; ++i)
        {
            const double f  = gridHz (i);
            const int    k0 = juce::jmax (1, (int) std::ceil (f / half / a11yBinHz));
            const int    k1 = juce::jmin (a11yBins - 1, (int) std::ceil (f * half / a11yBinHz) - 1);
            float m = SpectrumFrame::kFloorDb, h = SpectrumFrame::kFloorDb;
            int   best = -1;
            if (k1 >= k0)
            {
                for (int k = k0; k <= k1; ++k)
                {
                    if (mag[(size_t) k] > m) { m = mag[(size_t) k]; best = k; }
                    h = juce::jmax (h, hd[(size_t) k]);
                }
            }
            else
            {
                const double pos = f / a11yBinHz;
                const int    ka  = juce::jlimit (0, a11yBins - 1, (int) std::floor (pos));
                const int    kb  = juce::jmin (a11yBins - 1, ka + 1);
                const auto   t   = (float) juce::jlimit (0.0, 1.0, pos - (double) ka);
                m = mag[(size_t) ka] + t * (mag[(size_t) kb] - mag[(size_t) ka]);
                h = hd[(size_t) ka]  + t * (hd[(size_t) kb]  - hd[(size_t) ka]);
                best = t < 0.5f ? ka : kb;
            }
            curve.rawDb[s][(size_t) i]  = m;
            curve.holdDb[s][(size_t) i] = h;
            if (m > loudest[(size_t) i]) { loudest[(size_t) i] = m; curve.topBin[(size_t) i] = best; }
        }
    }

    // Smoothing: a moving average in dB over the chosen fraction of an octave, with the edges held, the same as
    // buildDeviceCurve does to the drawn curve (on a log axis a fraction of an octave is a fixed number of points).
    const int denom  = smoothDenom (smoothIdx);
    const int radius = denom <= 0 ? 0 : juce::jmax (1, (int) std::lround ((double) kGridPerOct / (2.0 * denom)));
    for (int s = 0; s < a11ySpectra; ++s)
    {
        const auto& src = curve.rawDb[s];
        auto&       dst = curve.smoothDb[s];
        if (radius == 0) { dst = src; continue; }
        double run = 0.0;
        for (int x = -radius; x <= radius; ++x) run += src[(size_t) juce::jlimit (0, n - 1, x)];
        for (int x = 0; x < n; ++x)
        {
            dst[(size_t) x] = (float) (run / (double) (2 * radius + 1));
            run -= src[(size_t) juce::jlimit (0, n - 1, x - radius)];
            run += src[(size_t) juce::jlimit (0, n - 1, x + radius + 1)];
        }
    }
    for (int i = 0; i < n; ++i)
    {
        float v = curve.smoothDb[0][(size_t) i];
        if (a11ySpectra >= 2) v = juce::jmax (v, curve.smoothDb[1][(size_t) i]);
        curve.top[(size_t) i] = v;
    }

    // The broad view for the line and the shape: the louder channel smoothed over 1/3 octave, whatever
    // the user's smoothing, so the overview holds still while the user zooms into details.
    {
        std::vector<float> louder ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            float v = curve.rawDb[0][(size_t) i];
            if (a11ySpectra >= 2) v = juce::jmax (v, curve.rawDb[1][(size_t) i]);
            louder[(size_t) i] = v;
        }
        movingAverage (louder, curve.broad, kGridPerOct / 6);
    }
    return curve;
}

float SpectrumLens::a11yCurveAt (const std::vector<float>& v, double hz) const
{
    if (v.empty()) return SpectrumFrame::kFloorDb;
    const double pos = juce::jlimit (0.0, (double) v.size() - 1.0, gridPos (hz));
    const int    i   = juce::jmin ((int) v.size() - 2, (int) pos);
    if (i < 0) return v[0];
    const auto t = (float) (pos - (double) i);
    return v[(size_t) i] + t * (v[(size_t) i + 1] - v[(size_t) i]);
}

// The probe level as the plot shows it, with the slope: the smoothed curve where it is drawn smoothed; otherwise the
// highest point within a quarter tone either side, so a narrow peak between two probe notes is not missed. The hold
// is never smoothed (as on screen), so it is the number the mouse box shows. The slope is linear in octaves, so
// tilting after smoothing gives the same curve as the screen, which tilts first.
float SpectrumLens::a11yProbeLevel (int slot, double hz, bool hold) const
{
    const auto& c = a11yCurve();
    if (! a11yHasFrame || slot >= a11ySpectra) return SpectrumFrame::kFloorDb;
    // Periscope: the weighting in place of the slope (with Slope it is the same tilt).
    if (! hold && smoothDenom (processor.spectrumSmoothIndex()) > 0)
        return a11yCurveAt (c.smoothDb[slot], hz) + weightAt (hz);

    const auto& v = hold ? c.holdDb[slot] : c.rawDb[slot];
    const int    n = (int) v.size();
    const double centre = gridPos (hz);
    const int    lo = juce::jlimit (0, n - 1, (int) std::ceil (centre - kGridPerOct / 24.0));
    const int    hi = juce::jlimit (0, n - 1, (int) std::floor (centre + kGridPerOct / 24.0));
    float db = a11yCurveAt (v, hz) + weightAt (hz);
    for (int i = lo; i <= hi; ++i) db = juce::jmax (db, v[(size_t) i] + c.weight[(size_t) i]);
    return db;
}

juce::String SpectrumLens::a11yLevels (double hz, bool withHold) const
{
    const int  floorDb = sets.rangeDb();
    const bool hold = withHold && sets.peakHold;
    const auto one = [&] (int slot)
    {
        auto t = spoken::db (a11yProbeLevel (slot, hz, false), floorDb);
        if (hold) t << a11ytext::T (", hold ") << spoken::db (a11yProbeLevel (slot, hz, true), floorDb);
        return t;
    };
    if (a11ySpectra >= 2) return a11ytext::T ("L ") + one (0) + a11ytext::T ("; R ") + one (1);
    return one (0);
}

// How far the probe is from the line, in whole dB (the louder channel, as on the plot). Nothing below the plot.
juce::String SpectrumLens::a11yLineDeviation (double hz) const
{
    const auto& sh = a11yShape();
    if (! sh.lineValid) return {};
    float level = a11yProbeLevel (0, hz, false);
    if (a11ySpectra >= 2) level = juce::jmax (level, a11yProbeLevel (1, hz, false));
    if (level <= -(float) sets.rangeDb()) return {};
    const int d = juce::roundToInt (level - sh.lineDb);
    if (d == 0) return a11ytext::T ("on the line");
    return a11ytext::TF (d > 0 ? "{0} dB above the line" : "{0} dB below the line", { juce::String (std::abs (d)) });
}

// How the curve leans on the plot: the signal's own tilt plus the slope. The line is flat, so rising to the right
// is brighter than the line and falling darker. Under a quarter of a dB per octave (2.5 dB over the whole range)
// it counts as flat.
juce::String SpectrumLens::a11yTiltValue() const
{
    const auto& sh = a11yShape();
    if (! sh.tiltValid) return a11ytext::T ("not enough signal");
    const float onPlot = sh.tiltDbPerOct + sh.weightTilt;   // Periscope: weightTilt is the slope with Slope
    if (std::abs (onPlot) < 0.25f) return a11ytext::T ("flat, follows the line");
    return a11ytext::TF (onPlot > 0.0f ? "rises {0} dB per octave, brighter than the line"
                                       : "falls {0} dB per octave, darker than the line",
                         { spoken::trimmed (std::abs (onPlot), 1) });
}

// Stretches near the line are left out, so silence about a range means it follows the line.
juce::String SpectrumLens::a11yShapeValue() const
{
    const auto& sh = a11yShape();
    if (! sh.lineValid) return a11ytext::T ("not enough signal");
    // The height first: the line is flat on the plot, so it is where the eye sees the curve lie.
    const auto line = a11ytext::TF ("line at {0}", { spoken::dB ((double) juce::roundToInt (sh.lineDb), 0) }) + "; ";
    if (sh.stretches.empty()) return line + a11ytext::TF ("all within {0} dB of the line", { spoken::trimmed (sh.threshold, 1) });
    return line + stretchesText (sh.stretches);
}

juce::String SpectrumLens::a11yFeatureValue (const Feature& f, bool dip) const
{
    const auto by = a11ytext::TF (dip ? "{0} dB below surroundings" : "{0} dB above surroundings",
                                  { juce::String (juce::jmax (1, juce::roundToInt (f.prominence))) });
    // Unsmoothed, a peak is a resonance or a note: its pitch and level matter, its width is a bin or two.
    if (smoothDenom (processor.spectrumSmoothIndex()) <= 0)
    {
        auto t = spoken::hz (f.hz) + ", " + spoken::noteWithCents (noteFor (f.hz)) + ", " + by + ", "
               + spoken::dB (f.db + weightAt (f.hz));
        const auto mask = a11yMaskAt (f.hz);
        return mask.isEmpty() ? t : t + ", " + mask;
    }
    // Smoothed, a hump or a hole has no exact pitch: where it starts and ends says more (an EQ band to set).
    const auto t = spoken::hz (f.hz) + ", " + hzRange (f.loHz, f.hiHz) + ", " + by;
    const auto mask = a11yMaskAt (f.hz);
    return mask.isEmpty() ? t : t + ", " + mask;
}

// Stretches of a curve of distances (one per grid point) at least the threshold away from zero. A stretch shorter
// than 1/3 octave is a detail, not shape (peaks and dips cover it); a gap shorter than 1/6 octave inside one stretch
// does not split it. At most 6 stretches, the biggest (distance times width), then back in frequency order.
std::vector<SpectrumLens::Stretch> SpectrumLens::findStretches (const std::vector<float>& dev, float threshold)
{
    const int n = (int) dev.size();
    std::vector<int> side ((size_t) n);
    for (int i = 0; i < n; ++i)
        side[(size_t) i] = dev[(size_t) i] >= threshold ? 1 : (dev[(size_t) i] <= -threshold ? -1 : 0);

    struct Run { int lo, hi, side; };
    const auto runsOf = [&]
    {
        std::vector<Run> runs;
        for (int i = 0; i < n; ++i)
        {
            if (runs.empty() || runs.back().side != side[(size_t) i]) runs.push_back ({ i, i, side[(size_t) i] });
            else runs.back().hi = i;
        }
        return runs;
    };
    for (const auto& r : runsOf())
        if (r.side != 0 && r.hi - r.lo + 1 < kGridPerOct / 3)
            std::fill (side.begin() + r.lo, side.begin() + r.hi + 1, 0);
    auto runs = runsOf();
    for (size_t k = 1; k + 1 < runs.size(); ++k)
        if (runs[k].side == 0 && runs[k - 1].side != 0 && runs[k - 1].side == runs[k + 1].side
            && runs[k].hi - runs[k].lo + 1 < kGridPerOct / 6)
            std::fill (side.begin() + runs[k].lo, side.begin() + runs[k].hi + 1, runs[k - 1].side);
    runs = runsOf();

    std::vector<std::pair<float, Stretch>> found;   // (size, stretch)
    for (const auto& r : runs)
    {
        if (r.side == 0) continue;
        double sum = 0.0;
        for (int i = r.lo; i <= r.hi; ++i) sum += dev[(size_t) i];
        Stretch st;
        st.loHz = gridHz (r.lo);
        st.hiHz = gridHz (r.hi);
        st.devDb = (float) (sum / (double) (r.hi - r.lo + 1));
        st.fromBottom = r.lo == 0;
        st.toTop = r.hi == n - 1;
        found.push_back ({ std::abs (st.devDb) * (float) (r.hi - r.lo + 1), st });
    }
    if (found.size() > 6)
    {
        std::sort (found.begin(), found.end(), [] (const auto& x, const auto& y) { return x.first > y.first; });
        found.resize (6);
        std::sort (found.begin(), found.end(), [] (const auto& x, const auto& y) { return x.second.loHz < y.second.loHz; });
    }
    std::vector<Stretch> out;
    for (const auto& f : found) out.push_back (f.second);
    return out;
}

// "35 to 80 Hz +5 dB, 85 to 400 Hz -4 dB": low to high, as the eye sweeps the picture.
juce::String SpectrumLens::stretchesText (const std::vector<Stretch>& stretches) const
{
    juce::StringArray parts;
    for (const auto& st : stretches)
    {
        const auto where = st.fromBottom ? a11ytext::TF ("up to {0}", { spoken::hz (roundHz (st.hiHz)) })
                         : (st.toTop ? a11ytext::TF ("above {0}", { spoken::hz (roundHz (st.loHz)) }) : hzRange (st.loHz, st.hiHz));
        parts.add (where + " " + spoken::signedDb ((float) juce::roundToInt (st.devDb)));
    }
    return parts.joinIntoString (", ");
}

// ---- the line, the tilt, the peaks and dips ----
const SpectrumLens::A11yShape& SpectrumLens::a11yShape() const
{
    const auto& c = a11yCurve();
    const int   smoothIdx = processor.spectrumSmoothIndex();
    const auto  now = juce::Time::getMillisecondCounter();
    const bool  settingsChanged = smoothIdx != shapeSmooth || sets.slopeDbPerOct != shapeSlope || sets.rangeDb() != shapeRange
                               || sets.weighting != shapeWeighting;
    const bool  stale = shapeFrame != a11yFrameSerial && (now - shapeTimeMs >= kShapeHoldMs || shapeHadFrame != a11yHasFrame);
    if (! settingsChanged && ! stale) return shape;

    shapeFrame    = a11yFrameSerial;
    shapeSmooth   = smoothIdx;
    shapeSlope    = sets.slopeDbPerOct;
    shapeWeighting = sets.weighting;
    shapeRange    = sets.rangeDb();
    shapeTimeMs   = now;
    shapeHadFrame = a11yHasFrame;

    shape = {};
    const int denom = smoothDenom (smoothIdx);
    // What counts as standing out is what the eye would notice: an analyser has no threshold, the eye sees a bump
    // when it has some height on screen. That height follows the Range: the plot always spans the same pixels, so
    // a 3 dB bump is a twentieth of the plot at 60 dB and half that at 120 dB. A twentieth of the range it is,
    // the same at every smoothing; the smoothing alone decides how many bumps there are, as on screen.
    shape.threshold = (float) sets.rangeDb() / 20.0f;
    const int n = (int) c.top.size();
    if (! a11yHasFrame || n < 3) return shape;

    // The curve as it is drawn: louder channel, smoothed, with the slope, held at the edges of the plot.
    const float floorDb = -(float) sets.rangeDb();
    const auto  onPlot = [&] (float db, int i) { return juce::jlimit (floorDb, 0.0f, db + c.weight[(size_t) i]); };
    std::vector<float> view ((size_t) n), scratch;
    for (int i = 0; i < n; ++i) view[(size_t) i] = onPlot (c.top[(size_t) i], i);

    // The broad view for the line and the shape (see a11yCurve), with the slope, held at the edges of the plot.
    std::vector<float> broad ((size_t) n);
    for (int i = 0; i < n; ++i) broad[(size_t) i] = onPlot (c.broad[(size_t) i], i);

    // THE LINE: flat on the plot, at the median height of the curve between 40 Hz and 16 kHz. The median, not the
    // mean: a few big humps (a kick, a bass note) must not lift the line the rest is compared with.
    {
        const int lo = juce::jlimit (0, n - 1, (int) std::lround (gridPos (40.0)));
        const int hi = juce::jlimit (lo, n - 1, (int) std::lround (gridPos (16000.0)));
        shape.lineDb    = medianOf (broad, lo, hi, scratch);
        shape.lineValid = shape.lineDb > floorDb + 1.0f;
    }

    // THE SHAPE: stretches of the broad curve above or below the line by the threshold.
    if (shape.lineValid)
    {
        std::vector<float> dev ((size_t) n);
        for (int i = 0; i < n; ++i) dev[(size_t) i] = broad[(size_t) i] - shape.lineDb;
        shape.stretches = findStretches (dev, shape.threshold);
    }

    // SIGNAL TILT: a straight-line fit of the third-octave band levels from 50 Hz to 10 kHz. Band power is flat for
    // pink noise while a bin falls 3 dB per octave, so the fit minus 3 is the tilt in the terms of the Slope
    // setting: pink noise falls 3 dB per octave, and a Slope of 3 makes it flat.
    //
    // The eye judges the lean past a narrow spike or notch (a tone, a resonance): one loud band would pull a plain
    // fit towards it (a tone at 440 Hz, left of the middle, made a flat noise "fall"). So a band further than the
    // threshold from the median of its neighbours (3 bands either side, about two octaves) counts at that median.
    // Broad humps span more than half that window and are kept.
    {
        std::vector<float> xs, ys;
        for (int b = 0; b < SpectrumFrame::kNumThird; ++b)
        {
            const double fc = kThirdOctaveHz[b];
            if (fc < 50.0 || fc > 10000.0) continue;
            double power = 0.0;
            for (int s = 0; s < a11ySpectra; ++s) power += std::pow (10.0, (double) bandDb[s][b] / 10.0);
            const double db = 10.0 * std::log10 (juce::jmax (1.0e-30, power / (double) juce::jmax (1, a11ySpectra)));
            if (db < (double) floorDb) continue;   // under the plot: noise floor, not signal
            xs.push_back ((float) std::log2 (fc / 1000.0));
            ys.push_back ((float) db);
        }
        // Periscope: how the weighting itself leans over the same span, a straight-line fit at the same band
        // centres. With Slope it is exactly the slope; a curve such as A leans and bends, and the eye sees the
        // lean of the result.
        {
            double wx = 0.0, wy = 0.0, wxx = 0.0, wxy = 0.0;
            int    wn = 0;
            for (int b = 0; b < SpectrumFrame::kNumThird; ++b)
            {
                const double fc = kThirdOctaveHz[b];
                if (fc < 50.0 || fc > 10000.0) continue;
                const double x = std::log2 (fc / 1000.0), y = weightAt (fc);
                wx += x; wy += y; wxx += x * x; wxy += x * y; ++wn;
            }
            const double wden = (double) wn * wxx - wx * wx;
            shape.weightTilt = wden > 1.0e-9 ? (float) (((double) wn * wxy - wx * wy) / wden) : 0.0f;
        }
        const int count = (int) ys.size();
        double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
        for (int i = 0; i < count; ++i)
        {
            const float med = medianOf (ys, juce::jmax (0, i - 3), juce::jmin (count - 1, i + 3), scratch);
            const double x  = xs[(size_t) i];
            const double y  = std::abs (ys[(size_t) i] - med) > shape.threshold ? med : ys[(size_t) i];
            sx += x; sy += y; sxx += x * x; sxy += x * y;
        }
        const double den = (double) count * sxx - sx * sx;
        if (count >= 8 && den > 1.0e-9)
        {
            shape.tiltDbPerOct = (float) (((double) count * sxy - sx * sy) / den - 10.0 * std::log10 (2.0));
            shape.tiltValid = true;
        }
    }

    // PEAKS AND DIPS on the drawn curve: local maxima (minima) that stand out by the threshold. The search reaches
    // about 8 times the smoothing width either side, at least 1/3 octave, so the list holds details of the size
    // the smoothing shows. Dips only on a smoothed curve: unsmoothed, every gap between two bins would be one.
    //
    // How much a hump stands out is measured as the eye sees it: above the higher of the two valleys beside it
    // (walking each way until the curve rises above the hump or the search ends); a hole, below the lower of the
    // two crests beside it. Against the median of the neighbourhood, humps and holes that alternate would each get
    // only half their height. Unsmoothed the curve is grass, and valleys between bins are deep everywhere, so there
    // a resonance is measured against the median of its neighbourhood instead.
    const double scaleOct = denom > 0 ? 1.0 / denom : 1.0 / 48.0;
    const int    halfWin  = juce::jmax (kGridPerOct / 6, (int) std::lround (kGridPerOct * 8.0 * scaleOct / 2.0));

    // Periscope: unsmoothed, a tone in the bass is as wide as the FFT window's main lobe (about 3 bins: 35 Hz at 60 Hz with
    // FFT 4096), wider than 1/6 octave, so the median of a 1/6-octave neighbourhood sat on the hump and a pure bass tone
    // was not listed at all. The neighbourhood is at least 6 bins either side there (at most 2 octaves).
    const double binHz = fftSizeSeen > 0 ? srSeen / (double) fftSizeSeen : 0.0;
    const auto windowAt = [&] (int i)
    {
        if (denom > 0 || binHz <= 0.0) return halfWin;
        const double gridPerHz = kGridPerOct / (gridHz (i) * std::log (2.0));
        return juce::jlimit (halfWin, 2 * kGridPerOct, (int) std::lround (6.0 * binHz * gridPerHz));
    };

    const auto standsOut = [&] (int i, bool dip)
    {
        const int   w  = windowAt (i);
        const int   lo = juce::jmax (0, i - w), hi = juce::jmin (n - 1, i + w);
        const float v  = view[(size_t) i];
        if (denom <= 0)
        {
            // Periscope: the median leaves out the window's main lobe around the candidate (2.5 bins for Hann, 4.5 for
            // the others), or with FFT 1024 a 60 Hz tone's lobe still filled half the neighbourhood and the median sat on
            // the hump. At the 20 Hz edge only the other side may remain; if nothing remains, the whole window is used.
            float m = 0.0f;
            bool  got = false;
            if (binHz > 0.0)
            {
                const double lobeBins = sets.window == Spectrum::hann ? 2.5 : 4.5;
                const int    gap = (int) std::lround (lobeBins * binHz * kGridPerOct / (gridHz (i) * std::log (2.0)));
                scratch.clear();
                for (int j = lo; j <= hi; ++j)
                    if (std::abs (j - i) > gap) scratch.push_back (view[(size_t) j]);
                if (! scratch.empty())
                {
                    const auto mid = scratch.begin() + (std::ptrdiff_t) (scratch.size() / 2);
                    std::nth_element (scratch.begin(), mid, scratch.end());
                    m = *mid;
                    got = true;
                }
            }
            if (! got) m = medianOf (view, lo, hi, scratch);
            return dip ? m - v : v - m;
        }
        const auto sideBase = [&] (int step)
        {
            float extreme = v;
            for (int j = i + step; j >= lo && j <= hi; j += step)
            {
                const float w = view[(size_t) j];
                if (dip ? w < v : w > v) break;
                extreme = dip ? juce::jmax (extreme, w) : juce::jmin (extreme, w);
            }
            return extreme;
        };
        const float left = sideBase (-1), right = sideBase (1);
        return dip ? juce::jmin (left, right) - v : v - juce::jmax (left, right);
    };

    struct Candidate { int i; float prominence; };
    std::vector<Candidate> peakCands, dipCands;
    for (int i = 1; i < n - 1; ++i)
    {
        const float v = view[(size_t) i];
        const bool isPeak = v > view[(size_t) i - 1] && v >= view[(size_t) i + 1];
        const bool isDip  = denom > 0 && v < view[(size_t) i - 1] && v <= view[(size_t) i + 1];
        if (isPeak && v > floorDb)
        {
            const float p = standsOut (i, false);
            if (p >= shape.threshold) peakCands.push_back ({ i, p });
        }
        if (isDip)
        {
            const float p = standsOut (i, true);
            if (p >= shape.threshold && v + p > floorDb + shape.threshold) dipCands.push_back ({ i, p });
        }
    }

    const auto describe = [&] (const Candidate& cand, bool dip)
    {
        const int i = cand.i;
        Feature f;
        f.prominence = cand.prominence;

        // Where the sides cross half of that height: the start and end of the hump or the hole.
        const float edge = dip ? view[(size_t) i] + 0.5f * cand.prominence : view[(size_t) i] - 0.5f * cand.prominence;
        const auto inside = [&] (int j) { return dip ? view[(size_t) j] < edge : view[(size_t) j] > edge; };
        int l = i, r = i;
        while (l > 0 && inside (l - 1)) --l;
        while (r < n - 1 && inside (r + 1)) ++r;
        f.loHz = gridHz (l);
        f.hiHz = gridHz (r);

        // Between grid points: a parabola through the three points.
        const double a = view[(size_t) i - 1], b = view[(size_t) i], e = view[(size_t) i + 1];
        const double den = a - 2.0 * b + e;
        const double p = std::abs (den) > 1.0e-9 ? juce::jlimit (-0.5, 0.5, 0.5 * (a - e) / den) : 0.0;
        f.hz = gridHz ((double) i + p);
        f.db = c.top[(size_t) i];

        // An unsmoothed peak is refined on the bins themselves (louder channel), or on a small FFT it would jump
        // along the bin grid: climb to the top bin, then a parabola over three bins.
        int k = c.topBin[(size_t) i];
        if (! dip && denom <= 0 && k >= 1 && k <= a11yBins - 2 && (int) a11yMag[0].size() >= a11yBins)
        {
            const auto lvl = [&] (int kk)
            {
                float v = a11yMag[0][(size_t) kk];
                if (a11ySpectra >= 2 && (int) a11yMag[1].size() > kk) v = juce::jmax (v, a11yMag[1][(size_t) kk]);
                return (double) v;
            };
            for (int guard = 0; guard < 4 && k < a11yBins - 2 && lvl (k + 1) > lvl (k); ++guard) ++k;
            for (int guard = 0; guard < 4 && k > 1 && lvl (k - 1) > lvl (k); ++guard) --k;
            const double ka = lvl (k - 1), kb = lvl (k), kc = lvl (k + 1);
            const double d2 = ka - 2.0 * kb + kc;
            const double q  = d2 < 0.0 ? juce::jlimit (-0.5, 0.5, 0.5 * (ka - kc) / d2) : 0.0;
            f.hz = ((double) k + q) * a11yBinHz;
            f.db = (float) (kb - 0.25 * (ka - kc) * q);
        }
        return f;
    };

    // The most prominent first; two features closer than twice the smoothing width (at least a semitone) are one.
    // All of them are counted, so the Smoothing row can say how busy the picture is (unsmoothed, dozens of
    // spikes; at 1/6 octave, a few humps); the list keeps the most prominent, as the eye picks the tallest spikes.
    const double minApart = juce::jmax (1.0 / 12.0, 2.0 * scaleOct);
    const auto pick = [&] (std::vector<Candidate>& cands, bool dip, int maxCount, std::vector<Feature>& out)
    {
        std::sort (cands.begin(), cands.end(), [] (const Candidate& x, const Candidate& y) { return x.prominence > y.prominence; });
        for (const auto& cand : cands)
        {
            const auto f = describe (cand, dip);
            if (f.hz < kMinHz || f.hz > kMaxHz) continue;
            const bool tooClose = std::any_of (out.begin(), out.end(), [&] (const Feature& o)
                                               { return std::abs (std::log2 (f.hz / o.hz)) < minApart; });
            if (! tooClose) out.push_back (f);
        }
        const int total = (int) out.size();
        if (total > maxCount) out.resize ((size_t) maxCount);
        return total;
    };
    shape.totalPeaks = pick (peakCands, false, kMaxPeaks, shape.peaks);
    shape.totalDips  = pick (dipCands,  true,  kMaxDips,  shape.dips);
    return shape;
}

juce::String SpectrumLens::a11yBandValue (int band) const
{
    // Periscope: a band with no FFT bin inside (40 Hz at 48 kHz and FFT 4096) is drawn as "not measurable", not as a
    // bar at the floor; speech said "below -90 dB", as if it were silent. Same test as paintBars.
    constexpr double kSixthDown = 0.8908987181403393, kSixthUp = 1.1224620483093730;   // 2^(-1/6), 2^(+1/6)
    const double binHz = fftSizeSeen > 0 ? srSeen / (double) fftSizeSeen : 0.0;
    const double bandLo = kThirdOctaveHz[band] * kSixthDown, bandHi = kThirdOctaveHz[band] * kSixthUp;
    if (binHz > 0.0 && std::ceil (bandHi / binHz) <= std::ceil (bandLo / binHz))
        return a11ytext::T ("not measurable at this FFT size");

    const int   floorDb = sets.rangeDb();
    const float w = bandWeight (kThirdOctaveHz[band]);   // Periscope: weighted as the bars are, 0 with Slope
    auto t = a11ySpectra >= 2
           ? a11ytext::T ("L ") + spoken::db (bandDb[0][band] + w, floorDb) + a11ytext::T (", R ") + spoken::db (bandDb[1][band] + w, floorDb)
           : spoken::db (bandDb[0][band] + w, floorDb);
    // A band narrower than about three bins is narrower than the window's main lobe: noise reads right there, but a
    // lone tone spreads past the band edges and reads lower than it is. Say so.
    if (binHz > 0.0 && bandHi - bandLo < 3.0 * binHz)
        t << ", " << a11ytext::T ("limited by the FFT size");
    return t;
}

juce::String SpectrumLens::a11yControlValue (int item) const
{
    const auto s = processor.spectrumSettings();
    switch (item)
    {
        case fftSize: return juce::String (s.fftSize()) + a11ytext::T (" points");
        case window:
            return s.window == Spectrum::hann ? a11ytext::T ("Hann")
                 : (s.window == Spectrum::blackmanHarris4 ? a11ytext::T ("Blackman-Harris") : a11ytext::T ("Kaiser"));
        case overlap: return juce::String (juce::roundToInt (s.overlap() * 100.0f)) + a11ytext::T (" percent");
        case channel:
            switch (s.channel)
            {
                case Spectrum::left:  return a11ytext::T ("Left");
                case Spectrum::right: return a11ytext::T ("Right");
                case Spectrum::mid:   return a11ytext::T ("Mid");
                case Spectrum::side:  return a11ytext::T ("Side");
                default:              return a11ytext::T ("Left and right");
            }
        case bandsMode:
            return s.bandsMode == Spectrum::bandsThird ? a11ytext::T ("Third-octave bars")
                 : (s.bandsMode == Spectrum::bandsBark ? a11ytext::T ("Bark bars") : a11ytext::T ("FFT curve"));
        case slope:
        {
            // With its effect, so a change is heard at once: how the curve now leans on the plot.
            auto t = spoken::trimmed (s.slopeDbPerOct, 1) + a11ytext::T (" dB per octave");
            // Periscope: a weighting curve replaces the slope; the row says so instead of a tilt that is not drawn.
            if (s.weighting != weighting::slope)
                return a11ytext::TF ("{0}, not used. Weighting: {1}", { t, a11yWeightingName (s.weighting) });
            if (a11yHasFrame && a11yShape().tiltValid) t << ". " << a11ytext::T ("Signal tilt") << ", " << a11yTiltValue();
            return t;
        }
        case maskCtl: return s.masking ? a11ytext::T ("On") : a11ytext::T ("Off");
        case weightCurve:
        {
            // With its effect, as the slope row: how the curve now leans on the plot.
            auto t = a11yWeightingName (s.weighting);
            if (s.weighting == weighting::slope)
                t << ", " << spoken::trimmed (s.slopeDbPerOct, 1) << a11ytext::T (" dB per octave");
            if (a11yHasFrame && a11yShape().tiltValid) t << ". " << a11ytext::T ("Signal tilt") << ", " << a11yTiltValue();
            return t;
        }
        case average:
            if (s.avgMode == Spectrum::avgNone)     return a11ytext::T ("Off");
            if (s.avgMode == Spectrum::avgInfinite) return a11ytext::T ("Infinite");
            return spoken::trimmed (s.avgSeconds, 1) + a11ytext::T (" seconds");
        case hold:
            if (! s.peakHold)                return a11ytext::T ("Off");
            if (s.holdDecayDbPerSec <= 0.0f) return a11ytext::T ("Infinite");
            return spoken::trimmed (s.holdDecayDbPerSec, 0) + a11ytext::T (" dB per second");
        case range: return juce::String (s.rangeDb()) + a11ytext::T (" dB");
        case smooth:
        {
            // With its effect: how many peaks and dips this smoothing leaves, the spoken amount of detail.
            const int denom = smoothDenom (processor.spectrumSmoothIndex());
            // A template, not "1/" + " octave": a fraction of an octave takes another case than "250 Hz octave" in some languages.
            auto t = denom <= 0 ? juce::String (a11ytext::T ("Off")) : a11ytext::TF ("1/{0} octave", { juce::String (denom) });
            if (a11yHasFrame)
            {
                const auto& sh = a11yShape();
                const auto np = juce::String (sh.totalPeaks), nd = juce::String (sh.totalDips);
                t << ", " << (denom <= 0 ? a11ytext::TF ("peaks: {0}", { np }) : a11ytext::TF ("peaks: {0}, dips: {1}", { np, nd }));
            }
            return t;
        }
        default: return {};
    }
}

// Steps a setting forward or back. Unlike the mouse click (cycleControl) it does not wrap: at the edge
// the value stays and the editor is silent.
bool SpectrumLens::stepControl (int control, int step)
{
    auto s = processor.spectrumSettings();

    // Peak hold options in the same order as the mouse: off, 12 dB/s, 30 dB/s, infinite.
    const auto holdIndex = [] (const Spectrum::Settings& x)
    {
        if (! x.peakHold) return 0;
        if (x.holdDecayDbPerSec <= 0.0f) return 3;
        return x.holdDecayDbPerSec >= 30.0f ? 2 : 1;
    };
    const int slopeSteps = juce::roundToInt ((Spectrum::kMaxSlopeDbPerOct - Spectrum::kMinSlopeDbPerOct) / Spectrum::kSlopeStep);

    int idx = 0, n = 1;
    switch (control)
    {
        case fftSize:   idx = s.fftOrder - Spectrum::kMinFftOrder; n = Spectrum::kMaxFftOrder - Spectrum::kMinFftOrder + 1; break;
        case window:    idx = s.window;       n = Spectrum::kNumWindows;   break;
        case overlap:   idx = s.overlapIndex; n = Spectrum::kNumOverlaps;  break;
        case channel:   idx = s.channel;      n = Spectrum::kNumChannels;  break;
        case bandsMode: idx = s.bandsMode;    n = Spectrum::kNumBandModes; break;
        case slope:     idx = juce::roundToInt ((s.slopeDbPerOct - Spectrum::kMinSlopeDbPerOct) / Spectrum::kSlopeStep); n = slopeSteps + 1; break;
        case average:   idx = s.avgMode;      n = Spectrum::kNumAverages;  break;
        case hold:      idx = holdIndex (s);  n = 4; break;
        case range:     idx = s.rangeDbIndex; n = Spectrum::kNumRanges;    break;
        case smooth:    idx = processor.spectrumSmoothIndex(); n = TelescopeProcessor::kNumSpectrumSmoothOptions; break;
        case weightCurve: idx = s.weighting; n = weighting::kNumCurves; break;
        case maskCtl:     idx = s.masking ? 1 : 0; n = 2; break;
        default: return false;
    }

    // Periscope: as with the mouse, the Slope row first brings the slope back when a weighting curve is on.
    if (control == slope && s.weighting != weighting::slope)
    {
        s.weighting = weighting::slope;
        processor.setSpectrumSettings (s);
        sets = processor.spectrumSettings();
        invalidateStatic();
        repaint();
        return true;
    }

    const int size = std::abs (step) >= a11yui::kExtreme ? n : (std::abs (step) >= a11yui::kCoarse ? 2 : 1);
    const int next = juce::jlimit (0, n - 1, idx + (step > 0 ? size : -size));
    if (next == idx) return false;

    switch (control)
    {
        case fftSize:   s.fftOrder = Spectrum::kMinFftOrder + next; break;
        case window:    s.window = next; break;
        case overlap:   s.overlapIndex = next; break;
        case channel:   s.channel = next; break;
        case bandsMode: s.bandsMode = next; break;
        case slope:     s.slopeDbPerOct = Spectrum::kMinSlopeDbPerOct + (float) next * Spectrum::kSlopeStep; break;
        case average:   s.avgMode = next; break;
        case hold:
            s.peakHold = next != 0;
            s.holdDecayDbPerSec = next == 2 ? 30.0f : (next == 3 ? 0.0f : 12.0f);
            break;
        case range:     s.rangeDbIndex = next; break;
        case weightCurve: s.weighting = next; break;
        case maskCtl:     s.masking = next != 0; break;
        case smooth:
            // Smoothing is for drawing only and is not part of the engine settings (as in cycleControl).
            processor.setSpectrumSmoothIndex (next);
            repaint();
            return true;
        default: return false;
    }
    processor.setSpectrumSettings (s);
    sets = processor.spectrumSettings();
    invalidateStatic();
    repaint();
    return true;
}

// ---- Periscope 1.1.0: the weighting ----
float SpectrumLens::weightAt (double hz) const
{
    return weighting::db (sets.weighting, hz, srSeen, sets.slopeDbPerOct);
}

float SpectrumLens::bandWeight (double centreHz) const
{
    return sets.weighting == weighting::slope ? 0.0f : weightAt (centreHz);
}

// The spoken name of a curve. The letters are said as in a meter manual: "A-weighting".
juce::String SpectrumLens::a11yWeightingName (int curve)
{
    static const char* const names[weighting::kNumCurves] = { "Slope", "A-weighting", "B-weighting", "C-weighting",
                                                             "D-weighting", "K-weighting", "ITU-R 468",
                                                             "ISO 226, 80 phon", "inverse X-curve",
                                                             "inverse N-curve" };
    return a11ytext::T (names[juce::jlimit (0, weighting::kNumCurves - 1, curve)]);
}

// ---- Periscope 1.1.0: auditory masking ----
// Masked partitions are darkened over the whole height of the plot, those below the threshold of hearing less:
// the picture keeps its curve and the eye sees which ranges the ear loses (Prism draws them semi-transparent).
void SpectrumLens::paintMasking (juce::Graphics& g) const
{
    if (! sets.masking || maskParts <= 0) return;
    g.saveState();
    g.reduceClipRegion (zones.plot);
    for (int i = 0; i < maskParts; ++i)
    {
        if (maskState[i] == AuditoryMasking::audible) continue;
        const float xa = xForFreq (maskLoHz[i]), xb = xForFreq (maskHiHz[i]);
        if (xb <= xa) continue;
        g.setColour (look::maskShade (maskState[i] == AuditoryMasking::masked));
        g.fillRect (juce::Rectangle<float> (xa, (float) zones.plot.getY(), xb - xa, (float) zones.plot.getHeight()));
    }
    g.restoreState();
}

juce::String SpectrumLens::a11yMaskAt (double hz) const
{
    if (! sets.masking) return {};
    for (int i = 0; i < maskParts; ++i)
        if (hz >= maskLoHz[i] && hz < maskHiHz[i])
            return maskState[i] == AuditoryMasking::masked ? a11ytext::T ("masked")
                 : (maskState[i] == AuditoryMasking::belowHearing ? a11ytext::T ("below hearing") : juce::String());
    return {};
}

// Neighbouring partitions in the same state are one range, low to high. At most 8 ranges per state are named,
// the widest (in octaves), then back in frequency order; the rest are counted.
juce::String SpectrumLens::a11yMaskingValue() const
{
    if (maskParts <= 0) return a11ytext::T ("no data yet");
    const auto rangesOf = [&] (int state)
    {
        struct R { double lo, hi; bool fromBottom, toTop; };
        std::vector<R> rs;
        for (int i = 0; i < maskParts; ++i)
        {
            if (maskState[i] != state) continue;
            if (! rs.empty() && i > 0 && maskState[i - 1] == state) rs.back().hi = maskHiHz[i];
            else rs.push_back ({ (double) maskLoHz[i], (double) maskHiHz[i], i == 0, false });
            rs.back().toTop = i == maskParts - 1;
        }
        const int total = (int) rs.size();
        if (total > 8)
        {
            std::sort (rs.begin(), rs.end(), [] (const R& a, const R& b) { return a.hi / a.lo > b.hi / b.lo; });
            rs.resize (8);
            std::sort (rs.begin(), rs.end(), [] (const R& a, const R& b) { return a.lo < b.lo; });
        }
        juce::StringArray parts;
        for (const auto& r : rs)
            parts.add (r.fromBottom ? a11ytext::TF ("up to {0}", { spoken::hz (roundHz (r.hi)) })
                     : (r.toTop ? a11ytext::TF ("above {0}", { spoken::hz (roundHz (r.lo)) }) : hzRange (r.lo, r.hi)));
        auto t = parts.joinIntoString (", ");
        if (total > 8) t << ", " << a11ytext::TF ("and {0} more", { juce::String (total - 8) });
        return t;
    };
    const auto masked = rangesOf (AuditoryMasking::masked), below = rangesOf (AuditoryMasking::belowHearing);
    auto t = masked.isEmpty() ? a11ytext::T ("nothing masked") : a11ytext::TF ("masked: {0}", { masked });
    if (below.isNotEmpty()) t << "; " << a11ytext::TF ("below hearing: {0}", { below });
    return t;
}
}
