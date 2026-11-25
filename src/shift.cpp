#include "kitezh/shift.hpp"
#include "kitezh/lock.hpp"

#include <unordered_set>
#include <utility>

namespace kitezh {

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
    ShiftReport report;
    std::unordered_set<std::string> seen;
    std::vector<Vessel> waiting;

    // Сначала убираем повторы, ошибочные записи и суда с большой осадкой.
    for (const auto& vessel : queue) {
        if (!seen.insert(vessel.call_sign).second) {
            report.refused.push_back({vessel.call_sign, "duplicate"});
        } else if (validateVessel(vessel)) {
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
        bool skipped = false;

        // Первое судно задаёт направление партии.
        for (std::size_t i = 1; i < waiting.size(); ++i) {
            const Vessel& candidate = waiting[i];

            if (batch.front().kind != VesselKind::Oversized &&
                candidate.direction == batch.front().direction &&
                candidate.kind != VesselKind::Oversized &&
                fitsInChamber(batch, candidate) &&
                (!skipped || candidate.kind == VesselKind::Ambulance)) {
                batch.push_back(candidate);
            } else {
                skipped = true;
                remaining.push_back(candidate);
            }
        }

        waiting = std::move(remaining);

        const bool upstream = batch.front().direction == Direction::Upstream;
        const Level entrance_level = upstream ? Level::Low : Level::High;
        const Gate entrance = upstream ? Gate::Lower : Gate::Upper;
        const Gate exit_gate = upstream ? Gate::Upper : Gate::Lower;

        if (lock.level() != entrance_level) {
            lock.pump();
        }

        lock.open(entrance);
        for (const auto& vessel : batch) {
            lock.enter(vessel);
        }
        lock.close(entrance);

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