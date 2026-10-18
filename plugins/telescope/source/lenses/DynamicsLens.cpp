#include "lenses/DynamicsLens.h"
#include "lenses/Look.h"
#include "PluginProcessor.h"
#include "analysis/modules/Loudness.h"
#include "a11y/A11yWidgets.h"
#include "a11y/SpokenFormat.h"
#include "ui-kit/Fonts.h"
#include "ui-kit/Theme.h"
#include <cmath>

namespace telescope
{
namespace
{
namespace th = telescope::look::tint;   // F2: el tema vigente (Look.h)

constexpr float kSmoothing = 0.25f;

juce::String fmt1 (float v, bool valid) { return valid ? juce::String (v, 1) : juce::String ("--.-"); }
}

const juce::ValueTree& DynamicsLens::stateTree() const { return processor.apvts.state; }

DynamicsLens::DynamicsLens (TelescopeProcessor& p) : Lens (30), processor (p)
{
    // settleHold 2 (antes 90, "la línea de tiempo sigue corriendo aunque el número no cambie"): la línea de
    // tiempo ahora cuenta como cambio por sí misma (advanceFrame compara sus marcas), igual que las barras
    // del histograma en píxeles. Con 90 cuadros de cola, cualquier cambio —y en silencio el histograma
    // cambia: el short-term del silencio cae en el bin del borde— dejaba la lente repintando 30 veces por
    // segundo (prompt 96).
    setSettleHold (2);

    threshold.setKnobLookAndFeel (&knobLaf);
    threshold.getProperties().set ("hue", (int) th::green.getARGB());
    // F2: el valor, con la tinta secundaria del tema (en oscuro es el mismo `mut` que pone OvniKnob).
    threshold.setColour (juce::Slider::textBoxTextColourId, look::txtSecondary);
    threshold.setRange ((double) Loudness::kMinClipThresholdDbtp, (double) Loudness::kMaxClipThresholdDbtp, 0.1);
    threshold.setTextValueSuffix (" dBTP");
    threshold.setValue ((double) processor.clipThresholdDbtp(), juce::dontSendNotification);
    threshold.onValueChange = [this] { processor.setClipThresholdDbtp ((float) threshold.getValue()); };
    addAndMakeVisible (threshold);

    timelineBuf.reserve ((size_t) kTimelineSeconds);
}

DynamicsLens::~DynamicsLens()
{
    threshold.setLookAndFeel (nullptr);   // el LookAndFeel muere con la lente: hay que soltarlo antes
}

//======================================================================================== geometría
DynamicsLens::Zones DynamicsLens::zonesFor (int w, int h) const
{
    Zones z;
    auto body = juce::Rectangle<int> (0, 0, w, h).reduced (th::padIn);

    z.header = body.removeFromTop (juce::jmax (104, h / 5));
    z.footer = body.removeFromBottom (juce::jmax (50, h / 12));
    body.removeFromTop (th::padIn / 2);
    body.removeFromBottom (th::padIn / 2);

    z.clips = body.removeFromBottom (juce::jmax (110, body.getHeight() * 2 / 5));
    body.removeFromBottom (th::padIn / 2);
    z.histogram = body;

    // La barra del PSR vive en la mitad derecha del héroe.
    z.psrBar = z.header.reduced (th::padIn / 2).withTrimmedLeft (z.header.getWidth() * 2 / 5)
                       .withTrimmedRight (z.header.getWidth() / 5).withSizeKeepingCentre (
                           juce::jmax (60, z.header.getWidth() * 2 / 5 - th::padIn), 30);

    // Abajo: contador · knob · línea de tiempo.
    auto clips = z.clips.reduced (th::padIn / 2);
    clips.removeFromTop (16);                                  // el rótulo lo pone la capa estática
    auto left = clips.removeFromLeft (juce::jmax (200, clips.getWidth() / 3));
    z.knob = left.removeFromRight (juce::jmin (86, left.getWidth() / 2))
                 .withSizeKeepingCentre (juce::jmin (86, left.getWidth() / 2), juce::jmin (96, left.getHeight()));
    clips.removeFromLeft (th::padIn / 2);
    z.timeline = clips;

    auto btns = z.footer.withTrimmedLeft (z.footer.getWidth() * 3 / 5);
    const int bw = juce::jmin (110, btns.getWidth() / 2 - 8);
    z.pauseBtn = btns.removeFromRight (bw).reduced (0, 10);
    btns.removeFromRight (10);
    z.resetBtn = btns.removeFromRight (bw).reduced (0, 10);

    return z;
}

void DynamicsLens::resized()
{
    ovni::ui::VisualizerBase::resized();   // invalida la capa estática (rehornea con la nueva escala)
    zones = zonesFor (getWidth(), getHeight());
    threshold.setBounds (zones.knob);
}

//======================================================================================== capa estática
void DynamicsLens::renderStatic (juce::Graphics& g, int width, int height)
{
    zones = zonesFor (width, height);

    const auto surface = [&g] (juce::Rectangle<int> r, float alpha)
    {
        g.setColour (th::surf.withAlpha (alpha));
        g.fillRoundedRectangle (r.toFloat(), 3.0f);
        g.setColour (look::gridMinor);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 3.0f, 1.0f);
    };
    surface (zones.header,    0.75f);
    surface (zones.histogram, 0.55f);
    surface (zones.clips,     0.55f);
    surface (zones.footer,    0.45f);

