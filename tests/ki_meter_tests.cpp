#include "core/ki_meter.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testSpendingAndDelayedRegeneration() {
    dbr::KiMeter meter({100.0F, 5.0F, 2.0}, 50.0F);

    require(meter.trySpend(25.0F), "A valid spend should succeed.");
    require(!meter.trySpend(100.0F), "An unaffordable spend should fail.");
    require(!meter.trySpend(0.0F), "A zero spend should fail.");
    require(!meter.trySpend(std::numeric_limits<float>::quiet_NaN()), "A NaN spend should fail.");
    require(meter.value() == 25.0F, "Rejected spends must not change KI.");

    meter.advance(2.0);
    require(meter.value() == 25.0F, "KI should not regenerate during the delay.");
    meter.advance(1.0);
    require(meter.value() == 30.0F, "KI should regenerate after the delay.");
}

void testRegenerationAndClamping() {
    dbr::KiMeter meter({100.0F, 10.0F, 1.0}, 10.0F);
    meter.advance(2.0);
    require(meter.value() == 30.0F, "An unused meter should regenerate immediately.");

    meter.advance(100.0);
    require(meter.value() == meter.maximum(), "Regeneration should clamp at maximum KI.");
}

void testChargingClampsAndDelaysRegeneration() {
    dbr::KiMeter meter({100.0F, 5.0F, 2.0}, 90.0F);
    require(meter.charge(25.0F) == 10.0F, "Charging should return the amount gained up to max KI.");
    require(meter.value() == 100.0F, "Charging should not exceed max KI.");

    require(meter.trySpend(30.0F), "A valid spend should succeed after charging.");
    meter.advance(1.0);
    require(meter.charge(2.0F) == 2.0F, "Charging should restore the requested amount.");
    meter.advance(2.0);
    require(meter.value() == 72.0F, "Charging should restart the regeneration delay.");
}

void testInvalidInputs() {
    bool rejectedTuning = false;
    try {
        dbr::KiMeter meter({0.0F, 1.0F, 0.0}, 0.0F);
    } catch (const std::invalid_argument&) {
        rejectedTuning = true;
    }
    require(rejectedTuning, "A zero maximum should be rejected.");

    dbr::KiMeter meter({100.0F, 1.0F, 0.0}, 50.0F);
    bool rejectedTime = false;
    try {
        meter.advance(-1.0);
    } catch (const std::invalid_argument&) {
        rejectedTime = true;
    }
    require(rejectedTime, "Negative elapsed time should be rejected.");
}

}

int main() {
    try {
        testSpendingAndDelayedRegeneration();
        testRegenerationAndClamping();
        testChargingClampsAndDelaysRegeneration();
        testInvalidInputs();
    } catch (const std::exception& error) {
        std::cerr << "KI meter test failed: " << error.what() << '\n';
        return 1;
    }

    std::cout << "KI meter tests passed.\n";
    return 0;
}