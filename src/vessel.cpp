#include "kitezh/vessel.hpp"

#include <stdexcept>

namespace kitezh {

std::optional<std::string> validateVessel(const Vessel& vessel) {
    if (vessel.call_sign.empty()) {
        return "empty call sign";
    }
    if ((vessel.call_sign.find(' ') != std::string::npos) or (vessel.call_sign.find('\t') != std::string::npos) or (vessel.call_sign.find('\n') != std::string::npos )) {
        return "bad call sign";
    }
    if ((vessel.length_m <= 0) or (vessel.length_m>kChamberLengthM)) {
        return "bad length";
    }
    if (vessel.draft_dm<=0) {
        return "bad draft";
    }
    return std::nullopt;
}

bool fitsInChamber(const std::vector<Vessel>& inside, const Vessel& next) {
    int n = inside.size();
    int sum = 0;
    for (int i = 0; i<n; ++i) {
        sum+=inside[i].length_m;
    }
    int result = sum + next.length_m + n * kGapM;
    if (result <= kChamberLengthM) {
        return true;
    }
    else {
        return false;
    }
}

}  // namespace kitezh