    // ---- rejilla del histograma: marcas cada 6 LU ----
    const auto hist = zones.histogram.reduced (th::padIn / 2).withTrimmedTop (18).withTrimmedBottom (14);
    g.setFont (look::mono (9.0f));
    for (int lufs = kBinsFloorLufs; lufs <= 0; lufs += 6)
    {
        const float t = (float) (lufs - kBinsFloorLufs) / (float) (-kBinsFloorLufs);
        const int   x = hist.getX() + juce::roundToInt (t * (float) hist.getWidth());
        g.setColour (look::gridMinor);
        look::fillSnapped (g, { (float) (x), (float) (hist.getY()), 1.0f, (float) (hist.getHeight()) });
        g.setColour (look::txtTertiary);
        g.drawText (juce::String (lufs), x - 14, hist.getBottom() + 1, 28, 12, juce::Justification::centred, true);
    }

    g.setColour (th::mut);
    g.setFont (look::label (10.0f));
    g.drawText (tr (strings::Key::histogramShortTerm),
                hist.getX(), zones.histogram.getY() + 6, hist.getWidth(), 14, juce::Justification::left, true);
    g.drawText (tr (strings::Key::clips) + juce::String::fromUTF8 (" \xc2\xb7 ")
                    + tr (strings::Key::lastMinutes),
                zones.clips.getX() + th::padIn / 2, zones.clips.getY() + 6,
                zones.clips.getWidth(), 14, juce::Justification::left, true);
}

//======================================================================================== capa viva
void DynamicsLens::paintLive (juce::Graphics& g)
{
    if (zones.header.isEmpty()) { zones = zonesFor (getWidth(), getHeight()); threshold.setBounds (zones.knob); }

    paintHero (g);
    paintHistogram (g);
    paintClips (g);

    paintButton (g, zones.resetBtn, tr (strings::Key::reset), false, hovered == 0);
    paintButton (g, zones.pauseBtn, tr (processor.isAnalysisPaused() ? strings::Key::resume : strings::Key::pause),
                 processor.isAnalysisPaused(), hovered == 1);
}

