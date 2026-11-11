#include "kitezh/vessel.hpp"

#include <stdexcept>

namespace kitezh {

std::optional<std::string> validateVessel(const Vessel& vessel) {
    if (vessel.call_sign.empty()) {
        return "empty call sign";
    }
    for (char el : vessel.call_sign) {
        if (std::isspace(el)) {
            return "bad call sign";
        }
    }
    if (vessel.length_m <= 0 or vessel.length_m > 36) {
        return "bad length";
    }
    if (vessel.draft_dm <= 0) {
        return "bad draft";
    }
    return std::nullopt;
}

bool fitsInChamber(const std::vector<Vessel>& a, const Vessel& vessel) {
    int total = vessel.length_m + a.size();
    for (auto ship : a) {
        total += ship.length_m;
    }
    return total <= 36;
}

}  // namespace kitezh
