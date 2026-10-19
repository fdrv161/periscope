#include "lenses/TonalBalanceLens.h"
#include "lenses/Look.h"
#include "PluginProcessor.h"
#include "lenses/LensReadout.h"
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

constexpr double kSixthDown = 0.8908987181403393;   // 2^(-1/6)
constexpr double kSixthUp   = 1.1224620483093730;   // 2^(+1/6)

constexpr double kLabelledHz[] = { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 };

// Los formatos que el chooser OFRECE. No es la lista de lo que se puede leer (un archivo arrastrado se
// intenta igual, tenga la extensión que tenga): es lo que se muestra por default para no hacer buscar.
constexpr const char* kAudioFilter = "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3;*.m4a;*.aac;*.caf;*.wma";

juce::String shortHz (double hz)
{
    if (hz >= 1000.0)
    {
        const double k = hz / 1000.0;
        return juce::String (k, k < 10.0 && std::abs (k - std::round (k)) > 0.01 ? 1 : 0) + "k";
    }
    return juce::String (hz, hz < 100.0 && std::abs (hz - std::round (hz)) > 0.01 ? 1 : 0);
}

juce::String signed1 (float v) { return juce::String (v > 0.0f ? "+" : "") + juce::String (v, 1); }

const juce::String kDot = juce::String::fromUTF8 ("  \xc2\xb7  ");

// F4 (T6): segundos → «m:ss», redondeado al segundo. Es lo que se lee en el rótulo de la tira.
juce::String mmss (double seconds)
{
    const auto t = (long long) std::llround (juce::jmax (0.0, seconds));
    return juce::String (t / 60) + ":" + juce::String (t % 60).paddedLeft ('0', 2);
}

// Un arrastre de menos de estos píxeles es un clic, no un tramo (y el doble clic trae dos clics).
constexpr int kMinDragPx = 4;
}

const juce::ValueTree& TonalBalanceLens::stateTree() const { return processor.apvts.state; }

TonalBalanceLens::TonalBalanceLens (TelescopeProcessor& p) : Lens (30), processor (p)
{
    // settleHold 2 (antes 30): advanceFrame cuenta como cambio todo lo que se aparta del último cuadro
    // mostrado (ver ahí), así que no hace falta cola para no perderse el final de una animación.
    setSettleHold (2);
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    for (int b = 0; b < ReferenceFrame::kNumBands; ++b)
    {
        dispLive[b] = dispRef[b] = kNormBottomDb;
        dispDelta[b] = 0.0f;
    }
    // Accessibility: track the analysis from the current state, so opening the lens does not say "done"
    // about an old result.
    a11yLastRevision = processor.referenceRevision();
    a11yWasBusy      = processor.referenceBusy();
}

// Do not lose a pending section (the user stepped with the arrows and left the lens right away).
// Periscope (audit H1): the file dialog goes first, while the whole lens is still alive. Destroying an open
// Windows file dialog pumps the message queue until its thread ends; a frame in that loop used to reach the
// already destroyed waveform reader. The frame timer is stopped before that for the same reason.
TonalBalanceLens::~TonalBalanceLens()
{
    stopAnimation();
    chooser.reset();
    a11yApplyPendingSection();
}

//======================================================================================== geometría
TonalBalanceLens::Zones TonalBalanceLens::zonesFor (int w, int h) const
{
    Zones z;
    auto body = juce::Rectangle<int> (0, 0, w, h).reduced (th::padIn);

    const int rowH = juce::jlimit (22, 30, h / 22);
    z.footer = body.removeFromBottom (rowH);
    body.removeFromBottom (th::padIn / 2);

    auto foot = z.footer;
    const int gap = 6;
    const int bw = juce::jmin (150, (foot.getWidth() - gap * (kNumControls - 1)) / kNumControls);
    for (int i = 0; i < kNumControls; ++i)
    {
        z.button[i] = foot.removeFromLeft (bw);
        foot.removeFromLeft (gap);
    }

    z.head = body.removeFromTop (kHeadH);
    body.removeFromTop (6);

    // F4 (T6): la tira de la referencia, entre la cabecera y las curvas. Sólo con una referencia cargada:
    // sin referencia la lente queda como estaba.
    if (wantsStrip())
    {
        z.strip = body.removeFromTop (juce::jlimit (30, 46, h / 14));
        body.removeFromTop (8);
    }

    auto axis = body.removeFromBottom (kAxisH);

    // Las curvas se llevan más alto que el delta: son las que se miran primero, y son las que tienen que
    // poder leerse como un espectro.
    const int curvesH = juce::jmax (60, (int) std::lround (0.58 * (double) body.getHeight()));
    auto top = body.removeFromTop (curvesH);
    // padIn ENTERO entre los dos paneles (no la mitad): abajo del primero termina la etiqueta "-42" y
    // arriba del segundo empieza "+12.0"; con 8 px las dos se tocaban.
    body.removeFromTop (th::padIn);

    z.scaleCurves = top.removeFromLeft (kScaleW);
    z.curves      = top;
    z.scaleDelta  = body.removeFromLeft (kScaleW);
    z.delta       = body;

    z.freqAxis = axis.withLeft (z.curves.getX()).withRight (z.curves.getRight());
    return z;
}

void TonalBalanceLens::resized()
{
    zones = zonesFor (getWidth(), getHeight());
    Lens::resized();
}

//======================================================================================== ejes
float TonalBalanceLens::xForFreq (double hz) const
{
    const double t = std::log (juce::jlimit (kMinHz, kMaxHz, hz) / kMinHz) / std::log (kMaxHz / kMinHz);
    return (float) zones.curves.getX() + (float) t * (float) zones.curves.getWidth();
}

double TonalBalanceLens::freqAtX (int x) const
{
    if (zones.curves.getWidth() <= 1) return kMinHz;
    const double t = juce::jlimit (0.0, 1.0,
                                   (double) (x - zones.curves.getX()) / (double) zones.curves.getWidth());
    return kMinHz * std::pow (kMaxHz / kMinHz, t);
}

int TonalBalanceLens::bandAtX (int x) const
{
    const double hz = freqAtX (x);
    for (int b = 0; b < ReferenceFrame::kNumBands; ++b)
        if (hz >= kThirdOctaveHz[b] * kSixthDown && hz < kThirdOctaveHz[b] * kSixthUp) return b;
    return -1;
}

float TonalBalanceLens::yForNorm (float db) const
{
    const float t = juce::jlimit (0.0f, 1.0f, (kNormTopDb - db) / (kNormTopDb - kNormBottomDb));
    return (float) zones.curves.getY() + t * (float) zones.curves.getHeight();
}

float TonalBalanceLens::yForDelta (float db) const
{
    const float t = juce::jlimit (0.0f, 1.0f, (kDeltaSpanDb - db) / (2.0f * kDeltaSpanDb));
    return (float) zones.delta.getY() + t * (float) zones.delta.getHeight();
}