void DynamicsLens::paintHero (juce::Graphics& g) const
{
    const auto hue = th::green;
    auto head = zones.header.reduced (th::padIn / 2);

    // ---- PSR, número héroe ----
    auto hero = head.removeFromLeft (head.getWidth() * 2 / 5);
    g.setColour (look::txtTertiary);
    g.setFont (look::label (10.0f));
    g.drawText ("PSR", hero.getX(), hero.getY(), hero.getWidth(), 12, juce::Justification::left, true);

    const auto heroSize = (float) juce::jmin (54, hero.getHeight() - 34);
    const auto psrText  = fmt1 (dispPsr, psrValid);
    g.setColour (psrValid ? th::txt : th::mut);
    g.setFont (look::mono (heroSize));
    const auto textW = (int) std::ceil (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), psrText)) + 4;
    const int  textH = juce::roundToInt (heroSize * 1.2f);
    g.drawText (psrText, hero.getX(), hero.getY() + 14, textW, textH, juce::Justification::left, true);

    // La unidad PEGADA al número (como en LOUDNESS): suelta en el medio del panel no se lee como su unidad.
    g.setColour (th::mut);
    g.setFont (look::label (juce::jmax (9.0f, heroSize * 0.28f)));
    g.drawText ("dB", hero.getX() + textW + 6, hero.getY() + 14 + textH - 16, 40, 14,
                juce::Justification::left, true);

    // ---- la barra 0…20 dB con la línea de referencia en 8 ----
    const auto bar = zones.psrBar;
    g.setColour (th::bg1);
    g.fillRect (bar);
    g.setColour (look::gridMinor);
    g.drawRect (bar, 1);

    if (psrValid)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (dispPsr - kPsrMin) / (kPsrMax - kPsrMin));
        // ===== 56: ZONAS DE COLOR =====
        // La barra era del mismo verde con 2 dB de PSR que con 18. Ahora el color dice de qué lado de la
        // referencia dinámica del spec §5.8 está: alerta bien apretado, ámbar acercándose, verde con aire.
        // Sigue siendo REFERENCIA y no veredicto — el número no se colorea, se colorea la barra.
        const auto zone = dispPsr < kPsrAlert  ? look::alert
                        : dispPsr < processor.psrTargetDb() ? look::caution   // Periscope: the user's target
                                                  : look::dataLine;
        g.setGradientFill (juce::ColourGradient (zone.withAlpha (0.45f), (float) bar.getX(), 0.0f,
                                                 zone.withAlpha (0.92f),
                                                 (float) bar.getX() + t * (float) bar.getWidth(), 0.0f, false));
        const float fw = t * (float) (bar.getWidth() - 2);
        g.fillRect (juce::Rectangle<float> ((float) bar.getX() + 1.0f, (float) bar.getY() + 1.0f, fw,
                                            (float) bar.getHeight() - 2.0f));
        // 57b — EL FILO. El degradado dice "de acá para allá hay"; el filo dice CUÁNTO, que es el número
        // que se lee de reojo mientras se mezcla.
        g.setColour (zone.withAlpha (0.95f));
        g.fillRect (juce::Rectangle<float> ((float) bar.getX() + 1.0f + juce::jmax (0.0f, fw - 2.0f),
                                            (float) bar.getY() + 1.0f, 2.0f, (float) bar.getHeight() - 2.0f));
    }

    // REFERENCIA, no veredicto: la línea del spec §5.8 dibujada y ROTULADA, sin colorear el número.
    // Periscope: the line is the user's PSR target (8 dB until changed), so the picture shows the target the speech uses.
    const float target = processor.psrTargetDb();
    const float rt = (target - kPsrMin) / (kPsrMax - kPsrMin);
    const int   rx = bar.getX() + juce::roundToInt (rt * (float) bar.getWidth());
    g.setColour (th::amber.withAlpha (0.85f));
    look::fillSnapped (g, { (float) (rx), (float) (bar.getY() - 3), 1.0f, (float) (bar.getHeight() + 6) });
    g.setFont (look::label (9.5f));
    g.setColour (th::amber.withAlpha (0.9f));
    g.drawText (tr (strings::Key::dynamicRefShort) + spoken::trimmed (target, 1) + " dB", rx - 60, bar.getBottom() + 5, 130, 12,
                juce::Justification::centred, true);

    g.setColour (look::txtTertiary);
    g.setFont (look::mono (9.0f));
    g.drawText ("0",  bar.getX(), bar.getY() - 14, 20, 12, juce::Justification::left, true);
    g.drawText ("20", bar.getRight() - 20, bar.getY() - 14, 20, 12, juce::Justification::right, true);

    // ---- PLR, secundario ----
    auto side = zones.header.reduced (th::padIn / 2);
    side = side.withTrimmedLeft (side.getWidth() * 4 / 5);
    g.setColour (look::txtTertiary);
    g.setFont (look::label (10.0f));
    g.drawText ("PLR", side.getX(), side.getY(), side.getWidth(), 12, juce::Justification::left, true);
    g.setColour (plrValid ? th::txt : th::mut);
    g.setFont (look::mono (24.0f));
    g.drawText (fmt1 (plr, plrValid), side.getX(), side.getY() + 16, side.getWidth(), 30,
                juce::Justification::topLeft, true);
    g.setColour (look::txtTertiary);
    g.setFont (look::label (9.5f));
    g.drawFittedText (tr (strings::Key::plrSinceReset),
                      side.getX(), side.getY() + 48, side.getWidth(), 28, juce::Justification::topLeft, 2);
}

