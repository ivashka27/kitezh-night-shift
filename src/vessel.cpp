#include "kitezh/vessel.hpp"
#include "kitezh/geometry.hpp"
#include <stdexcept>

namespace kitezh {

std::optional<std::string> validateVessel(const Vessel& vessel) {
    if (vessel.call_sign.empty()) {
        return "empty call sign";
    }
 for (char ch : vessel.call_sign) {
        if (ch == ' ' || ch == '\t' || ch == '\n') {
            return "bad call sign";
        }
    }
    if (vessel.length_m > kChamberLengthM|| vessel.length_m <= 0) {
        return "bad length";
    }
    if (vessel.draft_dm <= 0) {
        return "bad draft";
    }
    return std::nullopt;
}

bool fitsInChamber(const std::vector<Vessel>& inside, const Vessel& next) {
    int total_length = next.length_m;
    for (const auto& v : inside) {
        total_length += v.length_m;
    }

    int total_gaps = static_cast<int>(inside.size()) * kGapM;

    return (total_length + total_gaps) <= kChamberLengthM;
}

}  // namespace kitezh