//======================================================================================== capa estática
void TonalBalanceLens::renderStatic (juce::Graphics& g, int width, int height)
{
    zones = zonesFor (width, height);

    const auto panel = [&] (juce::Rectangle<int> r)
    {
        g.setColour (th::surf.withAlpha (0.65f));
        g.fillRoundedRectangle (r.toFloat(), 3.0f);
        g.setColour (look::gridMinor);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 3.0f, 1.0f);
    };
    panel (zones.curves);
    panel (zones.delta);

    // ---- rejilla de frecuencia: los 30 centros de ⅓ de octava, las décadas más marcadas ----
    for (const double hz : kThirdOctaveHz)
    {
        if (hz < kMinHz || hz > kMaxHz) continue;
        const bool decade = std::abs (std::log10 (hz) - std::round (std::log10 (hz))) < 1.0e-6;
        g.setColour (decade ? th::line : th::lineSoft);
        const int x = juce::roundToInt (xForFreq (hz));
        look::fillSnapped (g, { (float) (x), (float) (zones.curves.getY()), 1.0f, (float) (zones.curves.getHeight()) });
        look::fillSnapped (g, { (float) (x), (float) (zones.delta.getY()), 1.0f, (float) (zones.delta.getHeight()) });
    }

    g.setFont (look::mono (9.0f));
    g.setColour (look::txtTertiary);
    for (const double hz : kLabelledHz)
        g.drawText (shortHz (hz), juce::roundToInt (xForFreq (hz)) - 20, zones.freqAxis.getY() + 1, 40, 12,
                    juce::Justification::centred, true);

    // ---- escala de las curvas: dB RELATIVOS a la loudness, cada 10 dB, escala FIJA ----
    for (float db = kNormTopDb; db >= kNormBottomDb; db -= 6.0f)
    {
        const int y = juce::roundToInt (yForNorm (db));
        g.setColour (look::gridMinor);
        look::fillSnapped (g, { (float) (zones.curves.getX()), (float) (y), (float) (zones.curves.getWidth()), 1.0f });
        g.setColour (look::txtTertiary);
        g.setFont (look::mono (9.0f));
        g.drawText (juce::String ((int) db), zones.scaleCurves.getX(), y - 6, kScaleW - 8, 12,
                    juce::Justification::centredRight, true);
    }

    g.setColour (th::mut);
    g.setFont (look::label (10.0f));
    g.drawText ("dB / LU", zones.scaleCurves.getX(), zones.curves.getY() + 8, kScaleW - 8, 12,
                juce::Justification::centredRight, true);

    // ---- la banda de referencia de ±3 dB, ROTULADA. No es un veredicto: es una regla puesta a la vista
    //      para que el ojo tenga contra qué medir el largo de las barras.
    const auto refBand = juce::Rectangle<float> ((float) zones.delta.getX(), yForDelta (kDeltaRefDb),
                                                 (float) zones.delta.getWidth(),
                                                 yForDelta (-kDeltaRefDb) - yForDelta (kDeltaRefDb));
    g.setColour (th::green.withAlpha (0.07f));
    g.fillRect (refBand);
    g.setColour (th::green.withAlpha (0.28f));
    g.drawHorizontalLine (juce::roundToInt (yForDelta (kDeltaRefDb)), refBand.getX(), refBand.getRight());
    g.drawHorizontalLine (juce::roundToInt (yForDelta (-kDeltaRefDb)), refBand.getX(), refBand.getRight());

    g.setColour (th::mut);
    g.setFont (look::label (9.0f));
    g.drawText (tr (strings::Key::refPlusMinus) + juce::String (kDeltaRefDb, 0) + " dB", zones.delta.getX() + 6,
                juce::roundToInt (yForDelta (kDeltaRefDb)) + 2, 90, 12, juce::Justification::centredLeft, true);

    // ---- escala del delta ----
    for (const float db : { kDeltaSpanDb, kDeltaSpanDb * 0.5f, 0.0f, -kDeltaSpanDb * 0.5f, -kDeltaSpanDb })
    {
        const int y = juce::roundToInt (yForDelta (db));
        g.setColour (db == 0.0f ? th::line : th::lineSoft);
        look::fillSnapped (g, { (float) (zones.delta.getX()), (float) (y), (float) (zones.delta.getWidth()), 1.0f });
        g.setColour (look::txtTertiary);
        g.setFont (look::mono (9.0f));
        g.drawText (signed1 (db), zones.scaleDelta.getX(), y - 6, kScaleW - 8, 12,
                    juce::Justification::centredRight, true);
    }

    g.setColour (th::mut);
    g.setFont (look::label (10.0f));
    g.drawText (tr (strings::Key::delta), zones.scaleDelta.getX(), zones.delta.getY() + 8, kScaleW - 8, 12,
                juce::Justification::centredRight, true);

    if (! zones.strip.isEmpty()) paintStripWave (g);   // F4 (T6)
}

//======================================================================================== datos
bool TonalBalanceLens::advanceFrame()
{
    data = processor.reference().read();

    // ---- F4 (T6): la tira aparece y desaparece con la referencia, y su onda llega de otro hilo ----
    bool stripMoved = false;
    {
        const bool strip = wantsStrip();
        waveform.load (strip ? juce::File (processor.referencePath()) : juce::File());
        if (strip != shownStrip)
        {
            shownStrip = strip;
            zones = zonesFor (getWidth(), getHeight());
            invalidateStatic();
            stripMoved = true;
        }
        if (waveform.revision() != shownWaveRev)
        {
            shownWaveRev = waveform.revision();
            invalidateStatic();
            stripMoved = true;
        }
        const auto span = processor.referenceSpan();
        if (span.whole != shownSpan.whole || span.fromS != shownSpan.fromS || span.toS != shownSpan.toS)
        {
            shownSpan = span;
            stripMoved = true;
        }
    }

    // Los bins por banda salen de la geometría de la FFT vigente: es lo que le permite a la lectura decir
    // "esta banda la mide menos de un bin" en vez de dar una diferencia sin contexto.
    //
    // 57c — POR SOLAPAMIENTO, FRACCIONARIO, con la MISMA cuenta que el motor (ver
    // analysis/FileAnalysis.h): el bin k cubre [(k−½)·binHz, (k+½)·binHz) y aporta los hercios que
    // comparte con la banda. Antes se contaban bins enteros con dos `ceil`, y por eso la banda de 40 Hz
    // daba CERO a 4 096/48 k (0.79 bins caían entre los dos redondeos) y la lente la declaraba sin
    // medición: el agujero de la foto de Joaquín.
    const auto& sp = processor.spectrum().read();
    const double binHz = sp.binHz();
    for (int b = 0; b < ReferenceFrame::kNumBands; ++b)
    {
        if (binHz <= 0.0 || sp.numBins <= 0) { bins[b] = 0.0f; continue; }
        const double lo = kThirdOctaveHz[b] * kSixthDown, hi = kThirdOctaveHz[b] * kSixthUp;
        const int k0 = juce::jmax (0, (int) std::floor (lo / binHz) - 1);
        const int k1 = juce::jmin (sp.numBins - 1, (int) std::ceil (hi / binHz) + 1);
        double covered = 0.0;
        for (int k = k0; k <= k1; ++k)
            covered += juce::jmax (0.0, juce::jmin (hi, ((double) k + 0.5) * binHz)
                                            - juce::jmax (lo, ((double) k - 0.5) * binHz));
        bins[b] = (float) (covered / binHz);
    }

    const bool reduced = prefersReducedMotion();
    bool moved = false;

    for (int b = 0; b < ReferenceFrame::kNumBands; ++b)
    {
        // ========================================================================================================
        // ===== 57c · QUÉ SE PUEDE DIBUJAR =====
        //
        // «Se corta la línea» era DOS defectos con la misma foto, y éste es el segundo. La referencia de
        // Joaquín (un pad) tiene ruido entre −90 y −63 dBFS fuera de 90–350 Hz: por el criterio del 57b
        // eso es "medible" (crudo ≥ −90 dBFS), así que la lente lo dibujaba… en su normalizada de −62 LU,
        // veinte decibeles POR DEBAJO del piso del plot. `yForNorm` lo clampeaba al borde y salía una raya
        // vertical hasta el piso, una barra de delta clavada en ±12 y un readout con |delta| de 60.
        //
        // El criterio ahora tiene las DOS mitades: la banda se dibuja si el crudo es medible (≥ −90 dBFS,
        // que es lo que dice `norm > kFloorDb` desde el 57b) Y si su normalizada entra en el plot
        // (> −42 LU). Lo que no entra CORTA la curva, que es lo honesto: no se puede dibujar un valor
        // fuera de la escala sin mentir sobre dónde está.
        //
        // `> kNormBottomDb` y no `≥`: un valor exactamente en el piso no se distingue de uno que se salió,
        // y con el `>` el contrato "ningún vértice en la fila del piso" es exacto en vez de casi (lo
        // verifica TONAL[piso]).
        //
        // Se fue, en cambio, la exigencia de `bins[b] > 0` para el lado vivo: con el solapamiento
        // fraccionario ninguna banda por debajo de Nyquist queda sin cubrir, y lo que antes era "a esta
        // resolución no hay medición" pasó a ser "esta banda la mide 0.8 de un bin", que la lectura dice.
        // ========================================================================================================
        liveHas[b] = data.liveValid && data.liveNorm[b] > SpectrumFrame::kFloorDb
                                    && data.liveNorm[b] > kNormBottomDb;
        refHas[b]  = data.refValid  && data.refNorm[b]  > SpectrumFrame::kFloorDb
                                    && data.refNorm[b]  > kNormBottomDb;
        // COMPARABLE es lo del motor Y que los dos lados se puedan dibujar: un delta contra un valor que
        // no entra en el plot es el +106 dB de la foto del 57b con otra cara.
        comparable[b] = data.bandValid[b] && liveHas[b] && refHas[b];

        const float targetLive  = liveHas[b] ? data.liveNorm[b] : kNormBottomDb;
        const float targetRef   = refHas[b]  ? data.refNorm[b]  : kNormBottomDb;
        const float targetDelta = comparable[b] ? data.deltaDb[b] : 0.0f;

        const float wasLive = dispLive[b], wasRef = dispRef[b], wasDelta = dispDelta[b];
        // El PRIMER frame entra de una: un medidor que trepa desde el piso al abrirse miente durante un
        // segundo. Lo que se anima es el CAMBIO, no la llegada (misma regla que el resto de las lentes).
        const bool snap = reduced || ! primed;
        dispLive[b]  = snap ? targetLive  : wasLive  + (targetLive  - wasLive)  * kRelease;
        dispRef[b]   = snap ? targetRef   : wasRef   + (targetRef   - wasRef)   * kRelease;
        dispDelta[b] = snap ? targetDelta : wasDelta + (targetDelta - wasDelta) * kRelease;

        juce::ignoreUnused (wasLive, wasRef, wasDelta);
        moved = moved || liveHas[b] != shownLiveHas[b] || refHas[b] != shownRefHas[b]
                      || comparable[b] != shownComparable[b]
                      || std::abs (dispLive[b]  - shownLive[b])  > kShownEpsDb
                      || std::abs (dispRef[b]   - shownRef[b])   > kShownEpsDb
                      || std::abs (dispDelta[b] - shownDelta[b]) > kShownEpsDb;
    }

    // CAMBIO = LO QUE SE APARTA DEL ÚLTIMO CUADRO MOSTRADO, no del cuadro anterior (prompt 96). Hasta la F4 de
    // la 0.2, en silencio la curva viva bajaba de verdad y sin parar —el promedio de bandas es desde el reset y
    // el silencio lo diluía, mientras la integrada con compuerta no baja: 4.3/N dB por segundo con N segundos
    // acumulados—, y con el criterio viejo (0.001 dB entre cuadros, cola de 30) eso era repintar 30 veces por
    // segundo para siempre. Desde T4 (D-113) los cuadros bajo −70 LUFS no entran al promedio (Reference.h) y
    // la curva se queda quieta; el criterio sigue igual de bueno para cualquier otro movimiento lento.
    // Contra el último cuadro mostrado el error de lo que queda en pantalla está acotado a kShownEpsDb, y la
    // lente repinta cuando la curva se movió eso, no en cada cuadro.
    if (moved)
        for (int b = 0; b < ReferenceFrame::kNumBands; ++b)
        {
            shownLive[b]  = dispLive[b];  shownRef[b] = dispRef[b];  shownDelta[b] = dispDelta[b];
            shownLiveHas[b] = liveHas[b]; shownRefHas[b] = refHas[b]; shownComparable[b] = comparable[b];
        }

    primed = true;
    // Mientras se analiza, la barra de progreso se mueve: hay que seguir repintando aunque el dato no cambie.
    return moved || stripMoved || processor.referenceBusy() || waveform.busy();
}