void DynamicsLens::paintHistogram (juce::Graphics& g) const
{
    const auto hist = zones.histogram.reduced (th::padIn / 2).withTrimmedTop (18).withTrimmedBottom (14);
    if (hist.isEmpty() || histMax == 0) return;

    const auto hue = th::green;
    const float barW = (float) hist.getWidth() / (float) kBins;

    for (int i = 0; i < kBins; ++i)
    {
        if (histogram[i] == 0) continue;
        const float t = (float) histogram[i] / (float) histMax;
        const int   h = juce::jmax (1, juce::roundToInt (t * (float) hist.getHeight()));
        const int   x = hist.getX() + juce::roundToInt ((float) i * barW);
        const int   w = juce::jmax (1, juce::roundToInt (barW) - 1);

        // 56: el bin ACTUAL con el acento (es "dónde está el tema ahora") y el resto del histograma con
        // el relleno del sistema visual, que pesa lo justo para leerse como distribución y no competir.
        // 57b — barra antialiaseada con DEGRADADO y tapa brillante (look::dataBar): un histograma de
        // rectángulos planos se lee como una pared; con la tapa, cada barra tiene un filo que ES su valor.
        look::dataBar (g, juce::Rectangle<float> ((float) x, (float) (hist.getBottom() - h),
                                                  (float) w, (float) h),
                       i == currentBin ? look::accent : hue);
    }

    // El bin ACTUAL, marcado aunque todavía no tenga altura: es donde está el tema ahora mismo.
    if (currentBin >= 0)
    {
        const int x = hist.getX() + juce::roundToInt ((float) currentBin * barW);
        const float bw = (float) juce::jmax (1, juce::roundToInt (barW));
        g.setColour (look::accent.withAlpha (0.25f));
        g.fillRect (juce::Rectangle<float> ((float) x - 1.5f, (float) hist.getY() - 6.0f, bw + 3.0f, 6.5f));
        g.setColour (look::accent);
        g.fillRect (juce::Rectangle<float> ((float) x, (float) hist.getY() - 4.0f, bw, 3.0f));
    }
}

