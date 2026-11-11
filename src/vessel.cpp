#include "kitezh/vessel.hpp"

#include <stdexcept>

namespace kitezh {

std::optional<std::string> validateVessel(const Vessel& vessel) {
    if (vessel.call_sign.empty()) {
        return "empty call sign";
    }
    for (char ch : vessel.call_sign) {
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v' || ch == '\f') {
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
    int total_m = 0;
    for (const Vessel& v : inside) {
        total_m += v.length_m + kGapM;
    }
    return total_m + next.length_m <= kChamberLengthM;
}

} // namespace kitezh
