#include "core/ki_meter.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dbr {

KiMeter::KiMeter(KiTuning tuning, float initialValue)
    : tuning_(tuning), value_(initialValue) {
    if (!std::isfinite(tuning_.maximum) || tuning_.maximum <= 0.0F ||
        !std::isfinite(tuning_.regenerationPerSecond) || tuning_.regenerationPerSecond < 0.0F ||
        !std::isfinite(tuning_.regenerationDelaySeconds) || tuning_.regenerationDelaySeconds < 0.0 ||
        !std::isfinite(value_) || value_ < 0.0F || value_ > tuning_.maximum) {
        throw std::invalid_argument("Invalid KI meter tuning or initial value.");
    }
}

bool KiMeter::trySpend(float amount) noexcept {
    if (!std::isfinite(amount) || amount <= 0.0F || amount > value_) {
        return false;
    }

    value_ -= amount;
    secondsSinceSpend_ = 0.0;
    hasSpent_ = true;
    return true;
}

float KiMeter::charge(float amount) noexcept {
    if (!std::isfinite(amount) || amount <= 0.0F) {
        return 0.0F;
    }

    const float gained = std::min(amount, tuning_.maximum - value_);
    if (gained > 0.0F) {
        value_ += gained;
        secondsSinceSpend_ = 0.0;
        hasSpent_ = true;
    }
    return gained;
}

void KiMeter::advance(double elapsedSeconds) {
    if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0) {
        throw std::invalid_argument("Elapsed time must be finite and non-negative.");
    }

    if (elapsedSeconds == 0.0 || tuning_.regenerationPerSecond == 0.0F || value_ >= tuning_.maximum) {
        return;
    }

    double regenerationTime = elapsedSeconds;
    if (hasSpent_) {
        const double newTime = secondsSinceSpend_ + elapsedSeconds;
        if (!std::isfinite(newTime)) {
            throw std::invalid_argument("Elapsed time exceeds the supported range.");
        }

        const double beforeDelay = std::max(0.0, secondsSinceSpend_ - tuning_.regenerationDelaySeconds);
        const double afterDelay = std::max(0.0, newTime - tuning_.regenerationDelaySeconds);
        regenerationTime = afterDelay - beforeDelay;
        secondsSinceSpend_ = newTime;
    }

    const double restored = static_cast<double>(value_) +
        static_cast<double>(tuning_.regenerationPerSecond) * regenerationTime;
    value_ = static_cast<float>(std::min(static_cast<double>(tuning_.maximum), restored));
}

float KiMeter::value() const noexcept {
    return value_;
}

float KiMeter::maximum() const noexcept {
    return tuning_.maximum;
}

}