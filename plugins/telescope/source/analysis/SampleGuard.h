#pragma once
#include <cmath>

// ========================================================================================================
// Periscope: what the analysis does with samples it cannot measure.
//
// One NaN or infinity from a broken plugin earlier in the chain puts NaN into every recursive state it reaches (the
// K-weighting biquads, the spectrum averages, the sliding sums of the band correlation, the Field grid) and keeps it
// there until a reset: loudness then reads silence and integrated freezes, silently, for the rest of the session. In
// Monitor FX nobody presses Reset. Huge finite values do the same through the float FFT, whose power overflows.
//
// So the engine gets NaN as silence and anything beyond +-kMaxSample clamped. kMaxSample is +100 dBFS: no real
// signal is that loud, and under it the float FFT of the largest order cannot overflow. Every sample under it passes
// bit for bit, so no measurement of a normal signal changes. The audio itself is never touched: this runs on copies,
// on the analysis threads.
// ========================================================================================================
namespace telescope
{
inline constexpr float kMaxSample = 1.0e5f;

inline void sanitiseForAnalysis (float* data, int n) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        const float x = data[i];
        // One comparison catches the common case; NaN fails it too and lands below.
        if (std::abs (x) <= kMaxSample) continue;
        data[i] = std::isnan (x) ? 0.0f : (x > 0.0f ? kMaxSample : -kMaxSample);
    }
}
}
