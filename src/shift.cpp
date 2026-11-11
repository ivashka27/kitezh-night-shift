#include "kitezh/shift.hpp"

#include "kitezh/lock.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace kitezh {

namespace {

void runBatch(Lock& lock, const std::vector<Vessel>& batch, std::vector<std::string>& passed) {
    Gate entry_gate = Gate::Lower;
    Gate exit_gate = Gate::Upper;
    Level entry_level = Level::Low;
    if (batch[0].direction == Direction::Downstream) {
        entry_gate = Gate::Upper;
        exit_gate = Gate::Lower;
        entry_level = Level::High;
    }

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
        passed.push_back(v.call_sign);
    }
    lock.close(exit_gate);
}

}  // namespace

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
    ShiftReport report;

    std::vector<std::string> seen;
    std::vector<Vessel> waiting;

    for (const Vessel& v : queue) {
        bool is_duplicate = false;
        for (const std::string& sign : seen) {
            if (sign == v.call_sign) {
                is_duplicate = true;
            }
        }

        if (is_duplicate) {
            report.refused.push_back(Refusal{v.call_sign, "duplicate"});
            continue;
        }
        seen.push_back(v.call_sign);

        if (validateVessel(v).has_value()) {
            report.refused.push_back(Refusal{v.call_sign, "invalid"});
        } else if (v.draft_dm > kLowDepthDm) {
            report.refused.push_back(Refusal{v.call_sign, "draft"});
        } else {
            waiting.push_back(v);
        }
    }

    Lock lock;
    while (!waiting.empty()) {
        std::vector<Vessel> batch;
        std::vector<Vessel> rest;

        const Vessel& base = waiting[0];
        batch.push_back(base);

        if (base.kind == VesselKind::Oversized) {
            for (std::size_t i = 1; i < waiting.size(); ++i) {
                rest.push_back(waiting[i]);
            }
        } else {
            bool skipped = false;
            for (std::size_t i = 1; i < waiting.size(); ++i) {
                const Vessel& v = waiting[i];
                bool take = v.direction == base.direction
                    && v.kind != VesselKind::Oversized
                    && fitsInChamber(batch, v)
                    && (!skipped || v.kind == VesselKind::Ambulance);
                if (take) {
                    batch.push_back(v);
                } else {
                    skipped = true;
                    rest.push_back(v);
                }
            }
        }

        runBatch(lock, batch, report.passed);
        waiting = rest;
    }

    report.pump_cycles = lock.pumpCycles();
    return report;
}

}  // namespace kitezh
