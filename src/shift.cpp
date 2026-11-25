
#include "kitezh/shift.hpp"

#include "kitezh/lock.hpp"

#include <unordered_set>
#include <vector>

namespace kitezh {

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
    ShiftReport report;
    Lock lock;

    // Проверяем очередь и отделяем суда, которые не могут пройти.
    std::vector<Vessel> remaining;
    std::unordered_set<std::string> seen;

    for (const Vessel& vessel : queue) {
        if (!seen.insert(vessel.call_sign).second) {
            report.refused.push_back({vessel.call_sign, "duplicate"});
        } else if (validateVessel(vessel).has_value()) {
            report.refused.push_back({vessel.call_sign, "invalid"});
        } else if (vessel.draft_dm > kLowDepthDm) {
            report.refused.push_back({vessel.call_sign, "draft"});
        } else {
            remaining.push_back(vessel);
        }
    }

    // Формируем партии, сохраняя пропущенные суда для следующих партий.
    while (!remaining.empty()) {
        std::vector<Vessel> batch;
        std::vector<Vessel> leftovers;

        const Vessel& first = remaining.front();
        batch.push_back(first);

        bool skipped = false;

        for (std::size_t i = 1; i < remaining.size(); ++i) {
            const Vessel& candidate = remaining[i];

            bool canJoin = false;

            if (first.kind != VesselKind::Oversized) {
                canJoin =
                    candidate.direction == first.direction &&
                    candidate.kind != VesselKind::Oversized &&
                    fitsInChamber(batch, candidate) &&
                    (!skipped ||
                     candidate.kind == VesselKind::Ambulance);
            }

            if (canJoin) {
                batch.push_back(candidate);
            } else {
                skipped = true;
                leftovers.push_back(candidate);
            }
        }

        // Определяем ворота и уровень входа.
        Gate entryGate =
            first.direction == Direction::Upstream
                ? Gate::Lower
                : Gate::Upper;

        Gate exitGate =
            first.direction == Direction::Upstream
                ? Gate::Upper
                : Gate::Lower;

        Level entryLevel =
            first.direction == Direction::Upstream
                ? Level::Low
                : Level::High;

        // Переходим на уровень входа.
        if (lock.level() != entryLevel) {
            lock.pump();
        }

        // Вводим суда в камеру.
        lock.open(entryGate);

        for (const Vessel& vessel : batch) {
            lock.enter(vessel);
        }

        lock.close(entryGate);

        // Перекачиваем воду и выпускаем суда.
        lock.pump();
        lock.open(exitGate);

        for (const Vessel& vessel : batch) {
            lock.exit(vessel.call_sign);
            report.passed.push_back(vessel.call_sign);
        }

        lock.close(exitGate);

        // Пропущенные суда переходят в очередь следующей партии.
        remaining = leftovers;
    }

    report.pump_cycles = lock.pumpCycles();

    return report;
}

}  // namespace kitezh
