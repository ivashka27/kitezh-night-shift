#include "kitezh/shift.hpp"

#include "kitezh/lock.hpp"

namespace kitezh {

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
    ShiftReport report;
    Lock lock;
    std::vector<std::string> seen;
    std::vector<Vessel> waiting;

    for (const Vessel& vessel : queue) {
        bool duplicate = false;
        for (const std::string& sign : seen) {
            if (sign == vessel.call_sign) {
                duplicate = true;
            }
        }

        if (duplicate) {
            report.refused.push_back({vessel.call_sign, "duplicate"});
            continue;
        }

        seen.push_back(vessel.call_sign);

        if (validateVessel(vessel).has_value()) {
            report.refused.push_back({vessel.call_sign, "invalid"});
        } else if (vessel.draft_dm > kLowDepthDm) {
            report.refused.push_back({vessel.call_sign, "draft"});
        } else {
            waiting.push_back(vessel);
        }
    }

    while (!waiting.empty()) {
        std::vector<Vessel> batch;
        batch.push_back(waiting.front());
        waiting.erase(waiting.begin());

        Direction direction = batch.front().direction;
        bool skipped = false;

        if (batch.front().kind != VesselKind::Oversized) {
            std::size_t index = 0;
            while (index < waiting.size()) {
                const Vessel& vessel = waiting[index];
                bool canEnter = true;

                if (vessel.direction != direction) {
                    canEnter = false;
                }
                if (vessel.kind == VesselKind::Oversized) {
                    canEnter = false;
                }
                if (!fitsInChamber(batch, vessel)) {
                    canEnter = false;
                }
                if (skipped && vessel.kind != VesselKind::Ambulance) {
                    canEnter = false;
                }

                if (canEnter) {
                    batch.push_back(vessel);
                    waiting.erase(waiting.begin() + index);
                } else {
                    skipped = true;
                    index = index + 1;
                }
            }
        }

        Gate entryGate;
        Gate exitGate;
        Level entryLevel;
        if (direction == Direction::Upstream) {
            entryGate = Gate::Lower;
            exitGate = Gate::Upper;
            entryLevel = Level::Low;
        } else {
            entryGate = Gate::Upper;
            exitGate = Gate::Lower;
            entryLevel = Level::High;
        }

        if (lock.level() != entryLevel) {
            lock.pump();
        }

        lock.open(entryGate);
        for (const Vessel& vessel : batch) {
            lock.enter(vessel);
        }
        lock.close(entryGate);

        lock.pump();

        lock.open(exitGate);
        for (const Vessel& vessel : batch) {
            lock.exit(vessel.call_sign);
            report.passed.push_back(vessel.call_sign);
        }
        lock.close(exitGate);
    }

    report.pump_cycles = lock.pumpCycles();
    return report;
}

}  // namespace kitezh
