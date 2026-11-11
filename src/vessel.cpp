#include "kitezh/vessel.hpp"

namespace kitezh {

    namespace {
        bool hasForbiddenChar(const std::string& s) {
            for (char c : s) {
                if (c == ' ' || c == '\t' || c == '\n') return true;
            }
            return false;
        }
    }

    std::optional<std::string> validateVessel(const Vessel& vessel) {
        if (vessel.call_sign.empty()) return "empty call sign";
        if (hasForbiddenChar(vessel.call_sign)) return "bad call sign";
        if (vessel.length_m <= 0 || vessel.length_m > kChamberLengthM) return "bad length";
        if (vessel.draft_dm <= 0) return "bad draft";
        return std::nullopt;
    }

    bool fitsInChamber(const std::vector<Vessel>& inside, const Vessel& next) {
        int total = next.length_m;
        for (const auto& v : inside) total += v.length_m;
        int n = static_cast<int>(inside.size()) + 1;
        if (n > 1) total += (n - 1) * kGapM;
        return total <= kChamberLengthM;
    }

}  // namespace kitezh