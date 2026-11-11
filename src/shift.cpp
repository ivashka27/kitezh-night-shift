#include "kitezh/shift.hpp"

#include "kitezh/lock.hpp"

#include <algorithm>
#include <cstddef>

namespace kitezh {

namespace {

std::vector<Vessel> screenQueue(const std::vector<Vessel>& queue, std::vector<Refusal>& refused) {
    std::vector<Vessel> waiting;
    std::vector<std::string> seen;
    for (const Vessel& vessel : queue) {
        if (std::find(seen.begin(), seen.end(), vessel.call_sign) != seen.end()) {
            refused.push_back({vessel.call_sign, "duplicate"});
            continue;
        }
        seen.push_back(vessel.call_sign);
        if (validateVessel(vessel)) {
            refused.push_back({vessel.call_sign, "invalid"});
        } else if (vessel.draft_dm > kLowDepthDm) {
            refused.push_back({vessel.call_sign, "draft"});
        } else {
            waiting.push_back(vessel);
        }
    }
    return waiting;
}

std::vector<Vessel> takeBatch(std::vector<Vessel>& waiting) {
    std::vector<Vessel> batch{waiting.front()};
    std::vector<Vessel> rest;
    const Vessel base = waiting.front();
    bool skipped = false;
    for (std::size_t index = 1; index < waiting.size(); ++index) {
        const Vessel& vessel = waiting[index];
        const bool joins = base.kind != VesselKind::Oversized
            && vessel.direction == base.direction
            && vessel.kind != VesselKind::Oversized
            && fitsInChamber(batch, vessel)
            && (!skipped || vessel.kind == VesselKind::Ambulance);
        if (joins) {
            batch.push_back(vessel);
        } else {
            skipped = true;
            rest.push_back(vessel);
        }
    }
    waiting = rest;
    return batch;
}

void passBatch(Lock& lock, const std::vector<Vessel>& batch, std::vector<std::string>& passed) {
    const bool upstream = batch.front().direction == Direction::Upstream;
    const Level entry_level = upstream ? Level::Low : Level::High;
    const Gate entry_gate = upstream ? Gate::Lower : Gate::Upper;
    const Gate exit_gate = upstream ? Gate::Upper : Gate::Lower;

    if (lock.level() != entry_level) {
        lock.pump();
    }
    lock.open(entry_gate);
    for (const Vessel& vessel : batch) {
        lock.enter(vessel);
    }
    lock.close(entry_gate);

    lock.pump();

    lock.open(exit_gate);
    for (const Vessel& vessel : batch) {
        lock.exit(vessel.call_sign);
        passed.push_back(vessel.call_sign);
    }
    lock.close(exit_gate);
}

}  // namespace

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
    ShiftReport report;
    std::vector<Vessel> waiting = screenQueue(queue, report.refused);

    Lock lock;
    while (!waiting.empty()) {
        passBatch(lock, takeBatch(waiting), report.passed);
    }
    report.pump_cycles = lock.pumpCycles();
    return report;
}

}  // namespace kitezh