void DynamicsLens::paintClips (juce::Graphics& g) const
{
    const auto hue = th::green;
    auto area = zones.clips.reduced (th::padIn / 2);
    area.removeFromTop (16);

    // ---- contador ----
    auto counter = area.removeFromLeft (juce::jmax (200, area.getWidth() / 3) - zones.knob.getWidth() - th::padIn / 2);
    g.setColour (clipEvents > 0 ? th::red : th::txt);
    g.setFont (look::mono ((float) juce::jmin (40, counter.getHeight() - 24)));
    g.drawText (juce::String ((int) clipEvents), counter.getX(), counter.getY(), counter.getWidth(),
                counter.getHeight() - 18, juce::Justification::topLeft, true);
    g.setColour (look::txtTertiary);
    g.setFont (look::label (9.5f));
    g.drawText (tr (strings::Key::eventsAbove) + juce::String (processor.clipThresholdDbtp(), 1)
                    + " dBTP",
                counter.getX(), counter.getBottom() - 16, counter.getWidth(), 14,
                juce::Justification::left, true);

    // ---- línea de tiempo: 10 min, el ahora a la DERECHA (misma convención que la historia de LOUDNESS) ----
    const auto tl = zones.timeline;
    if (tl.isEmpty()) return;

    g.setColour (th::bg1);
    g.fillRect (tl);
    g.setColour (look::gridMinor);
    g.drawRect (tl, 1);

    for (int min = 2; min <= 10; min += 2)
    {
        const int x = tl.getRight() - juce::roundToInt ((float) min / 10.0f * (float) tl.getWidth());
        g.setColour (look::gridMinor);
        look::fillSnapped (g, { (float) (x), (float) (tl.getY() + 1), 1.0f, (float) (tl.getHeight() - 2) });
        g.setColour (look::txtTertiary);
        g.setFont (look::mono (8.5f));
        g.drawText ("-" + juce::String (min), x + 3, tl.getBottom() - 12, 24, 11, juce::Justification::left, true);
    }

    const auto n = (int) timelineBuf.size();
    for (int i = 0; i < n; ++i)
    {
        if (timelineBuf[(size_t) i] == 0u) continue;
        const float age = (float) (n - 1 - i);                        // segundos hacia atrás desde el ahora
        const int   x   = tl.getRight() - juce::roundToInt (age / (float) kTimelineSeconds * (float) tl.getWidth());
        if (x < tl.getX()) continue;
        // 57b — con HALO: un segundo mide menos de un píxel, así que el clip es un trazo de 2 px que en
        // una línea de tiempo de diez minutos se pierde. El halo lo hace encontrable sin agrandar el dato.
        g.setColour (look::alert.withAlpha (0.20f));
        g.fillRect (juce::Rectangle<float> ((float) x - 2.5f, (float) tl.getY() + 1.0f, 5.0f,
                                            (float) tl.getHeight() - 2.0f));
        g.setColour (look::alert.withAlpha (0.95f));
        g.fillRect (juce::Rectangle<float> ((float) x - 1.0f, (float) tl.getY() + 2.0f, 2.0f,
                                            (float) tl.getHeight() - 4.0f));
    }

    g.setColour (look::txtTertiary);
    g.setFont (look::label (9.5f));
    g.drawText (trLower (strings::Key::threshold), zones.knob.getX(), zones.knob.getY() - 13,
                zones.knob.getWidth(), 12, juce::Justification::centred, true);
    juce::ignoreUnused (hue);
}

void DynamicsLens::paintButton (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& text,
                                bool active, bool hovered_) const
{
    const auto hue = th::green;
    const auto r = area.toFloat();

    g.setColour (active ? hue.withAlpha (0.18f) : th::surf2);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (hue.withAlpha (active ? 0.9f : (hovered_ ? 0.55f : 0.28f)));
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    if (hovered_)
    {
        g.setColour (hue.withAlpha (th::state::hoverGlow));
        g.fillRoundedRectangle (r, 3.0f);
    }
    g.setColour (active ? hue : th::txt);
    g.setFont (look::label (11.0f));
    g.drawText (text, area, juce::Justification::centred, true);
}

