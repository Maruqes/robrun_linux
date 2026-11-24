// One performance window, shared by the shim's timed phases.
//
// Buckets of microseconds accumulate until the caller closes the window against
// a caller-supplied clock reading; then one greppable summary line reports
// where the time went. Platform-free on purpose: the host suite verifies the
// accounting without Windows.
//
// The clock is steady_clock, not GetTickCount64: the same ~15.6 ms tick
// quantisation would smear every average here.
//
// Two copies of every bucket: the window copy restarts with each perf line and
// feeds the logs, the totals copy never restarts and feeds the diagnostic
// section, which turns bucket deltas into per-second rates. One add() feeds
// both, so no phase is accounted twice or missed.
#pragma once

#include <chrono>
#include <cstdint>
#include <cstdio>

class PerfWindow {
public:
    static constexpr std::uint32_t kMaxBuckets = 8;

    static std::uint64_t now_us() {
        const auto since_epoch = std::chrono::steady_clock::now().time_since_epoch();
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(since_epoch).count());
    }

    static std::uint64_t since_us(std::uint64_t then) {
        const std::uint64_t now = now_us();
        return now > then ? now - then : 0;
    }

    // Starts the window on first use. Returns true when this call started it,
    // so the caller can skip the close check for the starting sample.
    bool ensure_started(std::uint64_t now) {
        if (ever_started_) return false;
        restart(now);
        ever_started_ = true;
        return true;
    }

    void restart(std::uint64_t now) {
        start_ = now;
        for (Bucket& b : buckets_) b = Bucket{};
        // totals_ is deliberately not cleared: the diagnostics reader diffs it
        // between polls, and a reset here would punch a hole in that stream.
    }

    bool closed(std::uint64_t now, std::uint64_t window_us) const {
        if (now < start_) return false;
        return now - start_ >= window_us;
    }

    void add(std::uint32_t bucket, std::uint64_t us) {
        if (bucket >= kMaxBuckets) return;
        Bucket& b = buckets_[bucket];
        ++b.count;
        b.total_us += us;
        if (us > b.max_us) b.max_us = us;
        Bucket& t = totals_[bucket];
        ++t.count;
        t.total_us += us;
        if (us > t.max_us) t.max_us = us;
    }

    std::uint64_t start_us() const { return start_; }
    std::uint64_t count(std::uint32_t bucket) const { return slot(bucket).count; }
    std::uint64_t total_us(std::uint32_t bucket) const { return slot(bucket).total_us; }
    std::uint64_t max_us(std::uint32_t bucket) const { return slot(bucket).max_us; }

    // Lifetime totals across every window: count, summed us, worst single us.
    std::uint64_t monotonic_count(std::uint32_t bucket) const {
        return total_slot(bucket).count;
    }
    std::uint64_t monotonic_total_us(std::uint32_t bucket) const {
        return total_slot(bucket).total_us;
    }
    std::uint64_t monotonic_max_us(std::uint32_t bucket) const {
        return total_slot(bucket).max_us;
    }

    // One summary part per bucket that has samples, in bucket order:
    // "name=12.3ms(4 avg0.68 max4.0)" joined by single spaces. A bucket with no
    // samples adds nothing, so a quiet subsystem is invisible instead of
    // diluting the line with zeros.
    std::size_t format(char* out, std::size_t cap, const char* const* names) const {
        if (!out || cap == 0) return 0;
        out[0] = '\0';
        std::size_t pos = 0;
        for (std::uint32_t b = 0; b < kMaxBuckets; ++b) {
            if (!buckets_[b].count || !names || !names[b]) continue;
            const double total_ms = static_cast<double>(buckets_[b].total_us) / 1000.0;
            const double avg_ms = total_ms / static_cast<double>(buckets_[b].count);
            const double max_ms = static_cast<double>(buckets_[b].max_us) / 1000.0;
            const int written = std::snprintf(
                out + pos, cap - pos, "%s%s=%.1fms(%llu avg%.2f max%.1f)", pos ? " " : "",
                names[b], total_ms, static_cast<unsigned long long>(buckets_[b].count),
                avg_ms, max_ms);
            if (written < 0) break;
            if (static_cast<std::size_t>(written) >= cap - pos) {
                pos = cap - 1;
                break;
            }
            pos += static_cast<std::size_t>(written);
        }
        return pos;
    }

private:
    struct Bucket {
        std::uint64_t count = 0;
        std::uint64_t total_us = 0;
        std::uint64_t max_us = 0;
    };

    const Bucket& slot(std::uint32_t bucket) const {
        static const Bucket empty;
        return bucket < kMaxBuckets ? buckets_[bucket] : empty;
    }

    const Bucket& total_slot(std::uint32_t bucket) const {
        static const Bucket empty;
        return bucket < kMaxBuckets ? totals_[bucket] : empty;
    }

    std::uint64_t start_ = 0;
    bool ever_started_ = false;
    Bucket buckets_[kMaxBuckets] = {};
    Bucket totals_[kMaxBuckets] = {};
};
