#include "kitezh/vessel.hpp"
#include "kitezh/geometry.hpp"

namespace kitezh {

    std::optional<std::string> validateVessel(const Vessel& v) {
        if (v.call_sign.empty()) {
            return "empty call sign";
        }
        for (char c : v.call_sign) {
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                return "bad call sign";
            }
        }
        if (v.length_m <= 0 || v.length_m > kChamberLengthM) {
            return "bad length";
        }
        if (v.draft_dm <= 0) {
            return "bad draft";
        }
        return std::nullopt;
    }

    bool fitsInChamber(const std::vector<Vessel>& inside, const Vessel& next) {
        long long total_length = next.length_m;
        for (const auto& v : inside) {
            total_length += v.length_m;
        }
        total_length += static_cast<long long>(inside.size()) * kGapM;
        return total_length <= kChamberLengthM;
    }

} // namespace kitezh