//======================================================================================== animación
bool DynamicsLens::advanceFrame()
{
    const auto& f = processor.analysis().read();
    latest     = f.loudness;
    psr        = f.psr;
    plr        = f.plr;
    psrValid   = f.psrValid;
    plrValid   = f.plrValid;
    crestDb    = f.crestDb;      // Periscope
    crestValid = f.crestValid;
    clipEvents = f.clipEvents;

    histMax = 1;
    for (int i = 0; i < kBins; ++i)
    {
        histogram[i] = f.histogram[i];
        histMax  = juce::jmax (histMax, histogram[i]);
    }

    // El histograma cambia A LA VISTA cuando alguna barra cambia de altura en píxeles (la misma cuenta que
    // paintHistogram). Antes contaba la suma de los bins: en silencio el short-term cae en el bin del
    // borde, la suma sube 10 veces por segundo y casi nunca mueve un píxel.
    bool barsMoved = false;
    {
        const int barsH = zones.histogram.reduced (th::padIn / 2).withTrimmedTop (18).withTrimmedBottom (14).getHeight();
        for (int i = 0; i < kBins; ++i)
        {
            const int hpx = histogram[i] == 0 ? 0
                          : juce::jmax (1, juce::roundToInt ((float) histogram[i] / (float) histMax * (float) barsH));
            barsMoved = barsMoved || hpx != lastBarPx[i];
            lastBarPx[i] = hpx;
        }
    }

    currentBin = (f.loudness.shortTerm > (float) kBinsFloorLufs - 0.5f && f.loudness.shortTerm < 1.0f)
                   ? juce::jlimit (0, kBins - 1, juce::roundToInt (f.loudness.shortTerm) - kBinsFloorLufs)
                   : -1;

    lastTimeline = timelineBuf;
    processor.clipHistory().copyLatest (timelineBuf, kTimelineSeconds);
    const bool timelineMoved = timelineBuf != lastTimeline;   // las marcas corren un lugar por segundo
    const float thresholdDb  = processor.clipThresholdDbtp();
    const bool thresholdMoved = thresholdDb != lastThresholdDb;
    lastThresholdDb = thresholdDb;

    const float before = dispPsr;
    if (! psrValid)                    dispPsr = 0.0f;
    else if (prefersReducedMotion())   dispPsr = psr;
    else                               dispPsr += (psr - dispPsr) * kSmoothing;

    // La lente dibuja CUATRO cosas y sólo una de ellas es el PSR. Mirar sólo el PSR para decidir si hay
    // que repintar deja un agujero real: con una señal casi periódica el PSR converge a un valor fijo, el
    // chasis pausa el repaint a los `settleHold` frames, y un clip nuevo (o un short-term que cae en otro
    // bin, o un PLR que se mueve) no se vería hasta que el PSR volviera a moverse (LOW del revisor del 49).
    bool changed = std::abs (dispPsr - before) > 1.0e-3f;
    changed |= (clipEvents != lastClipEvents);
    changed |= barsMoved || timelineMoved || thresholdMoved;
    changed |= (currentBin != lastBin);
    changed |= (std::abs (plr - lastPlr) > 1.0e-3f) || (plrValid != lastPlrValid);

    lastClipEvents = clipEvents;
    lastBin        = currentBin;
    lastPlr        = plr;
    lastPlrValid   = plrValid;
    return changed;
}

//======================================================================================== interacción
void DynamicsLens::mouseDown (const juce::MouseEvent& e)
{
    if (zones.resetBtn.contains (e.getPosition()))      processor.resetAnalysis();
    else if (zones.pauseBtn.contains (e.getPosition())) processor.setAnalysisPaused (! processor.isAnalysisPaused());
    else return;
    repaint();
}

void DynamicsLens::mouseMove (const juce::MouseEvent& e)
{
    const int was = hovered;
    hovered = zones.resetBtn.contains (e.getPosition()) ? 0
            : (zones.pauseBtn.contains (e.getPosition()) ? 1 : -1);
    if (hovered != was) repaint();
}

void DynamicsLens::mouseExit (const juce::MouseEvent&)
{
    if (hovered != -1) { hovered = -1; repaint(); }
}

//======================================================================================== accessibility
// PLR and PSR are the lens's main numbers; the histogram is told in one line (the most frequent level), the clip
// strip as a count over the last 10 minutes plus how long ago the last one was. The PSR warning colour is said as a word.
namespace
{
// Periscope: drBelowTarget (how long PSR was under the target), drCrest (classic crest factor) and daPsrTarget.
enum DynRow { drPlr = 0, drPsr, drBelowTarget, drCrest, drShortTerm, drMostFrequent, drMostOfTime, drClips, drRecentClips, kDynRows };
enum DynAction { daThreshold = 0, daPsrTarget, daPause, daReset, kDynActions };
}

juce::String DynamicsLens::a11yGroupName (int group) const
{
    return group == 0 ? a11ytext::T ("Readings") : a11ytext::T ("Actions");
}

int DynamicsLens::a11yRowCount (int group) const
{
    return group == 0 ? (int) kDynRows : (int) kDynActions;
}

juce::String DynamicsLens::a11yRowName (int group, int row) const
{
    if (group == 1)
    {
        static const char* const actions[kDynActions] = { "Clip threshold", "PSR target", "Pause", "Reset" };
        return a11ytext::T (actions[juce::jlimit (0, (int) kDynActions - 1, row)]);
    }
    static const char* const names[kDynRows] = { "PLR", "PSR", "Time below the PSR target", "Crest factor", "Short-term", "Most frequent short-term",
                                                 "Most of the time", "Clip events", "Clips in the last 10 minutes" };
    return a11ytext::T (names[juce::jlimit (0, (int) kDynRows - 1, row)]);
}