//======================================================================================== capa viva
void TonalBalanceLens::paintLive (juce::Graphics& g)
{
    if (zones.curves.isEmpty()) zones = zonesFor (getWidth(), getHeight());

    paintHead (g);
    if (! zones.strip.isEmpty()) paintStripLive (g);   // F4 (T6)

    paintCurve (g, dispRef,  refHas,  th::magenta);   // la referencia primero: el programa va encima
    paintCurve (g, dispLive, liveHas, th::green);
    paintDeltaBars (g);
    paintReadout (g);

    // Marco al arrastrar un archivo encima: el usuario tiene que ver DÓNDE va a soltar.
    if (dragOver)
    {
        g.setColour (th::green.withAlpha (0.10f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 4.0f);
        g.setColour (th::green.withAlpha (0.7f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 4.0f, 2.0f);
    }

    paintButton (g, zones.button[ctrlReset], tr (strings::Key::reset),  hovered == ctrlReset);
    paintButton (g, zones.button[ctrlLoad],  tr (strings::Key::load),   hovered == ctrlLoad);
    paintButton (g, zones.button[ctrlClear], tr (strings::Key::remove), hovered == ctrlClear);
}

// ========================================================================================================
// LA CABECERA: el estado en una línea, la leyenda de las dos curvas con su integrado, y la barra de
// progreso mientras se analiza. Es donde se contesta "¿qué estoy mirando?" sin tener que adivinar.
// ========================================================================================================
void TonalBalanceLens::paintHead (juce::Graphics& g) const
{
    auto head = zones.head;
    auto line1 = head.removeFromTop (16);
    auto line2 = head;

    // --- línea 1: estado ---
    const bool problem = processor.referenceMissingName().isNotEmpty()
                      || (! processor.referenceBusy() && processor.referenceError().isNotEmpty());
    g.setColour (problem ? th::amber : th::txt);
    g.setFont (look::mono (11.0f));
    g.drawText (stateText(), line1, juce::Justification::centredLeft, true);

    // Barra de progreso: sólo mientras se analiza, y pegada a la derecha de la línea de estado.
    if (processor.referenceBusy())
    {
        auto bar = line1.removeFromRight (juce::jmin (160, line1.getWidth() / 3)).reduced (0, 5);
        g.setColour (th::surf2);
        g.fillRoundedRectangle (bar.toFloat(), 2.0f);
        g.setColour (th::green.withAlpha (0.8f));
        g.fillRoundedRectangle (bar.toFloat().withWidth ((float) bar.getWidth()
                                                         * juce::jlimit (0.0f, 1.0f, processor.referenceProgress())),
                                2.0f);
    }

    // --- línea 2: la leyenda de las dos curvas ---
    g.setFont (look::label (10.0f));

    const juce::String liveLabel = tr (strings::Key::program)
        + (data.liveValid ? (kDot + juce::String (data.liveIntegrated, 1) + " LUFS")
                          : (kDot + tr (strings::Key::measuring) + " " + juce::String (data.liveSeconds, 1)
                             + " / " + juce::String (ReferenceFrame::kMinLiveSeconds, 1) + " s"));

    auto left = line2.removeFromLeft (line2.getWidth() / 2);
    g.setColour (th::green);
    g.fillRect (left.removeFromLeft (10).withSizeKeepingCentre (10, 2));
    left.removeFromLeft (4);
    g.setColour (data.liveValid ? th::txt : th::mut);
    g.drawText (liveLabel, left, juce::Justification::centredLeft, true);

    if (data.refValid)
    {
        g.setColour (th::magenta);
        g.fillRect (line2.removeFromLeft (10).withSizeKeepingCentre (10, 2));
        line2.removeFromLeft (4);
        // F4 (T6): con un tramo, los segundos son los del TRAMO (lo que se midió), no los del archivo.
        const auto span = processor.referenceSpan();
        const double refS = span.whole ? (double) data.refSeconds : span.toS - span.fromS;
        g.setColour (th::txt);
        g.drawText (juce::String (data.refName) + kDot + juce::String (data.refIntegrated, 1) + " LUFS"
                        + kDot + juce::String (refS, 1) + " s",
                    line2, juce::Justification::centredLeft, true);
    }
}

juce::String TonalBalanceLens::stateText() const
{
    if (processor.referenceBusy())
        return tr (strings::Key::analysing) + " " + processor.referenceName() + kDot
             + juce::String (juce::roundToInt (100.0f * processor.referenceProgress())) + " %";

    const auto missing = processor.referenceMissingName();
    if (missing.isNotEmpty())
        return tr (strings::Key::referenceMissing) + missing;

    const auto err = processor.referenceError();
    if (err.isNotEmpty())
        return processor.referenceName() + kDot + err;

    if (! data.refValid)
        return tr (strings::Key::noReferenceDrag);

    // 56b (LOW 3 del 54): el aviso NO reemplaza al estado, CONVIVE con él. Un aviso del tipo "la
    // referencia es mono" es una salvedad sobre lo que se está comparando, no un motivo para dejar de
    // decir QUÉ se está comparando — y tapando la frase normal, el usuario perdía la única línea que
    // explica que el delta suma cero.
    const auto warn = processor.referenceWarning();
    const auto state = tr (strings::Key::tiltEqualLoudness);
    return warn.isNotEmpty() ? (warn + kDot + state) : state;
}

// ========================================================================================================
// LAS CURVAS. Un vértice por BANDA (30), no por píxel: acá no hay 16 000 bins que resumir — lo que se
// dibuja es la rejilla de ⅓ de octava, y ponerle más vértices sería inventar resolución que el dato no
// tiene. Las bandas sin medición CORTAN la curva en vez de bajarla al piso: una curva que se desploma
// donde no hay datos dibuja un agujero que no existe.
// ========================================================================================================
void TonalBalanceLens::paintCurve (juce::Graphics& g, const float* norm, const bool* has,
                                   juce::Colour hue) const
{
    g.saveState();
    g.reduceClipRegion (zones.curves);

    juce::Path p;
    bool open = false;
    for (int b = 0; b < ReferenceFrame::kNumBands; ++b)
    {
        if (! has[b]) { open = false; continue; }
        const float x = xForFreq (kThirdOctaveHz[b]);
        const float y = yForNorm (norm[b]);
        if (! open) { p.startNewSubPath (x, y); open = true; }
        else        p.lineTo (x, y);
    }

    // 57b — glow barato en las dos curvas: son dos líneas del mismo grosor cruzándose sobre una rejilla,
    // y el glow es lo que deja seguir cada una sin tener que buscarle el color.
    const auto m = look::metricsFor (getWidth());
    look::glowPath (g, p, hue, m.dataW, m.glowRadius * 0.7f);
    g.setColour (hue.withAlpha (0.92f));
    g.strokePath (p, juce::PathStrokeType (m.dataW, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));

    // Un punto por banda medida: deja ver DÓNDE están los datos de verdad.
    g.setColour (hue.withAlpha (0.55f));
    for (int b = 0; b < ReferenceFrame::kNumBands; ++b)
        if (has[b])
            g.fillEllipse (xForFreq (kThirdOctaveHz[b]) - 1.5f, yForNorm (norm[b]) - 1.5f, 3.0f, 3.0f);

    g.restoreState();
}

// Las barras van entre los bordes REALES de cada banda (fc·2^∓1/6): sobre un eje logarítmico el ancho de
// la barra ES el ancho de la banda. Es la misma regla que BAND CORRELATION y que las barras de SPECTRUM.
void TonalBalanceLens::paintDeltaBars (juce::Graphics& g) const
{
    g.saveState();
    g.reduceClipRegion (zones.delta);

    const float base = yForDelta (0.0f);

    for (int b = 0; b < ReferenceFrame::kNumBands; ++b)
    {
        const double lo = kThirdOctaveHz[b] * kSixthDown, hi = kThirdOctaveHz[b] * kSixthUp;
        if (hi <= kMinHz || lo >= kMaxHz) continue;

        const float xa = xForFreq (lo), xb = xForFreq (hi);
        const float x  = xa + 1.0f;
        const float w  = juce::jmax (2.0f, xb - xa - 2.0f);

        // BANDA NO COMPARABLE (uno de los dos lados no la midió): se marca, no se dibuja en cero. Cero
        // sería decir "coincide perfecto", que es la mentira más fácil de dibujar de este gráfico.
        if (! comparable[b])
        {
            g.setColour (look::tick.withAlpha (0.45f));
            g.fillRect (juce::Rectangle<float> (x, base - 1.0f, w, 2.0f));
            continue;
        }

        const float v = dispDelta[b];
        const float y = yForDelta (v);
        const float top = juce::jmin (y, base), bot = juce::jmax (y, base);
        const auto hue = std::abs (v) > kDeltaRefDb ? th::amber : th::green;

        // 57b — degradado desde la punta hacia el cero, igual que BAND CORRELATION: las dos lentes
        // dibujan barras bipolares sobre una línea de cero y tienen que verse de la misma familia.
        g.setGradientFill (juce::ColourGradient (hue.withAlpha (0.46f), 0.0f, y,
                                                 hue.withAlpha (0.06f), 0.0f, base, false));
        g.fillRect (juce::Rectangle<float> (x, top, w, juce::jmax (1.0f, bot - top)));
        g.setColour (hue.withAlpha (0.92f));
        g.fillRect (juce::Rectangle<float> (x, y - 0.75f, w, 1.5f));
    }

    g.restoreState();
}

//======================================================================================== lectura
TonalBalanceLens::Readout TonalBalanceLens::readoutAt (juce::Point<int> p) const
{
    if (! zones.curves.contains (p) && ! zones.delta.contains (p)) return {};
    return readoutForBand (bandAtX (p.x));
}

TonalBalanceLens::Readout TonalBalanceLens::readoutForBand (int b) const
{
    Readout r;
    if (b < 0 || b >= ReferenceFrame::kNumBands) return r;

    r.valid      = true;
    r.band       = b;
    r.centreHz   = kThirdOctaveHz[b];
    r.bins       = bins[b];
    r.comparable = comparable[b];
    r.liveDrawable = liveHas[b];
    r.refDrawable  = refHas[b];
    r.liveNorm   = data.liveNorm[b];
    r.refNorm    = data.refNorm[b];
    // EL DELTA SIGUE A LA COMPARABILIDAD DE LA LENTE. El motor lo calcula en cuanto los dos lados son
    // medibles; la lente además exige que los dos ENTREN EN EL PLOT (57c), y un delta contra un valor que
    // no se puede dibujar es el +106 dB del 57b con otra cara. Donde no se compara, no hay delta — cero,
    // y el readout dice por qué.
    r.deltaDb    = comparable[b] ? data.deltaDb[b] : 0.0f;
    return r;
}

void TonalBalanceLens::paintReadout (juce::Graphics& g) const
{
    if (cursor.x < 0) return;
    const auto r = readoutAt (cursor);
    if (! r.valid) return;

    const float xa = xForFreq (r.centreHz * kSixthDown), xb = xForFreq (r.centreHz * kSixthUp);
    g.setColour (th::txt.withAlpha (0.10f));
    g.fillRect (juce::Rectangle<float> (xa, (float) zones.curves.getY(), xb - xa,
                                        (float) zones.curves.getHeight()));
    g.fillRect (juce::Rectangle<float> (xa, (float) zones.delta.getY(), xb - xa,
                                        (float) zones.delta.getHeight()));

    // 57c — la cuenta de bins es FRACCIONARIA y se dice así: por debajo de 1 la banda no es una medición
    // independiente, es la densidad del bin donde cae, y el número lo tiene que mostrar.
    const juce::String binsText = (r.bins < 10.0f ? juce::String (r.bins, 1)
                                                  : juce::String (juce::roundToInt (r.bins)))
                                + tr (strings::Key::binsAtThisFft);

    juce::String text = shortHz (r.centreHz) + " Hz" + kDot;
    if (r.bins <= 0.0f)
        text += tr (strings::Key::noBinsHere);
    else if (data.refValid && ! r.refDrawable && data.refNorm[r.band] > SpectrumFrame::kFloorDb)
        // La referencia SE MIDIÓ, pero cae fuera del plot. Decir "sin referencia en esta banda" sería
        // esconder que hay un número y que está veinte decibeles abajo.
        text += tr (strings::Key::refBelowRange) + kDot + binsText;
    else if (! r.comparable)
        text += tr (strings::Key::noReferenceInBand) + kDot + binsText;
    else
        text += trLower (strings::Key::program) + " " + signed1 (r.liveNorm) + kDot
              + trLower (strings::Key::referenceLabel) + " " + signed1 (r.refNorm) + kDot
              + trLower (strings::Key::delta) + " " + signed1 (r.deltaDb) + " dB" + kDot
              + binsText;

    g.setFont (look::mono (11.0f));
    const int tw = (int) std::ceil (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text)) + 16;
    const auto box = readoutBoxFor (zones.curves, cursor.x, tw);

    g.setColour (th::bg1.withAlpha (0.9f));
    g.fillRoundedRectangle (box.toFloat(), 3.0f);
    g.setColour (th::green.withAlpha (0.4f));
    g.drawRoundedRectangle (box.toFloat().reduced (0.5f), 3.0f, 1.0f);
    g.setColour (th::txt);
    g.drawText (text, box, juce::Justification::centred, true);
}

void TonalBalanceLens::paintButton (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& label,
                                    bool hovered_) const
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

    g.setColour (hue);
    g.setFont (look::label (10.0f));
    g.drawText (label, area, juce::Justification::centred, true);
}

