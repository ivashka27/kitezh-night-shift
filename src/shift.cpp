
#include "kitezh/shift.hpp"
#include "kitezh/vessel.hpp"
#include "kitezh/lock.hpp"

#include <set>
#include <vector>

#include "kitezh/vessel.hpp"

namespace kitezh {

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
    ShiftReport report;

    std::set<std::string> seen;
    std::vector<Vessel> waiting;

    for (const Vessel& vessel : queue) {
        if (!seen.insert(vessel.call_sign).second) {
            report.refused.push_back(
                {vessel.call_sign, "duplicate"}
            );
            continue;
        }

        if (validateVessel(vessel).has_value()) {
            report.refused.push_back(
                {vessel.call_sign, "invalid"}
            );
            continue;
        }

        if (vessel.draft_dm > kLowDepthDm) {
            report.refused.push_back(
                {vessel.call_sign, "draft"}
            );
            continue;
        }

        waiting.push_back(vessel);
    }

    Lock lock;

    while (!waiting.empty()) {
        std::vector<Vessel> batch;
        std::vector<Vessel> nextWaiting;

        Vessel first = waiting.front();
        Direction direction = first.direction;

        batch.push_back(first);

        if (first.kind == VesselKind::Oversized) {
            for (std::size_t i = 1; i < waiting.size(); ++i) {
                nextWaiting.push_back(waiting[i]);
            }
        } else {
            bool skipped = false;

            for (std::size_t i = 1; i < waiting.size(); ++i) {
                const Vessel& candidate = waiting[i];

                bool canJoin =
                    candidate.direction == direction &&
                    candidate.kind != VesselKind::Oversized &&
                    fitsInChamber(batch, candidate) &&
                    (!skipped ||
                     candidate.kind == VesselKind::Ambulance);

                if (canJoin) {
                    batch.push_back(candidate);
                } else {
                    skipped = true;
                    nextWaiting.push_back(candidate);
                }
            }
        }

        Gate inputGate =
            (direction == Direction::Upstream)
            ? Gate::Lower
            : Gate::Upper;

        Gate outputGate =
            (direction == Direction::Upstream)
            ? Gate::Upper
            : Gate::Lower;

        Level entryLevel =
            (direction == Direction::Upstream)
            ? Level::Low
            : Level::High;

        if (lock.level() != entryLevel) {
            lock.pump();
        }

        lock.open(inputGate);

        for (const Vessel& vessel : batch) {
            lock.enter(vessel);
        }

        lock.close(inputGate);

        lock.pump();

        lock.open(outputGate);

        for (const Vessel& vessel : batch) {
            lock.exit(vessel.call_sign);
            report.passed.push_back(vessel.call_sign);
        }

        lock.close(outputGate);

        waiting = nextWaiting;
    }


    report.pump_cycles = lock.pumpCycles();

    return report;
}

} // namespace kitezh