juce::String DynamicsLens::a11yRowValue (int group, int row) const
{
    if (group == 1)
    {
        if (row == daThreshold) return spoken::trimmed (processor.clipThresholdDbtp(), 1) + a11ytext::T (" dBTP");
        if (row == daPsrTarget) return spoken::dB (processor.psrTargetDb(), 1);
        if (row == daPause)     return processor.isAnalysisPaused() ? a11ytext::T ("paused") : a11ytext::T ("running");
        return {};
    }

    switch (row)
    {
        case drPlr: return plrValid ? spoken::trimmed (plr, 1) + a11ytext::T (" dB") : juce::String (a11ytext::T ("no reading yet"));
        case drPsr:
            if (! psrValid) return a11ytext::T ("no reading yet");
            // Periscope: between the alert and the target the bar is amber; say it, as the colour shows it.
            return spoken::trimmed (psr, 1) + a11ytext::T (" dB")
                 + (psr < kPsrAlert ? a11ytext::T (", low")
                                    : (psr < processor.psrTargetDb() ? a11ytext::T (", below target") : juce::String()));
        case drBelowTarget: return a11yBelowTarget();
        case drCrest:
            // Sample peak over RMS since the reset: the older, simpler cousin of PLR (true peak over loudness).
            return crestValid ? spoken::dB (crestDb, 1) : juce::String (a11ytext::T ("no reading yet"));
        case drShortTerm:
            return latest.shortTerm > -100.0f ? spoken::trimmed (latest.shortTerm, 1) + a11ytext::T (" LUFS") : juce::String (a11ytext::T ("no reading"));
        case drMostFrequent:
        {
            juce::uint64 total = 0;
            int best = -1;
            for (int i = 0; i < kBins; ++i)
            {
                total += histogram[i];
                if (histogram[i] > 0 && (best < 0 || histogram[i] > histogram[best])) best = i;
            }
            if (best < 0 || total == 0) return a11ytext::T ("no reading yet");
            const int pct = juce::roundToInt (100.0 * (double) histogram[best] / (double) total);
            return juce::String (best + kBinsFloorLufs) + a11ytext::T (" LUFS, ") + juce::String (pct) + a11ytext::T (" percent of the time");
        }
        case drMostOfTime:
        {
            // The width of the histogram, as the eye sees it: where the middle 80 percent of the short-term
            // readings since the reset lie (1 LU bins, like the bars).
            juce::uint64 total = 0;
            for (int i = 0; i < kBins; ++i) total += histogram[i];
            if (total == 0) return a11ytext::T ("no reading yet");
            juce::uint64 cum = 0;
            int lo = -1, hi = -1;
            for (int i = 0; i < kBins; ++i)
            {
                cum += histogram[i];
                if (lo < 0 && (double) cum >= 0.1 * (double) total) lo = i;
                if (hi < 0 && (double) cum >= 0.9 * (double) total) hi = i;
            }
            const auto lufsAt = [] (int bin) { return juce::String (bin + kBinsFloorLufs); };
            if (hi <= lo) return lufsAt (lo) + a11ytext::T (" LUFS");
            return a11ytext::TF ("{0} to {1}", { lufsAt (lo), lufsAt (hi) + a11ytext::T (" LUFS") });
        }
        case drClips:
            return juce::String ((int) clipEvents) + a11ytext::T (" above ") + spoken::trimmed (processor.clipThresholdDbtp(), 1) + a11ytext::T (" dBTP");
        case drRecentClips:
        {
            juce::uint64 n = 0;
            int lastIdx = -1;
            for (int i = 0; i < (int) timelineBuf.size(); ++i)
                if (timelineBuf[(size_t) i] > 0) { n += timelineBuf[(size_t) i]; lastIdx = i; }
            if (n == 0) return a11ytext::T ("none");
            const int ago = (int) timelineBuf.size() - 1 - lastIdx;   // one cell is one second of analysis, the last one is the newest
            return juce::String ((juce::int64) n) + a11ytext::T (", the latest ") + juce::String (ago) + a11ytext::T (" seconds back in the analysis");
        }
        default: return {};
    }
}

bool DynamicsLens::a11yIsAdjustable (int group, int row) const
{
    return group == 1 && (row == daThreshold || row == daPsrTarget);
}

