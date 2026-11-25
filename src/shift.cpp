#include "kitezh/shift.hpp"

#include "kitezh/lock.hpp"

#include <stdexcept>
#include <unordered_set>

namespace kitezh {

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
    // 1. Слева направо вычеркнуть duplicate, затем invalid, затем draft.
    //    Вычеркнутые не стоят в очереди и не рвут партию.
    // 2. Пока кто-то ждёт, собрать одну партию по правилам README.
    // 3. Провести её объектом Lock. Следующую начинать с закрытых затворов.
    // 4. pump_cycles взять из lock.pumpCycles() в конце, не из отдельного счётчика.
    ShiftReport report;
    Lock lock;
    std::unordered_set<std::string> seen;
    std::vector<Vessel> waiting;


    for (const Vessel& vessel : queue) {

        if (seen.count(vessel.call_sign) > 0) {
            report.refused.push_back({
                vessel.call_sign, "duplicate"
            });
            continue;
        }
        seen.insert(vessel.call_sign);
        if (validateVessel(vessel).has_value()) {
            report.refused.push_back({
                vessel.call_sign, "invalid"
            });
            continue;
        }
        if (vessel.draft_dm > kLowDepthDm) {
            report.refused.push_back({
                vessel.call_sign, "draft"
            });
            continue;
        }
        waiting.push_back(vessel);
    }

    while (!waiting.empty()) {
        std::vector<Vessel> batch;
        std::vector<Vessel> remaining;

        batch.push_back(waiting.front());
        Direction direction = batch.front().direction;
        bool skipped = false;
        for (std::size_t i = 1; i < waiting.size(); ++i) {
            const Vessel& candidate = waiting[i];
            bool canAdd = true;

            if (candidate.direction != direction) {
                canAdd = false;
            }
            if (candidate.kind == VesselKind::Oversized) {
                canAdd = false;
            }
            if (batch.front().kind == VesselKind::Oversized) {
                canAdd = false;
            }
            if (!fitsInChamber(batch, candidate)) {
                canAdd = false;
            }
            if (skipped &&
                candidate.kind != VesselKind::Ambulance) {
                canAdd = false;
            }
            if (canAdd) {
                batch.push_back(candidate);
            } else {
                remaining.push_back(candidate);
                skipped = true;
            }
        }

        Gate entrance = Gate::Lower;
        Gate departure = Gate::Upper;
        Level entranceLevel = Level::Low;

        if (direction == Direction::Downstream) {
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

        waiting = remaining;
    }

    report.pump_cycles = lock.pumpCycles();
    return report;
}

}  // namespace kitezh
