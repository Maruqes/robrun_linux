// The EOS_Platform_Tick cadence: the rate follows the call start times.
#include "core/tick_rate.hpp"

#include <cstdio>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (condition) return;
    std::printf("  FAIL: %s\n", what);
    ++failures;
}

}  // namespace

int main() {
    TickRate rate;
    check(rate.hz() == 0.0, "no rate before two ticks");

    // 60 Hz start times, each tick costing a constant 500 us. The old code fed
    // the durations and saw dt == 0, so the rate never left zero.
    std::uint64_t t = 1000000;
    for (int i = 0; i < 200; ++i) {
        rate.observe(t);
        t += 16667;
    }
    check(rate.hz() > 59.0 && rate.hz() < 61.0, "60 Hz start times read as 60 Hz");

    rate.observe(t + 3000000);
    check(rate.hz() == 0.0, "a gap of two seconds resets the rate");

    TickRate backwards;
    backwards.observe(2000000);
    backwards.observe(1000000);
    check(backwards.hz() == 0.0, "a clock that runs backwards does not produce a rate");

    if (failures) {
        std::printf("tick_rate_test: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("tick_rate_test: ok\n");
    return 0;
}
