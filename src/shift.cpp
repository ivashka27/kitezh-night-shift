#include "kitezh/shift.hpp"

#include "kitezh/lock.hpp"

#include <unordered_set>

namespace kitezh {

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
    ShiftReport report;

    Lock lock;

    std::unordered_set<std::string> was;
    std::vector<Vessel> remaining;

    for (const auto& vessel : queue) {
        if (!was.insert(vessel.call_sign).second) {
            report.refused.push_back({vessel.call_sign, "duplicate"});
            continue;
        }

        if (validateVessel(vessel).has_value()) {
            report.refused.push_back({vessel.call_sign, "invalid"});
            continue;
        }

        if (vessel.draft_dm > kLowDepthDm) {
            report.refused.push_back({vessel.call_sign, "draft"});
            continue;
        }

        remaining.push_back(vessel);
    }

    while (!remaining.empty()) {
        std::vector<Vessel> batch;
        std::vector<Vessel> next_remaining;

        Vessel first = remaining.front();
        remaining.erase(remaining.begin());

        batch.push_back(first);

        Direction direction = first.direction;

        if (first.kind != VesselKind::Oversized) {
            bool skipped = false;

            for (auto& vessel : remaining) {
                bool can_take = (
                    vessel.direction == direction &&
                    vessel.kind != VesselKind::Oversized &&
                    (!skipped || vessel.kind == VesselKind::Ambulance) &&
                    fitsInChamber(batch, vessel)
                );

                if (can_take) {
                    batch.push_back(vessel);
                }
                else {
                    skipped = true;
                    next_remaining.push_back(vessel);
                }
            }
        }
        else {
            next_remaining = remaining;
        }

        remaining = next_remaining;

        Gate entry_gate = (direction == Direction::Upstream) ? Gate::Lower : Gate::Upper;
        Gate exit_gate = (direction == Direction::Upstream) ? Gate::Upper : Gate::Lower;
        // ^^^ такой же метод реализовал, но не мог его сделать публичным, поэтому приходится прописыватьэто так

        Level entry_level = (direction == Direction::Upstream) ? Level::Low : Level::High;

        if (lock.level() != entry_level) {
            lock.pump();
        }

        lock.open(entry_gate);

        for (const auto& vessel : batch) {
            lock.enter(vessel);
        }

        lock.close(entry_gate);

        lock.pump();

        lock.open(exit_gate);

        for (const auto& vessel : batch) {
            lock.exit(vessel.call_sign);
            report.passed.push_back(vessel.call_sign);
        }

        lock.close(exit_gate);
    }

    report.pump_cycles = lock.pumpCycles();

    return report;
}

}  // namespace kitezh
