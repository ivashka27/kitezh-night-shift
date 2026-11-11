#include "kitezh/vessel.hpp"

#include <stdexcept>

namespace kitezh {

std::optional<std::string> validateVessel(const Vessel& vessel) {
    if (vessel.call_sign.empty()) return "empty call sign";
    
    for (char c : vessel.call_sign){
        if (c == ' ' || c == '\t' || c == '\n') return "bad call sign";
    }

    if (vessel.length_m <= 0 || vessel.length_m > kChamberLengthM) return "bad length";

    if (vessel.draft_dm <= 0) return "bad draft";

    return std::nullopt;
}

bool fitsInChamber(const std::vector<Vessel>& container, const Vessel& vessel) {
    int l = container.size() * kGapM + vessel.length_m;
    for (auto& c : container){
        l += c.length_m;
    }
    return l <= kChamberLengthM;
}

}  // namespace kitezh
