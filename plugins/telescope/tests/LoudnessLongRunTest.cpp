// Periscope: after Loudness::kExactBlocks the loudness history folds into 0.01 LU histograms (see Loudness.h), so a
// meter left running for days (Monitor FX) has constant memory. Hours of audio would make this test slow, so a meter
// with a 30 s limit is compared with an exact one on the same signal.
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include <juce_core/juce_core.h>
#include "analysis/modules/Loudness.h"

using telescope::Loudness;

namespace
{
// White noise whose level steps every 5 s through a cycle from -12 to -42 dBFS, so both gates and the LRA have work.
void fillBlock (juce::Random& rng, long long start, int n, double sr, std::vector<float>& l, std::vector<float>& r)
{
    static constexpr float kLevelsDb[] = { -12.0f, -20.0f, -30.0f, -42.0f, -18.0f, -26.0f, -36.0f, -15.0f };
    for (int i = 0; i < n; ++i)
    {
        const int step = (int) ((double) (start + i) / sr / 5.0) % 8;
        const float g = std::pow (10.0f, kLevelsDb[step] / 20.0f);
        l[(size_t) i] = g * (rng.nextFloat() * 2.0f - 1.0f);
        r[(size_t) i] = g * (rng.nextFloat() * 2.0f - 1.0f);
    }
}
}

TEST_CASE ("telescope: after the exact span the loudness history is a histogram and reads the same",
           "[telescope][ebu][longrun]")
{
    constexpr double sr = 48000.0;
    Loudness exact, folded;
    exact.prepare (sr);
    folded.prepare (sr);
    folded.setExactHistoryLimit (300);   // 30 s of blocks instead of six hours

    juce::Random rng (20261027);
    constexpr int block = 4096;
    std::vector<float> l (block), r (block);
    const auto total = (long long) (180.0 * sr);
    bool sameBeforeSwitch = true;

    for (long long pos = 0; pos < total; pos += block)
    {
        const int n = (int) std::min<long long> (block, total - pos);
        fillBlock (rng, pos, n, sr, l, r);
        exact.process (l.data(), r.data(), n);
        folded.process (l.data(), r.data(), n);

        // Before the switch the folded meter is the exact one, bit for bit.
        if (! folded.usesHistogram())
        {
            const auto a = exact.result(), b = folded.result();
            sameBeforeSwitch = sameBeforeSwitch && a.integrated == b.integrated && a.lra == b.lra;
        }
    }

    const auto a = exact.result(), b = folded.result();
    std::printf ("LONGRUN exacto I=%.4f LRA=%.4f  ·  histograma I=%.4f LRA=%.4f  ·  diferencia I=%.4f LRA=%.4f\n",
                 (double) a.integrated, (double) a.lra, (double) b.integrated, (double) b.lra,
                 (double) (b.integrated - a.integrated), (double) (b.lra - a.lra));

    REQUIRE (sameBeforeSwitch);
    REQUIRE (folded.usesHistogram());
    REQUIRE_FALSE (exact.usesHistogram());
    REQUIRE (a.integratedValid);
    REQUIRE (b.integratedValid);
    REQUIRE (a.lra > 5.0f);   // the signal really has a range, so the comparison below means something
    REQUIRE (std::abs (a.integrated - b.integrated) < 0.01f);
    REQUIRE (std::abs (a.lra - b.lra) < 0.02f);

    // A reset starts exact again.
    folded.reset();
    REQUIRE_FALSE (folded.usesHistogram());
}