//======================================================================================== interacción
// El chooser va ASÍNCRONO: nada modal adentro de un plugin (un diálogo modal dentro del message thread de
// un DAW es la manera más rápida de colgarle la sesión a alguien). Y por eso mismo los tests cargan por
// API, no fabricando diálogos.
void TonalBalanceLens::pressControl (int control)
{
    if (control == ctrlReset)
    {
        processor.resetAnalysis();
        primed = false;   // el promedio arranca de cero: la curva no tiene que "bajar" hasta ahí animando
    }
    else if (control == ctrlLoad)
    {
        chooser = std::make_unique<juce::FileChooser> (
            tr (strings::Key::chooseReference), juce::File(), kAudioFilter);

        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe = juce::Component::SafePointer<TonalBalanceLens> (this)]
                              (const juce::FileChooser& fc)
                              {
                                  if (safe == nullptr) return;
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile()) safe->processor.loadReference (f);
                              });
    }
    else if (control == ctrlClear)
    {
        processor.clearReference();
    }
    else return;

    repaint();
}

void TonalBalanceLens::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < kNumControls; ++i)
        if (zones.button[i].contains (e.getPosition())) { pressControl (i); return; }
    if (zones.strip.contains (e.getPosition())) stripBegin (e.getPosition().x);   // F4 (T6)
}

