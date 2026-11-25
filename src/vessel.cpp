#include "kitezh/vessel.hpp"
#include <string>
#include <stdexcept>

namespace kitezh {

std::optional<std::string> validateVessel(const Vessel& vessel) {
    if (vessel.call_sign.length() == 0) {
        return "empty call sign";
    }
    
    for (int i = 0; i < vessel.call_sign.length(); i++) {
        char c = vessel.call_sign[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
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
    int total = next.length_m;
    
    for (int i = 0; i < inside.size(); i++) {
        total = total + inside[i].length_m;
    }
    
    total = total + (inside.size() * kGapM);
    
    if (total <= kChamberLengthM) {
        return true;
    }
    return false;
}
    
}  // namespace kitezh
