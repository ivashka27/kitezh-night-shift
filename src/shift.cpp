
#include "kitezh/shift.hpp"
#include "kitezh/lock.hpp"

namespace kitezh {

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
    ShiftReport report;
    Lock lock;

    std::vector<Vessel> waiting;
    std::vector<std::string> names;

    for (const Vessel& v : queue) {
        bool duplicate = false;

        for (const std::string& name : names) {
            if (name == v.call_sign) {
                duplicate = true;
                break;
            }
        }

        if (duplicate) {
            report.refused.push_back({v.call_sign, "duplicate"});
            continue;
        }

        names.push_back(v.call_sign);

        if (validateVessel(v).has_value()) {
            report.refused.push_back({v.call_sign, "invalid"});
            continue;
        }

        if (v.draft_dm > kLowDepthDm) {
            report.refused.push_back({v.call_sign, "draft"});
            continue;
        }

        waiting.push_back(v);
    }

    while (!waiting.empty()) {
        std::vector<Vessel> batch;
        std::vector<Vessel> remaining;

        batch.push_back(waiting[0]);

        Direction dir = waiting[0].direction;
        bool skipped = false;

        for (int i = 1; i < static_cast<int>(waiting.size()); i++) {
            const Vessel& v = waiting[i];

            bool can_add = true;

            if (batch[0].kind == VesselKind::Oversized) {
                can_add = false;
            }

            if (v.kind == VesselKind::Oversized) {
                can_add = false;
            }

            if (v.direction != dir) {
                can_add = false;
            }

            if (!fitsInChamber(batch, v)) {
                can_add = false;
            }

            if (skipped && v.kind != VesselKind::Ambulance) {
                can_add = false;
            }

            if (can_add) {
                batch.push_back(v);
            } else {
                skipped = true;
                remaining.push_back(v);
            }
        }

        waiting = remaining;

        Gate entrance = Gate::Lower;
        Gate exit_gate = Gate::Upper;
        Level start_level = Level::Low;

        if (dir == Direction::Downstream) {
            entrance = Gate::Upper;
            exit_gate = Gate::Lower;
            start_level = Level::High;
        }

        if (lock.level() != start_level) {
            lock.pump();
        }

        lock.open(entrance);

        for (const Vessel& v : batch) {
            lock.enter(v);
        }

        lock.close(entrance);

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
