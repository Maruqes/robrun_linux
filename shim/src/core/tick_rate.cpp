#include "core/tick_rate.hpp"

void TickRate::observe(std::uint64_t now_us) {
    if (last_us_ && now_us > last_us_) {
        const double dt = static_cast<double>(now_us - last_us_) / 1000000.0;
        if (dt >= 2.0) {
            hz_ = 0.0;
        } else if (dt > 0.0001) {
            const double hz = 1.0 / dt;
            hz_ = hz_ > 0.0 ? hz_ * 0.9 + hz * 0.1 : hz;
        }
    }
    last_us_ = now_us;
}
