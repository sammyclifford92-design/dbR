#pragma once

namespace dbr {

struct KiTuning {
    float maximum;
    float regenerationPerSecond;
    double regenerationDelaySeconds;
};

class KiMeter {
public:
    KiMeter(KiTuning tuning, float initialValue);

    bool trySpend(float amount) noexcept;
    float charge(float amount) noexcept;
    void advance(double elapsedSeconds);

    float value() const noexcept;
    float maximum() const noexcept;

private:
    KiTuning tuning_;
    float value_;
    double secondsSinceSpend_ = 0.0;
    bool hasSpent_ = false;
};

}