void TonalBalanceLens::mouseDrag (const juce::MouseEvent& e)        { if (dragging) stripDrag (e.getPosition().x); }
void TonalBalanceLens::mouseUp (const juce::MouseEvent& e)          { if (dragging) stripEnd (e.getPosition().x); }
void TonalBalanceLens::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (zones.strip.contains (e.getPosition())) stripDoubleClick();
}

// ========================================================================================================
// F4 (T6) · LA TIRA. La lente no mide: pide. Arrastrar deja el tramo en el estado del processor, y el processor
// lo mide en el hilo del analizador de archivo con RangeAnalyzer::measure (la función de telescope-measure).
// ========================================================================================================
bool TonalBalanceLens::wantsStrip() const
{
    return processor.referencePath().isNotEmpty() && processor.referenceMissingName().isEmpty();
}

bool TonalBalanceLens::waveformReady() const
{
    const auto o = waveform.latest();
    return o != nullptr && o->ok() && o->path == processor.referencePath();
}

double TonalBalanceLens::stripSeconds() const
{
    const auto o = waveform.latest();
    if (o != nullptr && o->ok() && o->path == processor.referencePath()) return o->seconds;
    return data.refValid ? (double) data.refSeconds : 0.0;
}

double TonalBalanceLens::secondsAtX (int x) const
{
    const auto r = zones.strip;
    if (r.getWidth() <= 1) return 0.0;
    const double t = juce::jlimit (0.0, 1.0, (double) (x - r.getX()) / (double) r.getWidth());
    return t * stripSeconds();
}

int TonalBalanceLens::xAtSeconds (double sec) const
{
    const auto r = zones.strip;
    const double total = stripSeconds();
    if (total <= 0.0) return r.getX();
    return r.getX() + juce::roundToInt (juce::jlimit (0.0, 1.0, sec / total) * (double) r.getWidth());
}

void TonalBalanceLens::stripBegin (int x)
{
    if (stripSeconds() <= 0.0) return;
    dragging  = true;
    dragFromS = dragToS = secondsAtX (x);
    repaint();
}

void TonalBalanceLens::stripDrag (int x)
{
    if (! dragging) return;
    dragToS = secondsAtX (x);
    repaint();
}

void TonalBalanceLens::stripEnd (int x)
{
    if (! dragging) return;
    dragging = false;
    dragToS  = secondsAtX (x);
    if (std::abs (xAtSeconds (dragToS) - xAtSeconds (dragFromS)) >= kMinDragPx)
        processor.setReferenceRange (juce::jmin (dragFromS, dragToS), juce::jmax (dragFromS, dragToS));
    repaint();
}

void TonalBalanceLens::stripDoubleClick()
{
    dragging = false;
    processor.clearReferenceRange();
    repaint();
}

juce::String TonalBalanceLens::stripLabel() const
{
    const double total = stripSeconds();
    double from = 0.0, to = total;
    if (dragging)                                    { from = juce::jmin (dragFromS, dragToS); to = juce::jmax (dragFromS, dragToS); }
    else if (processor.referenceRange (from, to))    { from = juce::jlimit (0.0, total, from); to = juce::jlimit (0.0, total, to); }
    else                                             { from = 0.0; to = total; }
    return "ref" + juce::String::fromUTF8 (" \xc2\xb7 ") + mmss (from) + juce::String::fromUTF8 ("\xe2\x80\x93") + mmss (to);
}

juce::String TonalBalanceLens::stripHint() const
{
    // La ayuda entra entera o no se dibuja: un texto cortado con «…» en una tira de 30 px no ayuda a nadie.
    const auto hint  = tr (strings::Key::refSpanHint);
    const auto font  = look::label (11.0f);
    const int  labW  = (int) std::ceil (juce::GlyphArrangement::getStringWidth (font, stripLabel())) + 12;
    const int  hintW = (int) std::ceil (juce::GlyphArrangement::getStringWidth (font, hint)) + 12;
    return (labW + hintW + 16 <= zones.strip.getWidth()) ? hint : juce::String();
}

// La pantalla y la onda: capa estática (se rehornea cuando llega otra onda o cambia el tamaño).
void TonalBalanceLens::paintStripWave (juce::Graphics& g) const
{
    const auto r = zones.strip;
    look::drawScreenEdge (g, r);             // D-109: en claro, el filo oscuro de la pantalla
    const look::ScreenInk screen;            // y adentro, la tinta del oscuro en los dos temas
    g.setColour (th::bg1);
    g.fillRect (r);
    g.setColour (look::gridMinor);
    g.drawRect (r, 1);

    const auto o = waveform.latest();
    if (o == nullptr || ! o->ok() || o->path != processor.referencePath()) return;

    const auto area = r.reduced (2, 3).toFloat();
    const float mid = area.getCentreY(), half = 0.5f * area.getHeight();
    g.setColour (look::gridMinor);
    g.fillRect (juce::Rectangle<float> (area.getX(), mid - 0.25f, area.getWidth(), 0.5f));

    // Normalizada a su PICO: es un dibujo de la forma, no un medidor (el nivel lo dice la cabecera en LUFS), y
    // una referencia bajita se vería como una raya. Con un piso, para que el silencio no se infle hasta el borde.
    float peak = 0.0f;
    for (int b = 0; b < ReferenceWaveform::kBuckets; ++b)
        peak = juce::jmax (peak, std::abs (o->lo[(size_t) b]), std::abs (o->hi[(size_t) b]));
    const float gain = 1.0f / juce::jmax (0.05f, peak);

    // Media columna lógica por paso: a 2× es un píxel físico por columna.
    const int cols = juce::jmax (1, (int) std::ceil (area.getWidth() * 2.0f));
    const float w  = area.getWidth() / (float) cols;
    g.setColour (th::magenta.withAlpha (0.80f));
    for (int c = 0; c < cols; ++c)
    {
        const int b0 = (int) ((long long) c * ReferenceWaveform::kBuckets / cols);
        const int b1 = juce::jmax (b0 + 1, (int) ((long long) (c + 1) * ReferenceWaveform::kBuckets / cols));
        float lo = 1.0f, hi = -1.0f;
        for (int b = b0; b < b1 && b < ReferenceWaveform::kBuckets; ++b)
        {
            lo = juce::jmin (lo, o->lo[(size_t) b]);
            hi = juce::jmax (hi, o->hi[(size_t) b]);
        }
        if (hi < lo) continue;
        lo = juce::jlimit (-1.0f, 1.0f, lo * gain);
        hi = juce::jlimit (-1.0f, 1.0f, hi * gain);
        const float y0 = mid - hi * half, y1 = mid - lo * half;
        g.fillRect (juce::Rectangle<float> (area.getX() + (float) c * w, y0, w, juce::jmax (0.5f, y1 - y0)));
    }
}

