
#include "kitezh/vessel.hpp"

namespace kitezh {

std::optional<std::string> validateVessel(const Vessel& vessel) {
    if (vessel.call_sign.empty()) {
        return "empty call sign";
    }

    if (vessel.call_sign.find_first_of(" \t\n") != std::string::npos) {
        return "bad call sign";
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
    int totalLength = next.length_m;

    for (const Vessel& vessel : inside) {
        totalLength += vessel.length_m;
    }

    totalLength += static_cast<int>(inside.size()) * kGapM;

    return totalLength <= kChamberLengthM;
}

}  // namespace kitezh
