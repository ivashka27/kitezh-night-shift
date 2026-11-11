#include "kitezh/vessel.hpp"

#include <stdexcept>

namespace kitezh {

std::optional<std::string> validateVessel(const Vessel& vessel) {
    if (vessel.call_sign.empty()) {
        return "empty call sign";
    }
    for (char c : vessel.call_sign) {
        if (c == ' ' or c == '\t' or c == '\n') {
            return "bad call sign";
        }
    }
    if (vessel.length_m <= 0 or vessel.length_m > kChamberLengthM) {
        return "bad length";
    }
    if (vessel.draft_dm <= 0) {
        return "bad draft";
    }
    return std::nullopt;
}

bool fitsInChamber(const std::vector<Vessel>& inside, const Vessel& next) {
    int total = 0;
    for (const Vessel& v : inside) {
        total += v.length_m;
    }
    total += next.length_m;
    total += inside.size()*kGapM;
    return total <= kChamberLengthM;
}

}  // namespace kitezh
