#include "kitezh/shift.hpp"
#include "kitezh/lock.hpp"

#include <unordered_set>

namespace kitezh {

    ShiftReport runNightShift(const std::vector<Vessel>& queue) {
        ShiftReport report;
        std::unordered_set<std::string> seen;
        std::vector<Vessel> waiting;

        for (const auto& v : queue) {
            if (seen.count(v.call_sign)) {
                report.refused.push_back({ v.call_sign, "duplicate" });
                continue;
            }
            seen.insert(v.call_sign);
            if (validateVessel(v)) {
                report.refused.push_back({ v.call_sign, "invalid" });
                continue;
            }
            if (v.draft_dm > kLowDepthDm) {
                report.refused.push_back({ v.call_sign, "draft" });
                continue;
            }
            waiting.push_back(v);
        }

        Lock lock;
        while (!waiting.empty()) {
            std::vector<Vessel> party;
            std::vector<bool> taken(waiting.size(), false);

            party.push_back(waiting[0]);
            taken[0] = true;
            Direction D = waiting[0].direction;
            bool skipped = false;

            if (waiting[0].kind != VesselKind::Oversized) {
                for (size_t i = 1; i < waiting.size(); ++i) {
                    const Vessel& v = waiting[i];
                    if (v.direction != D) { skipped = true; continue; }
                    if (v.kind == VesselKind::Oversized) { skipped = true; continue; }
                    if (!fitsInChamber(party, v)) { skipped = true; continue; }
                    if (skipped && v.kind != VesselKind::Ambulance) { skipped = true; continue; }
                    party.push_back(v);
                    taken[i] = true;
                }
            }

            std::vector<Vessel> rest;
            for (size_t i = 0; i < waiting.size(); ++i)
                if (!taken[i]) rest.push_back(waiting[i]);
            waiting = rest;

            Gate inGate = (D == Direction::Upstream) ? Gate::Lower : Gate::Upper;
            Gate outGate = (D == Direction::Upstream) ? Gate::Upper : Gate::Lower;
            Level inLevel = (D == Direction::Upstream) ? Level::Low : Level::High;

            if (lock.level() != inLevel) lock.pump();
            lock.open(inGate);
            for (const auto& v : party) lock.enter(v);
            lock.close(inGate);
            lock.pump();
            lock.open(outGate);
            for (const auto& v : party) {
                report.passed.push_back(v.call_sign);
                lock.exit(v.call_sign);
            }
            lock.close(outGate);
        }

        report.pump_cycles = lock.pumpCycles();
        return report;
    }

}  // namespace kitezh