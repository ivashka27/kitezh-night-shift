#include "kitezh/vessel.hpp"

#include <stdexcept>

namespace kitezh {

std::optional<std::string> validateVessel(const Vessel& vessel) {
    if (vessel.call_sign.empty()) {
        return "empty call sign";
    }
    for (char i: vessel.call_sign) {
        if (i == ' ' || i == '\t' || i == '\n') {
            return "bad call sign";
        }
    }
    if (vessel.length_m <= 0 || vessel.length_m > 36) {
        return "bad length";
    }
    if (vessel.draft_dm <= 0) {
        return "bad draft";
    }
    return std::nullopt;
}

bool fitsInChamber(const std::vector<Vessel>& inside, const Vessel& next) {
    int sum_length = next.length_m;

    for (int i = 0; i < inside.size(); i++) {
        sum_length += inside[i].length_m;
        sum_length += 1;
    }

    if (sum_length <= 36) {
        return true;
    }

    return false;
}

}  // namespace kitezh
