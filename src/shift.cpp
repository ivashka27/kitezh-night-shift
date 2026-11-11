#include "kitezh/shift.hpp"

#include "kitezh/lock.hpp"

#include <stdexcept>

#include <string>
#include <set>
#include <vector>


namespace kitezh {

ShiftReport runNightShift(const std::vector<Vessel>& q) {
    ShiftReport rep;
    std::set<std::string> seen;
    std::vector<Vessel> w;
    Lock lock;

    for (const auto& v : q) {
        if (seen.count(v.call_sign) > 0) {
            rep.refused.push_back({v.call_sign, "duplicate"});
            continue;
        }
        seen.insert(v.call_sign);

        if (validateVessel(v).has_value()) {
            rep.refused.push_back({v.call_sign, "invalid"});
            continue;
        }

        if (v.draft_dm > kLowDepthDm) {
            rep.refused.push_back({v.call_sign, "draft"});
            continue;
        }

        w.push_back(v);
    }

    while (!w.empty()) {
        std::vector<Vessel> b;
        std::vector<Vessel> nw;
        b.push_back(w[0]);
        Direction d = w[0].direction;
        if (w[0].kind == VesselKind::Oversized) {
            for (size_t i = 1; i < w.size(); i++) {
                nw.push_back(w[i]);
            }
        } else {
            bool skip = false;
            for (size_t i = 1; i < w.size(); ++i) {
                const Vessel& c = w[i];
                bool ok = (c.direction == d) && (c.kind != VesselKind::Oversized) && fitsInChamber(b, c) && (!skip || c.kind == VesselKind::Ambulance);
                if (ok) {
                    b.push_back(c);
                } else {
                    skip = true;
                    nw.push_back(c);
                }
            }
        }
        w = std::move(nw);

        Level in_lvl = (d == Direction::Upstream) ? Level::Low : Level::High;
        Gate in_g = (d == Direction::Upstream) ? Gate::Lower : Gate::Upper;
        Gate out_g = (d == Direction::Upstream) ? Gate::Upper : Gate::Lower;

        if (lock.level() != in_lvl) {
            lock.pump();
        }

        lock.open(in_g);
        for (const auto& v : b) {
            lock.enter(v);
        }
        lock.close(in_g);

        lock.pump();

        lock.open(out_g);
        for (const auto& v : b) {
            lock.exit(v.call_sign);
            rep.passed.push_back(v.call_sign);
        }
        lock.close(out_g);
    }
    rep.pump_cycles = lock.pumpCycles();
    return rep;
}


}  // namespace kitezh
