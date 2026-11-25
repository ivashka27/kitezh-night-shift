#include "kitezh/shift.hpp"

#include "kitezh/lock.hpp"

#include <algorithm>

namespace kitezh {

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
    ShiftReport report;
    std::vector<std::string> seen;
    std::vector<Vessel> waiting;

    for (const Vessel& vessel : queue) {
        if (std::find(seen.begin(), seen.end(), vessel.call_sign) != seen.end()) {
            report.refused.push_back({vessel.call_sign, "duplicate"});
            continue;
        }
        // Первое появление занимает позывной даже при последующем отказе.
        seen.push_back(vessel.call_sign);
        if (validateVessel(vessel)) {
            report.refused.push_back({vessel.call_sign, "invalid"});
        } else if (vessel.draft_dm > kLowDepthDm) {
            report.refused.push_back({vessel.call_sign, "draft"});
        } else {
            waiting.push_back(vessel);
        }
    }

    Lock lock;
    while (!waiting.empty()) {
        std::vector<Vessel> batch{waiting.front()};
        std::vector<Vessel> remaining;
        const Direction direction = batch.front().direction;
        bool skipped = false;

        for (std::size_t index = 1; index < waiting.size(); ++index) {
            const Vessel& vessel = waiting[index];
            const bool can_join = batch.front().kind != VesselKind::Oversized
                && vessel.direction == direction
                && vessel.kind != VesselKind::Oversized
                && fitsInChamber(batch, vessel)
                && (!skipped || vessel.kind == VesselKind::Ambulance);
            if (can_join) {
                batch.push_back(vessel);
            } else {
                // После любого пропуска вклиниться может только скорая.
                skipped = true;
                remaining.push_back(vessel);
            }
        }
        waiting = remaining;

        const bool upstream = direction == Direction::Upstream;
        const Level entrance_level = upstream ? Level::Low : Level::High;
        const Gate entrance = upstream ? Gate::Lower : Gate::Upper;
        const Gate departure = upstream ? Gate::Upper : Gate::Lower;
        if (lock.level() != entrance_level) {
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
