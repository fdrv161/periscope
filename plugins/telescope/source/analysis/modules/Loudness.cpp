#include "analysis/modules/Loudness.h"
#include <algorithm>
#include <cmath>

namespace telescope
{
void Loudness::prepare (double sampleRate)
{
    sr  = sampleRate > 0.0 ? sampleRate : 48000.0;
    hop = std::max (1, (int) std::llround (sr / 10.0));   // 100 ms

    kwL.prepare (sr);
    kwR.prepare (sr);
    tpL.prepare (sr);
    tpR.prepare (sr);

    clipHoldSamples = std::max (1, (int) std::llround (sr * (double) kClipHoldMs / 1000.0));

    reset();
}

void Loudness::reset()
{
    kwL.reset(); kwR.reset();
    tpL.reset(); tpR.reset();

    accL = accR = 0.0;
    dcHopSumL = dcHopSumR = 0.0;
    dcTotalL  = dcTotalR  = 0.0;
    dcSamples = 0;
    fill = 0;
    hopsDone = 0;

    msL.clear(); msR.clear();
    blockZ.clear();
    blockLufs.clear();
    gatedBlockSorted.clear();
    absSum = relSum = 0.0;
    absCount = relCount = 0;
    relGateLast = 0.0;
    relKnown = false;
    stGatedSorted.clear();
    stPower = 0.0;
    hopTpDb.clear();
    hopTpLin = 0.0f;
    hopTpLinL = hopTpLinR = 0.0f;   // 57c (M-1 del revisor): los acumuladores por canal se limpian como el conjunto
    // 10 minutos de holgura: es la ventana de la historia del sello y evita realocar durante una medición.
    blockZ.reserve (10 * 60 * 10);
    blockLufs.reserve (10 * 60 * 10);
    gatedBlockSorted.reserve (10 * 60 * 10);
    stGatedSorted.reserve (10 * 60 * 10);

    current = Result{};
    resetClipCount();
}

// El umbral vive en dBTP pero se compara en lineal (una exponencial por muestra sería absurda).
void Loudness::setClipThresholdDbtp (float dbtp)
{
    if (dbtp == clipThreshold) return;   // el mismo umbral no reinicia nada
    clipThreshold = dbtp;
    resetClipCount();
}

void Loudness::resetClipCount()
{
    clipThresholdLin = (float) std::pow (10.0, (double) clipThreshold / 20.0);
    belowRun = clipHoldSamples;          // la PRIMERA muestra sobre el umbral abre evento
    pendingClipHop        = 0;
    current.clipEvents    = 0;
    current.clipEventsHop = 0;
}

void Loudness::process (const float* L, const float* R, int n)
{
    for (int i = 0; i < n; ++i)
    {
        // TRUE-PEAK: se mide sobre la señal CRUDA, antes del K-weighting (BS.1770 Anexo 2).
        const float l = tpL.processSample (L[i]);
        const float r = tpR.processSample (R[i]);
        const float tp = std::max (l, r);
        if (tp > 0.0f)
        {
            const auto db = (float) (20.0 * std::log10 ((double) tp));
            if (db > current.truePeakMax) current.truePeakMax = db;
        }
        if (tp > hopTpLin) hopTpLin = tp;
        // 57c — los dos valores YA ESTABAN calculados; lo único nuevo es no tirarlos.
        if (l > hopTpLinL) hopTpLinL = l;
        if (r > hopTpLinR) hopTpLinR = r;

        // EVENTOS DE CLIP. Un evento se abre en la primera muestra sobre el umbral y no se cierra hasta
        // kClipHoldMs por debajo (ver la nota del header: si no, un seno que se pasa 50 ms contaría
        // cincuenta clips, uno por ciclo).
        if (tp > clipThresholdLin)
        {
            if (belowRun >= clipHoldSamples) { ++current.clipEvents; ++pendingClipHop; }
            belowRun = 0;
        }
        else if (belowRun < clipHoldSamples)
        {
            ++belowRun;
        }

        // ===== 55: CONTINUA. Sobre la señal CRUDA, como el true-peak: el filtro K tiene un pasa-altos de
        // 38 Hz que se come justo lo que se quiere medir.
        dcHopSumL += (double) L[i];
        dcHopSumR += (double) R[i];

        const double yl = kwL.processSample ((double) L[i]);
        const double yr = kwR.processSample ((double) R[i]);
        accL += yl * yl;
        accR += yr * yr;

        if (++fill == hop)
            finishHop();
    }
}

double Loudness::meanOfLast (const std::deque<double>& d, int count) noexcept
{
    double sum = 0.0;
    const auto n = (int) d.size();
    for (int i = n - count; i < n; ++i) sum += d[(size_t) i];
    return sum / (double) count;
}

void Loudness::finishHop()
{
    // TRUE-PEAK DEL HOP: el máximo de este hop, en dBTP. Alimenta la línea de tiempo de DYNAMICS y, por
    // el ring de los últimos 3 s, el PSR.
    current.truePeakHop = hopTpLin > 0.0f ? (float) (20.0 * std::log10 ((double) hopTpLin)) : kSilenceFloor;
    hopTpLin = 0.0f;

    // 57c — el mismo número, por canal (ver Result).
    const auto toDbtp = [] (float lin) { return lin > 0.0f ? (float) (20.0 * std::log10 ((double) lin))
                                                           : kSilenceFloor; };
    current.truePeakHopL = toDbtp (hopTpLinL);
    current.truePeakHopR = toDbtp (hopTpLinR);
    current.truePeakMaxL = std::max (current.truePeakMaxL, current.truePeakHopL);
    current.truePeakMaxR = std::max (current.truePeakMaxR, current.truePeakHopR);
    hopTpLinL = hopTpLinR = 0.0f;

    // Los eventos que ABRIERON en este hop (el ring de 1 Hz de la línea de tiempo los suma de a diez).
    current.clipEventsHop = pendingClipHop;
    pendingClipHop = 0;
    hopTpDb.push_back (current.truePeakHop);
    if ((int) hopTpDb.size() > kPsrHops) hopTpDb.pop_front();

    msL.push_back (accL / (double) hop);
    msR.push_back (accR / (double) hop);
    if ((int) msL.size() > kShortTermHops) { msL.pop_front(); msR.pop_front(); }

    // ===== 55: CONTINUA. La suma del hop sale tal cual (la historia por segundo suma diez y divide una
    // vez); la media desde el reset se actualiza acá.
    current.dcSumHopL = dcHopSumL;
    current.dcSumHopR = dcHopSumR;
    dcTotalL += dcHopSumL;
    dcTotalR += dcHopSumR;
    dcSamples += hop;
    current.dcL = dcSamples > 0 ? (float) (dcTotalL / (double) dcSamples) : 0.0f;
    current.dcR = dcSamples > 0 ? (float) (dcTotalR / (double) dcSamples) : 0.0f;
    dcHopSumL = dcHopSumR = 0.0;

    accL = accR = 0.0;
    fill = 0;
    ++hopsDone;

    // ===== 57c · LO PARCIAL, desde el PRIMER hop =====
    //
    // La MISMA media de `msL`/`msR` que la oficial, pero sobre los hops que hay. Se calcula acá arriba —
    // antes de los dos `if` de abajo— justamente para que exista cuando ellos todavía no. Cuando la
    // ventana se llena, `meanOfLast` recibe el mismo `count` que la oficial y el resultado es el mismo
    // AL BIT: no es una aproximación que converge, es la misma cuenta con menos términos.
    {
        const int mHops = (int) std::min<long long> (hopsDone, kMomentaryHops);
        const int sHops = (int) std::min<long long> (hopsDone, kShortTermHops);
        current.momentaryPartial = toLufs (meanOfLast (msL, mHops) + meanOfLast (msR, mHops));
        current.shortTermPartial = toLufs (meanOfLast (msL, sHops) + meanOfLast (msR, sHops));
    }

    // MOMENTARY — bloque de 400 ms (4 hops). Cada hop produce uno: es el 75 % de solapamiento del estándar.
    if (hopsDone >= kMomentaryHops)
    {
        const double zSum = meanOfLast (msL, kMomentaryHops) + meanOfLast (msR, kMomentaryHops);
        current.momentary = toLufs (zSum);
        if (current.momentary > current.momentaryMax) current.momentaryMax = current.momentary;

        blockZ.push_back (zSum);          // éste es el bloque de compuerta del integrado
        recomputeIntegrated();
    }

    // SHORT-TERM — ventana de 3 s (30 hops), a 10 Hz.
    if (hopsDone >= kShortTermHops)
    {
        const double zSum = meanOfLast (msL, kShortTermHops) + meanOfLast (msR, kShortTermHops);
        current.shortTerm = toLufs (zSum);
        if (current.shortTerm > current.shortTermMax) current.shortTermMax = current.shortTerm;

        recomputeLra (current.shortTerm);

        // HISTOGRAMA — un bin por short-term válido. El bin i está centrado en (i - 60) LUFS: se redondea
        // (no se trunca) porque los valores caen JUSTO sobre los enteros (un seno a -20 dBFS da -20.000
        // LUFS) y truncar los mandaría al bin de al lado según el último bit. Fuera de rango va al borde,
        // así Σ bins == shortTermHops siempre.
        const int bin = std::clamp ((int) std::llround ((double) current.shortTerm) + 60, 0, kHistogramBins - 1);
        ++current.histogram[(size_t) bin];
        ++current.shortTermHops;

        // F4 (T4): el de la lente, sólo sobre la compuerta absoluta. El mismo bin; lo que cambia es quién entra.
        if ((double) current.shortTerm > kAbsGate)
        {
            ++current.histogramGated[(size_t) bin];
            ++current.shortTermHopsGated;
        }

        // PSR — TP máximo de los últimos 3 s menos el short-term. Es la lectura "en vivo" de cuánto
        // margen de pico le queda a la mezcla en este momento.
        float tpWindow = kSilenceFloor;
        for (const float v : hopTpDb) tpWindow = std::max (tpWindow, v);
        current.psr      = tpWindow - current.shortTerm;
        current.psrValid = tpWindow > kSilenceFloor;
    }

    // PLR — TP máximo desde el reset menos el integrado (AES TD1004). Vale lo mismo que el integrado:
    // si todavía no pasó la compuerta, no hay PLR que mostrar.
    current.plrValid = current.integratedValid && current.truePeakMax > kSilenceFloor;
    current.plr      = current.plrValid ? current.truePeakMax - current.integrated : 0.0f;
}

// Doble compuerta de BS.1770: absoluta -70 LUFS, después relativa a 10 LU bajo la media de los que pasaron.
// Periscope: incremental, with the same result bit for bit as summing the whole history (see Loudness.h).
void Loudness::recomputeIntegrated()
{
    // The block just pushed: its LUFS once, and the absolute-gated running sum (block order, as before).
    const double zNew = blockZ.back();
    const float  lNew = toLufs (zNew);
    blockLufs.push_back (lNew);
    const bool newAbs = lNew > kAbsGate;
    if (newAbs)
    {
        absSum += zNew;
        ++absCount;
        gatedBlockSorted.insert (std::upper_bound (gatedBlockSorted.begin(), gatedBlockSorted.end(), lNew), lNew);
    }

    if (absCount == 0)
    {
        current.integrated      = kSilenceFloor;
        current.integratedValid = false;
        return;
    }

    const double relGate = (double) toLufs (absSum / (double) absCount) + kRelGateI;

    // Does any OLDER gated block change side of the relative gate? Its side changes when its level lies in
    // (lower gate, higher gate]. The new block sits in the sorted list too, so it is taken out of the count.
    bool rescan = ! relKnown;
    if (! rescan && relGate != relGateLast)
    {
        const double lo = std::min (relGate, relGateLast), hi = std::max (relGate, relGateLast);
        const auto above = [] (double g, float v) { return g < (double) v; };
        const auto a = std::upper_bound (gatedBlockSorted.begin(), gatedBlockSorted.end(), lo, above);
        const auto b = std::upper_bound (gatedBlockSorted.begin(), gatedBlockSorted.end(), hi, above);
        auto crossing = (long long) (b - a);
        if (newAbs && (double) lNew > lo && (double) lNew <= hi) --crossing;
        rescan = crossing > 0;
    }

    if (rescan)
    {
        // The same loop as the author's, in block order, with the stored LUFS instead of two log10 per block.
        relSum   = 0.0;
        relCount = 0;
        for (size_t i = 0; i < blockZ.size(); ++i)
        {
            const double l = (double) blockLufs[i];
            if (l > kAbsGate && l > relGate) { relSum += blockZ[i]; ++relCount; }
        }
        relKnown = true;
    }
    else if (newAbs && (double) lNew > relGate)
    {
        // Same set as before plus the new block, which is the last one: adding it last is the block-order sum.
        relSum += zNew;
        ++relCount;
    }
    relGateLast = relGate;

    if (relCount == 0)
    {
        current.integrated      = kSilenceFloor;
        current.integratedValid = false;
        return;
    }

    current.integrated      = toLufs (relSum / (double) relCount);
    current.integratedValid = true;
}

// LRA — EBU Tech 3342 §5, siguiendo la implementación MATLAB de referencia del propio documento:
// compuerta absoluta >= -70, después >= (media energética de los que pasaron) - 20 LU, y P95 - P10 sobre
// los que quedan (índices redondeados sobre n-1, como el `round((n-1)*PRC/100 + 1)` del MATLAB).
void Loudness::recomputeLra (float v)
{
    // Periscope: the absolute-gated values stay sorted and their power runs in arrival order, the same order and
    // the same terms the author's loop summed, so the result is identical bit for bit (see Loudness.h).
    if (v >= (float) kAbsGate)
    {
        stPower += std::pow (10.0, (double) v / 10.0);
        stGatedSorted.insert (std::upper_bound (stGatedSorted.begin(), stGatedSorted.end(), v), v);
    }
    if (stGatedSorted.empty()) { current.lra = 0.0f; return; }

    const double integ = 10.0 * std::log10 (stPower / (double) stGatedSorted.size());
    const double gate  = integ + kRelGateLra;
    // The relative-gated values, sorted, are the tail of the list from the first value >= the gate.
    const auto first = std::partition_point (stGatedSorted.begin(), stGatedSorted.end(),
                                             [gate] (float x) { return (double) x < gate; });
    const auto count = (size_t) (stGatedSorted.end() - first);
    if (count == 0) { current.lra = 0.0f; return; }

    const auto n   = (double) count - 1.0;
    const auto lo  = (size_t) std::llround (n * 0.10);
    const auto hi  = (size_t) std::llround (n * 0.95);
    current.lra = first[(std::ptrdiff_t) hi] - first[(std::ptrdiff_t) lo];
}
}
