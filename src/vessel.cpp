#include "kitezh/vessel.hpp"

namespace kitezh {

std::optional<std::string> validateVessel(const Vessel& vessel) {
    if (vessel.call_sign.empty()) {
        return "empty call sign";
    }
    for (const char symbol : vessel.call_sign) {
        if (symbol == ' ' || symbol == '\t' || symbol == '\n') {
            return "bad call sign";
        }
    }
    if (vessel.length_m <= 0 || vessel.length_m > kChamberLengthM) {
        return "bad length";
    }
    if (vessel.draft_dm <= 0) {
        return "bad draft";
    }
    return std::nullopt;
}

bool fitsInChamber(const std::vector<Vessel>& inside, const Vessel& next) {
    int total_m = next.length_m;
    for (const Vessel& vessel : inside) {
        total_m += vessel.length_m + kGapM;
    }
    return total_m <= kChamberLengthM;
}

}  // namespace kitezh
