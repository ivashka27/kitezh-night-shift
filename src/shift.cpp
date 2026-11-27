#include "kitezh/shift.hpp"

#include "kitezh/lock.hpp"
#include "kitezh/geometry.hpp"

#include <stdexcept>

namespace kitezh {

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
    ShiftReport report;
    std::vector<Vessel> waiting;
    std::vector<std::string> seen;

    for (size_t i = 0; i < queue.size(); ++i) {
        Vessel vessel = queue[i];

        bool found = false;
        for (size_t j = 0; j < seen.size(); ++j) {
            if (seen[j] == vessel.call_sign) {
                found = true;
                break;
            }
        }

        if (found) {
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

    Lock lock;

    while (!waiting.empty()) {
        std::vector<Vessel> batch;
        batch.push_back(waiting[0]);
        waiting.erase(waiting.begin());

        Direction dir = batch[0].direction;

        if (batch[0].kind != VesselKind::Oversized) {
            bool skipped = false;
            size_t i = 0;

            while (i < waiting.size()) {
                Vessel next = waiting[i];

                if (next.direction != dir || next.kind == VesselKind::Oversized) {
                    skipped = true;
                    ++i;
                    continue;
                }

                if (!fitsInChamber(batch, next)) {
                    skipped = true;
                    ++i;
                    continue;
                }

                if (skipped && next.kind != VesselKind::Ambulance) {
                    ++i;
                    continue;
                }

                batch.push_back(next);
                waiting.erase(waiting.begin() + i);
            }
        }

        Level in_level = Level::High;
        Gate in_gate = Gate::Upper;
        Gate out_gate = Gate::Lower;

        if (dir == Direction::Upstream) {
            in_level = Level::Low;
            in_gate = Gate::Lower;
            out_gate = Gate::Upper;
        }

        if (lock.level() != in_level) {
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
