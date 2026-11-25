#include "kitezh/vessel.hpp"
#include "kitezh/geometry.hpp"
#include <stdexcept>

namespace kitezh {

std::optional<std::string> validateVessel(const Vessel& vessel) {
    if (vessel.call_sign.empty()) {
        return "empty call sign";
    }
    for (char a: vessel.call_sign) {
        if (a == ' ' || a == '\t' || a =='\n') {
            return "bad call sign";
        }
    }
    if (vessel.length_m<=0 || vessel.length_m> kChamberLengthM) {
        return"bad length";
    }
    if (vessel.draft_dm <= 0) {
        return "bad draft";
    }
    return std:: nullopt;
}

bool fitsInChamber(const std::vector<Vessel>& inside, const Vessel& vessel) {

    int total = vessel.length_m;
    for (const Vessel& ship: inside) {
        total+=ship.length_m + kGapM;
    }
    if (total <= kChamberLengthM) {
        return true;
    } else {
        return false;
    }


}

}  // namespace kitezh
