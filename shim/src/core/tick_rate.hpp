// The cadence of EOS_Platform_Tick: an exponential moving average of the
// call rate, fed with the monotonic timestamp of each call.
//
// Platform-free on purpose: the host suite links this file.
#pragma once

#include <cstdint>

class TickRate {
public:
    // `now_us` is the monotonic time the tick started, not its duration.
    // A gap of two seconds or more resets the rate to zero.
    void observe(std::uint64_t now_us);
    double hz() const { return hz_; }

private:
    std::uint64_t last_us_ = 0;
    double hz_ = 0.0;
};