// El tramo, sus bordes y el rótulo: capa viva (cambian al arrastrar).
void TonalBalanceLens::paintStripLive (juce::Graphics& g) const
{
    const auto r = zones.strip;
    const look::ScreenInk screen;   // D-109: adentro de la pantalla, la tinta del oscuro

    const double total = stripSeconds();
    double from = 0.0, to = total;
    bool whole = true;
    if (dragging)                                    { from = juce::jmin (dragFromS, dragToS); to = juce::jmax (dragFromS, dragToS); whole = false; }
    else if (processor.referenceRange (from, to))    { whole = false; }

    if (! whole && total > 0.0)
    {
        const int x0 = xAtSeconds (from), x1 = xAtSeconds (to);
        g.setColour (th::bg0.withAlpha (0.62f));   // lo que queda afuera del tramo, apagado
        g.fillRect (juce::Rectangle<int> (r.getX(), r.getY(), x0 - r.getX(), r.getHeight()));
        g.fillRect (juce::Rectangle<int> (x1, r.getY(), r.getRight() - x1, r.getHeight()));
        g.setColour (look::accent);
        look::fillSnapped (g, { (float) x0, (float) r.getY(), 1.0f, (float) r.getHeight() });
        look::fillSnapped (g, { (float) (x1 - 1), (float) r.getY(), 1.0f, (float) r.getHeight() });
    }

    // El rótulo, arriba a la izquierda, sobre su propia placa: se lee igual sobre la onda que sobre el vacío.
    const auto font  = look::label (11.0f);
    const auto label = stripLabel();
    const int  labW  = (int) std::ceil (juce::GlyphArrangement::getStringWidth (font, label)) + 12;
    const auto plate = juce::Rectangle<int> (r.getX() + 3, r.getY() + 3, labW, 15);
    g.setColour (th::bg0.withAlpha (0.88f));
    g.fillRoundedRectangle (plate.toFloat(), 2.0f);
    g.setFont (font);
    g.setColour (th::txt);
    g.drawText (label, plate, juce::Justification::centred, false);

    const auto hint = stripHint();
    if (hint.isNotEmpty())
    {
        const int hintW = (int) std::ceil (juce::GlyphArrangement::getStringWidth (font, hint)) + 12;
        const auto hp = juce::Rectangle<int> (r.getRight() - 3 - hintW, r.getY() + 3, hintW, 15);
        g.setColour (th::bg0.withAlpha (0.88f));
        g.fillRoundedRectangle (hp.toFloat(), 2.0f);
        g.setColour (th::mut);
        g.drawText (hint, hp, juce::Justification::centred, false);
    }
}

void TonalBalanceLens::mouseMove (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    const int was = hovered;
    const auto wasCursor = cursor;

    hovered = -1;
    for (int i = 0; i < kNumControls; ++i)
        if (zones.button[i].contains (p)) { hovered = i; break; }

    cursor = (zones.curves.contains (p) || zones.delta.contains (p)) ? p : juce::Point<int> (-1, -1);
    if (hovered != was || cursor != wasCursor) repaint();
}

void TonalBalanceLens::mouseExit (const juce::MouseEvent&)
{
    if (hovered != -1 || cursor.x != -1) { hovered = -1; cursor = { -1, -1 }; repaint(); }
}

// CUALQUIER archivo, no sólo las extensiones conocidas (ver el encabezado del header): si no se puede
// leer, el mensaje lo dice. Rechazar en silencio un AIFF llamado .dat sería peor.
bool TonalBalanceLens::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1;
}

void TonalBalanceLens::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    if (files.isEmpty()) { repaint(); return; }

    const juce::File f (files[0]);
    if (f.existsAsFile()) processor.loadReference (f);
    repaint();
}

void TonalBalanceLens::fileDragEnter (const juce::StringArray&, int, int) { dragOver = true;  repaint(); }
void TonalBalanceLens::fileDragExit  (const juce::StringArray&)          { dragOver = false; repaint(); }

//======================================================================================== accessibility
// The same numbers as the tooltip under the cursor: programme and reference level relative to their
// integrated loudness, and the difference between them. Where no comparison is possible, the row says why.
namespace
{
// Periscope: the comparison group sits right after the summary, where the user looks first.
enum TbGroup { tbSummary = 0, tbCompare, tbBands, tbActions };
enum TbCompareRow { cmpLoudness = 0, cmpRange, cmpPlr, cmpTruePeak, kNumCompareRows };
// Periscope: rowShape and rowTilt, the first glance at the difference curve, before the single largest bands.
enum TbSummaryRow { rowState = 0, rowReference, rowSection, rowShape, rowTilt, kSummaryFixedRows };
enum TbActionRow { actReset = 0, actLoad, actClear, actSectionStart, actSectionEnd, actWholeFile, kNumActions };
}

juce::String TonalBalanceLens::a11yGroupName (int group) const
{
    switch (group)
    {
        case tbSummary: return a11ytext::T ("Summary");
        case tbCompare: return a11ytext::T ("Comparison with the reference");
        case tbBands:   return a11ytext::T ("Third-octave bands");
        default:        return a11ytext::T ("Actions");
    }
}

int TonalBalanceLens::a11yRowCount (int group) const
{
    switch (group)
    {
        case tbSummary: return kSummaryFixedRows + kA11yDiffs;
        case tbCompare: return kNumCompareRows;
        case tbBands:   return ReferenceFrame::kNumBands;
        default:        return kNumActions;
    }
}

juce::String TonalBalanceLens::a11yRowName (int group, int row) const
{
    if (group == tbBands) return spoken::hz (kThirdOctaveHz[juce::jlimit (0, ReferenceFrame::kNumBands - 1, row)]);
    if (group == tbCompare)
    {
        switch (row)
        {
            case cmpLoudness: return a11ytext::T ("Loudness");
            case cmpRange:    return a11ytext::T ("Loudness range");
            case cmpPlr:      return a11ytext::T ("PLR");
            default:          return a11ytext::T ("True peak max");
        }
    }
    if (group == tbActions)
    {
        switch (row)
        {
            case actReset: return a11ytext::T ("Reset averaging");
            case actLoad:  return a11ytext::T ("Load reference file");
            case actClear: return a11ytext::T ("Remove reference");
            case actSectionStart: return a11ytext::T ("Section start");
            case actSectionEnd:   return a11ytext::T ("Section end");
            default:       return a11ytext::T ("Use the whole reference file");
        }
    }
    switch (row)
    {
        case rowState:     return a11ytext::T ("State");
        case rowReference: return a11ytext::T ("Reference");
        case rowSection:   return a11ytext::T ("Reference section");
        case rowShape:     return a11ytext::T ("Shape against the reference");
        case rowTilt:      return a11ytext::T ("Tilt against the reference");
        default:           return a11ytext::T ("Largest difference ") + juce::String (row - kSummaryFixedRows + 1);
    }
}

juce::String TonalBalanceLens::a11yRowValue (int group, int row) const
{
    if (group == tbActions)
    {
        if (row != actSectionStart && row != actSectionEnd) return {};
        if (! wantsStrip() || stripSeconds() <= 0.0) return a11ytext::T ("no reference loaded");
        double from = 0.0, to = 0.0;
        a11ySection (from, to);
        return spoken::duration (row == actSectionStart ? from : to);
    }
    if (group == tbBands) return a11yBandValue (juce::jlimit (0, ReferenceFrame::kNumBands - 1, row));
    if (group == tbCompare) return a11yCompareValue (row);

    switch (row)
    {
        case rowState: return spoken::clean (stateText());
        case rowReference:
        {
            const auto name = processor.referenceName();
            return name.isEmpty() ? juce::String (a11ytext::T ("none")) : name;
        }
        case rowSection:
        {
            if (processor.referencePath().isEmpty()) return a11ytext::T ("none");
            double from = 0.0, to = 0.0;
            if (! processor.referenceRange (from, to)) return a11ytext::T ("whole file");
            return mmss (from) + a11ytext::T (" to ") + mmss (to);
        }
        case rowShape: return data.refValid ? a11yShapeVsReference() : juce::String (a11ytext::T ("no reference loaded"));
        case rowTilt:  return data.refValid ? a11yTiltVsReference()  : juce::String (a11ytext::T ("no reference loaded"));
        default: break;
    }

    if (! data.refValid) return a11ytext::T ("no reference loaded");
    const auto diffs = a11yLargestDiffs();
    const int i = row - kSummaryFixedRows;
    if (i >= (int) diffs.size()) return a11ytext::T ("none");
    const int b = diffs[(size_t) i];
    const float d = data.deltaDb[b];
    return spoken::hz (kThirdOctaveHz[b]) + ", " + spoken::trimmed (std::abs (d), 1) + a11ytext::T (" dB ")
         + (d >= 0.0f ? a11ytext::T ("louder") : a11ytext::T ("quieter")) + a11ytext::T (" than the reference");
}

