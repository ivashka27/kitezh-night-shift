#include "kitezh/shift.hpp"

#include "kitezh/lock.hpp"

#include <stdexcept>

namespace kitezh {

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
    ShiftReport report;
    Lock lock;

    std::vector<std::string> seen;
    std::vector<Vessel> waiting;

    for (const Vessel& vessel : queue) {
        bool duplicate = false;

        for (const std::string& name : seen) {
            if (name == vessel.call_sign) {
                duplicate = true;
                break;
            }
        }

        if (duplicate) {
            report.refused.push_back({vessel.call_sign, "duplicate"});
            continue;
        }

        seen.push_back(vessel.call_sign);

        if (validateVessel(vessel).has_value()) {
            report.refused.push_back({vessel.call_sign, "invalid"});
            continue;
        }

        if (vessel.draft_dm > kLowDepthDm) {
            report.refused.push_back({vessel.call_sign, "draft"});
            continue;
        }

        waiting.push_back(vessel);
    }

    while (!waiting.empty()) {
        std::vector<Vessel> batch;
        std::vector<Vessel> remaining;

        batch.push_back(waiting[0]);

        Direction direction = waiting[0].direction;
        bool skipped = false;

        for (std::size_t i = 1; i < waiting.size(); ++i) {
            const Vessel& vessel = waiting[i];

            bool canJoin =
                batch[0].kind != VesselKind::Oversized &&
                vessel.kind != VesselKind::Oversized &&
                vessel.direction == direction &&
                fitsInChamber(batch, vessel) &&
                (!skipped || vessel.kind == VesselKind::Ambulance);

            if (canJoin) {
                batch.push_back(vessel);
            } else {
                skipped = true;
                remaining.push_back(vessel);
            }
        }

        waiting = remaining;

        Gate entrance;
        Gate departure;
        Level entranceLevel;

        if (direction == Direction::Upstream) {
            entrance = Gate::Lower;
            departure = Gate::Upper;
            entranceLevel = Level::Low;
        } else {
            entrance = Gate::Upper;
            departure = Gate::Lower;
            entranceLevel = Level::High;
        }

        if (lock.level() != entranceLevel) {
            lock.pump();
        }

        lock.open(entrance);

        for (const Vessel& vessel : batch) {
            lock.enter(vessel);
        }

        lock.close(entrance);

        lock.pump();

        lock.open(departure);

        for (const Vessel& vessel : batch) {
            lock.exit(vessel.call_sign);
            report.passed.push_back(vessel.call_sign);
        }

        lock.close(departure);
    }

    report.pump_cycles = lock.pumpCycles();

    return report;
}

}  // namespace kitezh