// Periscope: the share of the analysed seconds (at most the last 10 minutes) whose PSR was under the target. PSR per
// second is the highest true peak of that second and the two before it minus the second's highest short-term
// loudness: the same 3 s rule as the live PSR, taken from the per-second history the engine keeps whatever lens
// is open. Silent seconds are left out. Recomputed at most once a second: the copy is about 200 KB.
juce::String DynamicsLens::a11yBelowTarget() const
{
    const auto now = juce::Time::getMillisecondCounter();
    const float target = processor.psrTargetDb();
    if (belowCacheValid && now - belowCacheMs < 1000 && belowCacheTarget == target) return belowCacheText;

    processor.secondHistory().copyLatest (secondsBuf, SecondHistory::kCapacity);
    int measured = 0, below = 0;
    for (int i = 0; i < (int) secondsBuf.size(); ++i)
    {
        const auto& r = secondsBuf[(size_t) i];
        if (r.shortTermMax <= -70.0f) continue;
        float peak = r.tpMaxDbtp;
        for (int k = juce::jmax (0, i - 2); k < i; ++k) peak = juce::jmax (peak, secondsBuf[(size_t) k].tpMaxDbtp);
        if (peak <= kSilenceDb + 1.0f) continue;
        ++measured;
        if (peak - r.shortTermMax < target) ++below;
    }

    juce::String t;
    if (measured == 0) t = a11ytext::T ("no reading yet");
    else
        t = a11ytext::TF ("{0} of the time below {1}, over {2}",
                          { spoken::percent (juce::roundToInt (100.0 * below / measured)), spoken::dB (target, 1),
                            spoken::duration ((double) measured) });
    belowCacheText = t; belowCacheMs = now; belowCacheTarget = target; belowCacheValid = true;
    return t;
}

// Clip threshold: step 0.1 dB, coarse step 0.5 dB. The knob in the picture follows without its notification:
// that one is asynchronous, and the row would be read with the old value.
bool DynamicsLens::a11yAdjust (int group, int row, int step)
{
    if (! a11yIsAdjustable (group, row)) return false;
    if (row == daPsrTarget)
    {
        // Periscope: step 0.5 dB, coarse 1 dB, Alt+Home/End to the ends.
        const float was  = processor.psrTargetDb();
        const float size = std::abs (step) >= a11yui::kExtreme ? 100.0f : (std::abs (step) >= a11yui::kCoarse ? 1.0f : 0.5f);
        const float next = juce::jlimit (TelescopeProcessor::kMinPsrTargetDb, TelescopeProcessor::kMaxPsrTargetDb,
                                         std::round ((was + (step > 0 ? size : -size)) * 2.0f) / 2.0f);
        if (std::abs (next - was) < 1.0e-4f) return false;
        processor.setPsrTargetDb (next);
        repaint();
        return true;
    }
    const float now = processor.clipThresholdDbtp();
    const float size = std::abs (step) >= a11yui::kExtreme ? 100.0f : (std::abs (step) >= a11yui::kCoarse ? 0.5f : 0.1f);
    const float next = juce::jlimit (Loudness::kMinClipThresholdDbtp, Loudness::kMaxClipThresholdDbtp,
                                     std::round ((now + (step > 0 ? size : -size)) * 10.0f) / 10.0f);
    if (std::abs (next - now) < 1.0e-4f) return false;
    processor.setClipThresholdDbtp (next);
    threshold.setValue ((double) processor.clipThresholdDbtp(), juce::dontSendNotification);
    repaint();
    return true;
}

bool DynamicsLens::a11yIsActivatable (int group, int row) const
{
    return group == 1 && row != daThreshold && row != daPsrTarget;
}

juce::String DynamicsLens::a11yActivate (int group, int row)
{
    if (group != 1) return {};
    if (row == daPause)
    {
        processor.setAnalysisPaused (! processor.isAnalysisPaused());
        repaint();
        return processor.isAnalysisPaused() ? a11ytext::T ("Paused") : a11ytext::T ("Running");
    }
    if (row == daReset)
    {
        processor.resetAnalysis();
        repaint();
        return a11ytext::T ("Analysis reset");
    }
    return {};
}
}
