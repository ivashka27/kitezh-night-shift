#include "kitezh/shift.hpp"

#include "kitezh/lock.hpp"

#include <stdexcept>

namespace kitezh {

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
    // 1. Слева направо вычеркнуть duplicate, затем invalid, затем draft.
    // Вычеркнутые не стоят в очереди и не рвут партию.
    // 2. Пока кто-то ждёт, собрать одну партию по правилам README.
    // 3. Провести её объектом Lock. Следующую начинать с закрытых затворов.
    // 4. pump_cycles взять из lock.pumpCycles() в конце, не из отдельного счётчика.
    ShiftReport report;
    std::vector<std::string> seen;
    std::vector<Vessel> waiting;

    for (const Vessel& v : queue) {
        bool duplicate = false;
        for (const std::string& cs : seen) {
            if (cs == v.call_sign) {
                duplicate = true;
                break;
            }
        }
        if (!v.call_sign.empty() && duplicate) {
            report.refused.push_back(Refusal{v.call_sign, "duplicate"});
            continue;
        }
        if (!v.call_sign.empty()) {
            seen.push_back(v.call_sign);
        }

        if (validateVessel(v).has_value()) {
            report.refused.push_back(Refusal{v.call_sign, "invalid"});
            continue;
        }

        if (v.draft_dm > kLowDepthDm) {
            report.refused.push_back(Refusal{v.call_sign, "draft"});
            continue;
        }

        waiting.push_back(v);
    }

    Lock lock;
    std::vector<bool> taken(waiting.size(), false);

    while (true) {
        std::size_t first_idx = waiting.size();
        for (std::size_t i = 0; i < waiting.size(); ++i) {
            if (!taken[i]) {
                first_idx = i;
                break;
            }
        }
        if (first_idx == waiting.size()) {
            break;
        }

        std::vector<Vessel> batch;
        batch.push_back(waiting[first_idx]);
        taken[first_idx] = true;

        const Direction dir = waiting[first_idx].direction;
        if (waiting[first_idx].kind != VesselKind::Oversized) {
            bool can_take_regular = true;
            for (std::size_t i = first_idx + 1; i < waiting.size(); ++i) {
                if (taken[i]) {
                    continue;
                }
                const Vessel& candidate = waiting[i];
                if (candidate.direction != dir) {
                    can_take_regular = false;
                    continue;
                }
                if (candidate.kind == VesselKind::Oversized) {
                    can_take_regular = false;
                    continue;
                }
                if (candidate.kind == VesselKind::Ambulance) {
                    if (fitsInChamber(batch, candidate)) {
                        batch.push_back(candidate);
                        taken[i] = true;
                    } else {
                        can_take_regular = false;
                    }
                } else {
                    if (can_take_regular && fitsInChamber(batch, candidate)) {
                        batch.push_back(candidate);
                        taken[i] = true;
                    } else {
                        can_take_regular = false;
                    }
                }
            }
        }

        const Level entry_level = (dir == Direction::Upstream) ? Level::Low : Level::High;
        const Gate entry_gate = (dir == Direction::Upstream) ? Gate::Lower : Gate::Upper;
        const Gate exit_gate = (dir == Direction::Upstream) ? Gate::Upper : Gate::Lower;

        if (lock.level() != entry_level) {
            lock.pump();
        }
        lock.open(entry_gate);
        for (const Vessel& v : batch) {
            lock.enter(v);
        }
        lock.close(entry_gate);
        lock.pump();
        lock.open(exit_gate);
        for (const Vessel& v : batch) {
            lock.exit(v.call_sign);
            report.passed.push_back(v.call_sign);
        }
        lock.close(exit_gate);
    }

    report.pump_cycles = lock.pumpCycles();
    return report;
}

} // namespace kitezh
