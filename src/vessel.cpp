#include "kitezh/vessel.hpp"

#include <stdexcept>

namespace kitezh {

std::optional<std::string> validateVessel(const Vessel& vessel) {
    if (vessel.call_sign.empty()) {
        return "empty call sign";
    }

    for (char symbol : vessel.call_sign) {
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
    int total_length = next.length_m;
    
    for (const  Vessel& inside_vessel : inside) {
        total_length += inside_vessel.length_m;
    }

    if (!inside.empty()) {
        total_length += static_cast<int>(inside.size()) * kGapM;
    }

    return total_length <= kChamberLengthM;
}

}  // namespace kitezh
