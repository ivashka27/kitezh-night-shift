#include "kitezh/vessel.hpp"

#include <cstddef>

namespace kitezh {

namespace {

bool isBlank(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

}  // namespace

std::optional<std::string> validateVessel(const Vessel& vessel) {
    if (vessel.call_sign.empty()) {
        return std::string("empty call sign");
    }
    for (char c : vessel.call_sign) {
        if (isBlank(c)) {
            return std::string("bad call sign");
        }
    }
    if (vessel.length_m <= 0 || vessel.length_m > kChamberLengthM) {
        return std::string("bad length");
    }
    if (vessel.draft_dm <= 0) {
        return std::string("bad draft");
    }
    return std::nullopt;
}

bool fitsInChamber(const std::vector<Vessel>& inside, const Vessel& next) {
    long long total = next.length_m;
    for (const Vessel& v : inside) {
        total += v.length_m + kGapM;
    }
    return total <= kChamberLengthM;
}

}  // namespace kitezh
