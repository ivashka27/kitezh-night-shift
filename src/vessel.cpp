#include "kitezh/vessel.hpp"

#include <stdexcept>

namespace kitezh {

std::optional<std::string> validateVessel(const Vessel& vessel) {
    if (vessel.call_sign.empty()) {
        return "empty call sign";
    }

    for (size_t i = 0; i < vessel.call_sign.size(); ++i) {
        char ch = vessel.call_sign[i];
        if (ch == ' ' || ch == '\t' || ch == '\n') {
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
    if (next.length_m <= 0 || next.length_m > kChamberLengthM) {
        return false;
    }

    int used_length = next.length_m;
    for (size_t i = 0; i < inside.size(); ++i) {
        used_length += inside[i].length_m + kGapM;
    }

    return used_length <= kChamberLengthM;
}

}  // namespace kitezh