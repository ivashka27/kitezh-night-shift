#include "kitezh/vessel.hpp"

#include <stdexcept>

namespace kitezh {

    bool hasBadSign(const std::string& s) {
        for (char c : s) {
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                return true;
            }
        }
        return false;
    }

std::optional<std::string> validateVessel(const Vessel& vessel) {

    // 1. пустой call_sign → "empty call sign"
    if (vessel.call_sign.empty()) {
        return "empty call sign";
    }

    // 2. пробел, таб или перевод строки в позывном → "bad call sign"
    if (hasBadSign(vessel.call_sign)) {
        return "bad call sign";
    }

    // 3. length_m <= 0 или length_m > 36 → "bad length"
    if (vessel.length_m <= 0 || vessel.length_m > 36) {
        return "bad length";
    }

    // 4. draft_dm <= 0 → "bad draft"
    if (vessel.draft_dm <= 0) {
        return "bad draft";
    }

    // Все проверки пройдены:
    return std::nullopt;
}

bool fitsInChamber(const std::vector<Vessel>& ships_inside, const Vessel& next_ship) {

    long long total_length = next_ship.length_m;

    //суммирование длин судов внутри
    for (int i = 0; i < (int)(ships_inside.size()); i++) {
        total_length += ships_inside[i].length_m;
    }

    // + зазоры
    total_length += ships_inside.size();

    return total_length <= 36;
}

}  // namespace kitezh
