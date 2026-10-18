#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <vector>

// ========================================================================================================
// LoudnessHistory — el ring que alimenta el gráfico de historia de la lente LOUDNESS: 10 minutos de
// momentary y short-term a 10 Hz (un punto por hop de 100 ms), desde el último RESET.
//
// Escribe el AnalysisThread, lee el message thread. Un juce::SpinLock de sección MUY corta (copiar N
// floats) alcanza y sobra: el escritor entra 10 veces por segundo.
// ========================================================================================================
namespace telescope
{
class LoudnessHistory
{
public:
    static constexpr int kHz       = 10;              // un punto por hop de 100 ms
    static constexpr int kCapacity = 10 * 60 * kHz;   // 10 minutos

    struct Point { float momentary, shortTerm; };

    void push (float momentary, float shortTerm) noexcept
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        points[(size_t) (writePos % kCapacity)] = { momentary, shortTerm };
        ++writePos;
    }

    void clear() noexcept
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        writePos = 0;
    }

    int size() const noexcept
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        return juce::jmin (writePos, kCapacity);
    }

    // Copia los últimos `count` puntos en orden cronológico (el más viejo primero). Devuelve cuántos copió.
    int copyLatest (std::vector<Point>& dst, int count) const
    {
        // Periscope: room is made before the spin lock, so the resize inside never allocates while the worker spins.
        dst.reserve ((size_t) juce::jlimit (0, kCapacity, count));
        const juce::SpinLock::ScopedLockType sl (lock);
        const int have = juce::jmin (writePos, kCapacity);
        const int n    = juce::jmin (count, have);
        dst.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
            dst[(size_t) i] = points[(size_t) ((writePos - n + i) % kCapacity)];
        return n;
    }

private:
    mutable juce::SpinLock lock;
    std::array<Point, kCapacity> points {};
    int writePos = 0;   // monotónico; el índice real es writePos % kCapacity
};
}

// ========================================================================================================
// ClipHistory — el ring de 1 Hz que dibuja la LÍNEA DE TIEMPO de clips de DYNAMICS: cuántos eventos
// empezaron en cada segundo, 10 minutos, desde el último RESET.
//
// Va a 1 Hz y no a 10 Hz como el de loudness a propósito: lo que se lee de un vistazo es "en qué segundo
// del tema clipeó", no en qué décima. Misma disciplina de threading que LoudnessHistory (SpinLock corto;
// acá el escritor entra UNA vez por segundo).
// ========================================================================================================
namespace telescope
{
class ClipHistory
{
public:
    static constexpr int kHz       = 1;
    static constexpr int kCapacity = 10 * 60 * kHz;   // 10 minutos

    void push (juce::uint32 eventsThisSecond) noexcept
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        seconds[(size_t) (writePos % kCapacity)] = eventsThisSecond;
        ++writePos;
    }

    void clear() noexcept
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        writePos = 0;
    }

    int size() const noexcept
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        return juce::jmin (writePos, kCapacity);
    }

    // Copia los últimos `count` segundos en orden cronológico (el más viejo primero). Devuelve cuántos.
    int copyLatest (std::vector<juce::uint32>& dst, int count) const
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        const int have = juce::jmin (writePos, kCapacity);
        const int n    = juce::jmin (count, have);
        dst.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
            dst[(size_t) i] = seconds[(size_t) ((writePos - n + i) % kCapacity)];
        return n;
    }

private:
    mutable juce::SpinLock lock;
    std::array<juce::uint32, kCapacity> seconds {};
    int writePos = 0;
};

// Periscope: the broadband L/R correlation of each second, 10 minutes since the reset, for the Scope lens's
// correlation history row. The engine computes it from the hop sums whatever lens is open (it is three sums per
// sample), so the history has no gaps. A silent second is stored as kSilent and left out by the reader.
class CorrelationHistory
{
public:
    static constexpr int   kCapacity = 10 * 60;   // seconds
    static constexpr float kSilent   = 2.0f;      // outside -1..+1

    void push (float corr) noexcept
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        seconds[(size_t) (writePos % kCapacity)] = corr;
        ++writePos;
    }

    void clear() noexcept
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        writePos = 0;
    }

    // The last `count` seconds, oldest first. The destination is sized before the lock (see History::copyLatest).
    int copyLatest (std::vector<float>& dst, int count) const
    {
        dst.reserve ((size_t) juce::jlimit (0, kCapacity, count));
        const juce::SpinLock::ScopedLockType sl (lock);
        const int have = juce::jmin (writePos, kCapacity);
        const int n    = juce::jmin (count, have);
        dst.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
            dst[(size_t) i] = seconds[(size_t) ((writePos - n + i) % kCapacity)];
        return n;
    }

private:
    mutable juce::SpinLock lock;
    std::array<float, kCapacity> seconds {};
    int writePos = 0;
};
}