// Periscope: the difference curve in words, as Spectrum's "shape against the line" with the reference in place of
// the line. Stretches of comparable bands where your mix is 3 dB or more above or below the reference (the band drawn
// on screen), joined across a single band that falls short, in frequency order, each with its mean difference.
// The curve is loudness-normalised like the whole lens, so overall level does not show here.
juce::String TonalBalanceLens::a11yShapeVsReference (bool withQ) const
{
    if (! data.liveValid) return a11ytext::T ("play your mix to measure it");
    constexpr int n = ReferenceFrame::kNumBands;
    int side[n] {};
    int compared = 0, firstCmp = -1, lastCmp = -1;
    for (int b = 0; b < n; ++b)
    {
        if (! comparable[b]) continue;
        ++compared;
        if (firstCmp < 0) firstCmp = b;
        lastCmp = b;
        const float d = data.deltaDb[b];
        // kDeltaRefDb: the same 3 dB band that is drawn and labelled on screen.
        side[b] = d >= kDeltaRefDb ? 1 : (d <= -kDeltaRefDb ? -1 : 0);
    }
    // A few bands (a bass stem alone) cannot tell the shape of a mix: "all within 3 dB" would claim too much.
    if (compared < 6) return a11ytext::T ("not enough signal");
    // A single comparable band that falls short between two bands on the same side does not split the stretch.
    for (int b = 1; b < n - 1; ++b)
        if (side[b] == 0 && comparable[b] && side[b - 1] != 0 && side[b - 1] == side[b + 1]
            && data.deltaDb[b] * (float) side[b - 1] > 0.0f)
            side[b] = side[b - 1];

    struct Stretch { int first, last; double lo, hi, mean; };
    std::vector<Stretch> found;
    for (int b = 0; b < n;)
    {
        if (side[b] == 0) { ++b; continue; }
        int e = b;
        double sum = 0.0;
        while (e < n && side[e] == side[b]) sum += data.deltaDb[e++];
        found.push_back ({ b, e - 1, kThirdOctaveHz[b] * kSixthDown, kThirdOctaveHz[e - 1] * kSixthUp, sum / (e - b) });
        b = e;
    }
    if (found.empty()) return a11ytext::TF ("all within {0} dB of the reference", { spoken::trimmed (kDeltaRefDb, 1) });
    // At most six, as Spectrum's shape row: the biggest (width times difference) first, then back in frequency order.
    if (found.size() > 6)
    {
        std::sort (found.begin(), found.end(), [] (const Stretch& x, const Stretch& y)
                   { return (x.last - x.first + 1) * std::abs (x.mean) > (y.last - y.first + 1) * std::abs (y.mean); });
        found.resize (6);
        std::sort (found.begin(), found.end(), [] (const Stretch& x, const Stretch& y) { return x.first < y.first; });
    }

    juce::StringArray parts;
    for (const auto& st : found)
    {
        // The words name the grid's ends ("up to", "above"), as the stretch really ends there.
        const auto where = st.first == 0 ? a11ytext::TF ("up to {0}", { spoken::hz (spoken::roundHz (st.hi)) })
                         : (st.last == n - 1 ? a11ytext::TF ("above {0}", { spoken::hz (spoken::roundHz (st.lo)) })
                                             : spoken::hzRange (st.lo, st.hi));
        if (! withQ) { parts.add (where + " " + spoken::signedDb ((float) st.mean)); continue; }

        // The Q key. A stretch that reaches the end of what both sides measured is a job for a shelf (a reference
        // cut at 16 kHz ends there, not at 20 kHz); a closed stretch gets the bell's centre and its RBJ Q, as on
        // Spectrum's peaks. Its edges are where the difference crosses 3 dB, not half its height: a starting point.
        // A single band is as narrow as this grid can tell, so its Q is a lower bound.
        if (st.first <= firstCmp || st.last >= lastCmp) { parts.add (where + ": " + a11ytext::T ("shelf")); continue; }
        const double centre = std::sqrt (st.lo * st.hi), q = centre / (st.hi - st.lo);
        const auto   qs = spoken::trimmed (q, q < 1.0 ? 2 : (q < 10.0 ? 1 : 0));
        parts.add (where + ": " + a11ytext::TF (st.first == st.last ? "around {0}, Q {1} or more" : "around {0}, Q {1}",
                                                 { spoken::hz (spoken::roundHz (centre)), qs }));
    }
    return parts.joinIntoString (withQ ? "; " : ", ");
}

// Periscope: Q on the shape row speaks the EQ setting for each stretch; other rows keep Q as a letter search.
juce::String TonalBalanceLens::a11yRowQ (int group, int row) const
{
    if (group != tbSummary || row != rowShape) return {};
    return data.refValid ? a11yShapeVsReference (true) : juce::String (a11ytext::T ("no reference loaded"));
}

// Periscope: how the difference curve leans, a straight-line fit of the comparable bands from 50 Hz to 10 kHz against
// octaves, with Spectrum's guards: at least 8 bands spanning 3 octaves or more (a hi-hat alone has only a local
// tilt), and a band more than 3 dB from the median of its neighbours counts as that median, so one narrow
// difference does not lean the whole line. Positive: your mix is brighter than the reference.
juce::String TonalBalanceLens::a11yTiltVsReference() const
{
    if (! data.liveValid) return a11ytext::T ("play your mix to measure it");
    std::vector<double> xs, ys;
    for (int b = 0; b < ReferenceFrame::kNumBands; ++b)
    {
        if (! comparable[b] || kThirdOctaveHz[b] < 50.0 || kThirdOctaveHz[b] > 10000.0) continue;
        xs.push_back (std::log2 (kThirdOctaveHz[b] / 1000.0));
        ys.push_back (data.deltaDb[b]);
    }
    const int count = (int) xs.size();
    if (count < 8 || xs.back() - xs.front() < 3.0) return a11ytext::T ("not enough signal");

    std::vector<double> robust (ys);
    for (int i = 0; i < count; ++i)
    {
        std::vector<double> around;
        for (int j = juce::jmax (0, i - 2); j <= juce::jmin (count - 1, i + 2); ++j)
            if (j != i) around.push_back (ys[(size_t) j]);
        std::sort (around.begin(), around.end());
        const double median = around[around.size() / 2];
        if (std::abs (ys[(size_t) i] - median) > kDeltaRefDb) robust[(size_t) i] = median;
    }

    double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0;
    for (int i = 0; i < count; ++i)
    {
        const double x = xs[(size_t) i], y = robust[(size_t) i];
        sx += x; sy += y; sxx += x * x; sxy += x * y;
    }
    const double den = count * sxx - sx * sx;
    if (den <= 0.0) return a11ytext::T ("not enough signal");
    const float slope = (float) ((count * sxy - sx * sy) / den);
    if (std::abs (slope) < 0.25f) return a11ytext::T ("same tilt as the reference");
    return a11ytext::TF (slope > 0.0f ? "brighter than the reference by {0} dB per octave"
                                      : "darker than the reference by {0} dB per octave",
                         { spoken::trimmed (std::abs (slope), 1) });
}

// Periscope: help for the comparison rows (F1).
juce::String TonalBalanceLens::a11yRowHelp (int group, int row) const
{
    if (group == tbSummary && row == rowShape)
        return a11ytext::T ("Shape against the reference: the broad stretches where your mix is 3 dB or more above or below the reference, loudness already matched. Press Q for the EQ setting of each stretch: the centre and Q of a bell of the same width, or a shelf where a stretch reaches the end of what both sides measured.");
    if (group == tbSummary && row == rowTilt)
        return a11ytext::T ("Tilt against the reference: a straight line through the difference from 50 Hz to 10 kHz, in dB per octave. Under 0.25 dB per octave it counts as the same tilt as the reference. It needs at least 8 bands over 3 octaves.");
    if (group != tbCompare) return {};
    // One literal: the key collector (_scripts/collect_keys.py) takes each literal of a T() call on its own.
    return a11ytext::T ("Your mix against the reference, in the units of the Loudness and Dynamics lenses. Your mix counts from the last reset (Reset averaging here, or Reset in Loudness), the reference is the whole file or its section. The Loudness row says how much to turn the reference down or up so that both play equally loud.");
}

const TonalBalanceLens::RefLoudness& TonalBalanceLens::a11yRefLoudness() const
{
    const auto rev = processor.referenceRevision();
    if (! refLoudnessRead || rev != refLoudnessRevision)
    {
        const auto a = processor.referenceAnalysis();
        refLoudness = {};
        refLoudness.valid      = a.valid && a.integratedLufs > kSilenceDb + 1.0f;
        refLoudness.integrated = a.integratedLufs;
        refLoudness.lra        = a.lra;
        refLoudness.truePeak   = a.truePeakDbtp;
        refLoudnessRevision = rev;
        refLoudnessRead = true;
    }
    return refLoudness;
}

