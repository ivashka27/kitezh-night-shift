#include "kitezh/shift.hpp"
#include "kitezh/lock.hpp"

#include <stdexcept>

namespace kitezh {

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
    ShiftReport report;
    std::vector<std::string> seen_names;
    std::vector<Vessel> waiting;

    for (size_t i = 0; i < queue.size(); ++i) {
        const Vessel& v = queue[i];

        bool is_duplicate = false;
        if (!v.call_sign.empty()) {
            for (size_t j = 0; j < seen_names.size(); ++j) {
                if (seen_names[j] == v.call_sign) {
                    is_duplicate = true;
                    break;
                }
            }
            if (!is_duplicate) {
                seen_names.push_back(v.call_sign);
            }
        }

        if (is_duplicate) {
            report.refused.push_back(Refusal{v.call_sign, "duplicate"});
            continue;
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

    while (!waiting.empty()) {
        std::vector<Vessel> batch;
        std::vector<Vessel> remaining;

        Vessel first = waiting[0];
        batch.push_back(first);

        if (first.kind == VesselKind::Oversized) {
            for (size_t i = 1; i < waiting.size(); ++i) {
                remaining.push_back(waiting[i]);
            }
        } else {
            bool skipped = false;
            for (size_t i = 1; i < waiting.size(); ++i) {
                const Vessel& next = waiting[i];

                bool same_direction = (next.direction == first.direction);
                bool not_oversized = (next.kind != VesselKind::Oversized);
                bool can_join = (!skipped || next.kind == VesselKind::Ambulance);

                if (same_direction && not_oversized && can_join && fitsInChamber(batch, next)) {
                    batch.push_back(next);
                } else {
                    skipped = true;
                    remaining.push_back(next);
                }
            }
        }

        waiting = remaining;

        Level needed_level = (first.direction == Direction::Upstream) ? Level::Low : Level::High;
        Gate in_gate = (first.direction == Direction::Upstream) ? Gate::Lower : Gate::Upper;
        Gate out_gate = (first.direction == Direction::Upstream) ? Gate::Upper : Gate::Lower;

        if (lock.level() != needed_level) {
            lock.pump();
        }

        lock.open(in_gate);
        for (size_t i = 0; i < batch.size(); ++i) {
            lock.enter(batch[i]);
        }
        lock.close(in_gate);

        lock.pump();

        lock.open(out_gate);
        for (size_t i = 0; i < batch.size(); ++i) {
            lock.exit(batch[i].call_sign);
            report.passed.push_back(batch[i].call_sign);
        }
        lock.close(out_gate);
    }

    report.pump_cycles = lock.pumpCycles();
    return report;
}

}  // namespace kitezh