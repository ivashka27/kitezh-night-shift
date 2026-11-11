
#include "kitezh/vessel.hpp"

#include <cctype>
#include <string>

namespace kitezh {
namespace {

bool containsWhitespace(const std::string& text) {
    for (const unsigned char character : text) {
        if (std::isspace(character) != 0) {
            return true;
        }
    }

    return false;
}

}  // namespace

bool fitsInChamber(const std::vector<Vessel>& inside, const Vessel& next) {
    long long occupied_length = next.length_m;

    for (const Vessel& vessel : inside) {
        occupied_length += vessel.length_m;
        occupied_length += kGapM;
    }

    return occupied_length <= kChamberLengthM;
}

std::optional<std::string> validateVessel(const Vessel& vessel) {
    if (vessel.call_sign.empty()) {
        return "empty call sign";
    }

    if (containsWhitespace(vessel.call_sign)) {
        return "bad call sign";
    }

    if (vessel.length_m <= 0 || vessel.length_m > kChamberLengthM) {
        return "bad length";
    }

    if (vessel.draft_dm <= 0) {
        return "bad draft";
    }

    return std::nullopt;
}

}  // namespace kitezh