// Periscope: one comparison row. The reference comes from its file analysis (same meter as live, so the numbers
// compare directly); the mix from the live loudness meter, which always runs.
juce::String TonalBalanceLens::a11yCompareValue (int row) const
{
    if (! data.refValid) return a11ytext::T ("no reference loaded");
    const auto& ref = a11yRefLoudness();
    if (! ref.valid) return a11ytext::T ("no reference loaded");

    // Copies: read() is only valid until the next read. PLR lives on the frame itself, next to the loudness block.
    const auto& frame = processor.analysis().read();
    const auto  live = frame.loudness;
    const bool  plrValid = frame.plrValid;
    const float plr = frame.plr;
    const auto both = [] (const juce::String& mine, const juce::String& theirs)
    {
        return a11ytext::TF ("your mix {0}, reference {1}", { mine, theirs });
    };
    const auto lufs = [] (float v) { return spoken::trimmed (v, 1) + a11ytext::T (" LUFS"); };

    switch (row)
    {
        case cmpLoudness:
        {
            if (! live.integratedValid) return a11ytext::T ("play your mix to measure it") + ", " + a11ytext::TF ("reference {0}", { lufs (ref.integrated) });
            // How far the reference's fader has to move so that both play equally loud.
            const float d = ref.integrated - live.integrated;
            const auto amount = spoken::dB (std::abs (d), 1);
            juce::String advice;
            if (std::abs (d) < 0.1f) advice = a11ytext::T ("same loudness");
            else if (d > 0.0f)       advice = a11ytext::TF ("reference louder by {0}, turn it down by {0}", { amount });
            else                     advice = a11ytext::TF ("reference quieter by {0}, turn it up by {0}", { amount });
            return advice + "; " + both (lufs (live.integrated), lufs (ref.integrated));
        }
        case cmpRange:
            if (! live.integratedValid) return a11ytext::T ("play your mix to measure it");
            return both (spoken::trimmed (live.lra, 1) + a11ytext::T (" LU"), spoken::trimmed (ref.lra, 1) + a11ytext::T (" LU"));
        case cmpPlr:
        {
            if (! plrValid) return a11ytext::T ("play your mix to measure it");
            return both (spoken::dB (plr, 1), spoken::dB (ref.truePeak - ref.integrated, 1));
        }
        default:
        {
            if (live.truePeakMax <= kSilenceDb + 1.0f) return a11ytext::T ("play your mix to measure it");
            const auto dbtp = [] (float v) { return spoken::trimmed (v, 1) + a11ytext::T (" dBTP"); };
            return both (dbtp (live.truePeakMax), dbtp (ref.truePeak));
        }
    }
}

juce::String TonalBalanceLens::a11yBandValue (int b) const
{
    const auto r = readoutForBand (b);
    if (! r.valid) return {};
    if (r.bins <= 0.0f) return a11ytext::T ("no FFT bins in this band");

    juce::String t = r.liveDrawable ? a11ytext::T ("programme ") + spoken::signedDb (r.liveNorm)
                                    : juce::String (a11ytext::T ("programme below range"));
    if (! data.refValid) return t;

    if (r.comparable)
        t << a11ytext::T (", reference ") << spoken::signedDb (r.refNorm) << a11ytext::T (", difference ") << spoken::signedDb (r.deltaDb);
    else if (! r.refDrawable && data.refNorm[b] > SpectrumFrame::kFloorDb)
        t << a11ytext::T (", reference below range");
    else
        t << a11ytext::T (", no reference in this band");
    return t;
}

std::vector<int> TonalBalanceLens::a11yLargestDiffs() const
{
    std::vector<int> bands;
    for (int b = 0; b < ReferenceFrame::kNumBands; ++b)
        if (comparable[b]) bands.push_back (b);
    std::sort (bands.begin(), bands.end(), [this] (int x, int y)
               { return std::abs (data.deltaDb[x]) > std::abs (data.deltaDb[y]); });
    if ((int) bands.size() > kA11yDiffs) bands.resize ((size_t) kA11yDiffs);
    return bands;
}

bool TonalBalanceLens::a11yIsActivatable (int group, int row) const
{
    return group == tbActions && row != actSectionStart && row != actSectionEnd;
}

bool TonalBalanceLens::a11yIsAdjustable (int group, int row) const
{
    return group == tbActions && (row == actSectionStart || row == actSectionEnd) && wantsStrip() && stripSeconds() > 0.0;
}

void TonalBalanceLens::a11ySection (double& from, double& to) const
{
    const double total = stripSeconds();
    if (pendingFrom >= 0.0 && pendingTo >= 0.0) { from = pendingFrom; to = pendingTo; return; }
    if (processor.referenceRange (from, to)) { from = juce::jlimit (0.0, total, from); to = juce::jlimit (0.0, total, to); return; }
    from = 0.0;
    to = total;
}

// Step is one second, coarse step 10 seconds, Alt+Home/End to the edge of the file. The section is never shorter than the processor minimum.
bool TonalBalanceLens::a11yAdjust (int group, int row, int step)
{
    if (! a11yIsAdjustable (group, row)) return false;
    const double total = stripSeconds();
    const double minSpan = TelescopeProcessor::kMinReferenceSpanS;
    const double size = std::abs (step) >= a11yui::kExtreme ? total : (std::abs (step) >= a11yui::kCoarse ? 10.0 : 1.0);
    const double delta = step > 0 ? size : -size;

    double from = 0.0, to = 0.0;
    a11ySection (from, to);
    const double oldFrom = from, oldTo = to;
    if (row == actSectionStart) from = juce::jlimit (0.0, juce::jmax (0.0, to - minSpan), from + delta);
    else                        to   = juce::jlimit (juce::jmin (total, from + minSpan), total, to + delta);
    if (std::abs (from - oldFrom) < 1.0e-6 && std::abs (to - oldTo) < 1.0e-6) return false;   // edge

    pendingFrom = from;
    pendingTo   = to;
    pendingApplyAt = juce::Time::getMillisecondCounter() + kSectionApplyDelayMs;
    return true;
}

void TonalBalanceLens::a11yApplyPendingSection()
{
    if (pendingFrom < 0.0 || pendingTo < 0.0) return;
    const double total = stripSeconds();
    // The whole file means "no section", like a mouse double-click.
    if (pendingFrom <= 0.0 && pendingTo >= total - 1.0e-3) processor.clearReferenceRange();
    else                                                   processor.setReferenceRange (pendingFrom, pendingTo);
    pendingFrom = pendingTo = -1.0;
    repaint();
}

// End of the background reference analysis (after loading a file or changing the section): speak the result.
juce::String TonalBalanceLens::a11yPollEvent()
{
    if (pendingFrom >= 0.0 && (juce::int32) (juce::Time::getMillisecondCounter() - pendingApplyAt) >= 0)
        a11yApplyPendingSection();

    const bool busy = processor.referenceBusy();
    const auto rev  = processor.referenceRevision();
    juce::String message;

    if (busy && ! a11yWasBusy)
        message = a11ytext::T ("Analysing reference");
    else if (! busy && rev != a11yLastRevision)
    {
        const auto err = processor.referenceError();
        if (err.isNotEmpty())
            message = a11ytext::T ("Reference could not be read: ") + spoken::clean (err);
        else
        {
            message = a11ytext::T ("Reference analysed, ") + processor.referenceName();
            double from = 0.0, to = 0.0;
            if (processor.referenceRange (from, to))
                message << a11ytext::T (", section ") << spoken::duration (from) << a11ytext::T (" to ") << spoken::duration (to);
        }
    }

    a11yWasBusy = busy;
    if (! busy) a11yLastRevision = rev;
    return message;
}

juce::String TonalBalanceLens::a11yActivate (int group, int row)
{
    if (group != tbActions) return {};
    switch (row)
    {
        case actReset:
            pressControl (ctrlReset);
            return a11ytext::T ("Averaging reset");
        case actLoad:
            pressControl (ctrlLoad);
            return {};   // a file chooser opened; the screen reader announces it by itself
        case actClear:
            if (processor.referencePath().isEmpty()) return a11ytext::T ("No reference loaded");
            pendingFrom = pendingTo = -1.0;
            pressControl (ctrlClear);
            return a11ytext::T ("Reference removed");
        default:
            if (processor.referencePath().isEmpty()) return a11ytext::T ("No reference loaded");
            dragging = false;
            pendingFrom = pendingTo = -1.0;
            processor.clearReferenceRange();
            repaint();
            return a11ytext::T ("Whole reference file used");
    }
}
}
