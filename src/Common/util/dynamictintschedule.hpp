#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

// Main-thread-only policy. Absolute deadlines prevent frequent input/layout
// events from postponing a capture forever. No Windows input data is retained.
class DynamicTintSchedule {
    using Tick = std::uint64_t;
    static constexpr Tick Never = std::numeric_limits<Tick>::max();
    static constexpr std::array<Tick, 6> RetryDelays {120, 300, 800, 1600, 3200, 6000};
    Tick m_Last = 0, m_Hint = Never, m_Startup = 0, m_MotionUntil = 0;
    std::size_t m_Retry = RetryDelays.size();
    unsigned m_Stable = 0;
    bool m_HasSample = false;
public:
    void Reset() noexcept { *this = {}; }
    void Activity(Tick now) noexcept { m_Hint = std::min(m_Hint, now + 100); }
    void Startup(Tick now) noexcept { m_Startup = now; m_Retry = 0; Activity(now); }
    unsigned Delay(Tick now, unsigned requestedMs, bool adaptive) const noexcept
    {
        if (!m_HasSample) return 0;
        const Tick interval = std::max(1u, requestedMs);
        Tick due = m_Last + interval;
        if (adaptive)
        {
            due = m_Last + std::max<Tick>(500, interval);
            if (now < m_MotionUntil) due = std::min(due, m_Last + std::max<Tick>(50, interval));
            const Tick eventGap = m_Last + std::max<Tick>(80, interval);
            if (m_Hint != Never) due = std::min(due, std::max(m_Hint, eventGap));
            if (m_Retry < RetryDelays.size()) due = std::min(due, std::max(m_Startup + RetryDelays[m_Retry], eventGap));
        }
        return due <= now ? 0 : static_cast<unsigned>(std::min<Tick>(due - now, 1000));
    }
    bool Due(Tick now, unsigned interval, bool adaptive) const noexcept { return Delay(now, interval, adaptive) == 0; }
    void Sampled(Tick now, bool changed) noexcept
    {
        m_HasSample = true;
        m_Last = now;
        if (m_Hint <= now) m_Hint = Never;
        while (m_Retry < RetryDelays.size() && m_Startup + RetryDelays[m_Retry] <= now) ++m_Retry;
        if (changed) { m_MotionUntil = now + 1000; m_Stable = 0; }
        else if (++m_Stable >= 3) m_MotionUntil = 0;
    }
};
