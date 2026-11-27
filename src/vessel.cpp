#include "kitezh/vessel.hpp"

#include <stdexcept>

namespace kitezh {

std::optional<std::string> validateVessel(const Vessel& vess) {
    if (vess.call_sign.empty()) {
        return "empty call sign";
    }
    if (vess.call_sign.find_first_of(" \n\t") != std::string::npos) {
        return "bad call sign";
    }
    if (vess.length_m <= 0 || vess.length_m > 36) {
        return "bad length";
    }
    if (vess.draft_dm <= 0) {
        return "bad draft";
    }
    return std::nullopt;
}

bool fitsInChamber(const std::vector<Vessel>& inside, const Vessel& next) {
    int sum_lenght = next.length_m;
    for (const Vessel& vess : inside) {
        sum_lenght += kGapM + vess.length_m;
    }
    if (sum_lenght <= kChamberLengthM ) {
        return true;
    }
    return false;
}

}  // namespace kitezh
