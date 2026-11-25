#include "kitezh/shift.hpp"
#include "kitezh/lock.hpp"

#include <cstddef>
#include <unordered_set>

namespace kitezh {

namespace {

std::vector<std::size_t> collectBatch(const std::vector<Vessel>& waiting) {
    std::vector<std::size_t> picked{0};
    const Vessel& base = waiting[0];
    if (base.kind == VesselKind::Oversized) {
        return picked;
    }

    std::vector<Vessel> taken{base};
    bool skipped = false;
    for (std::size_t i = 1; i < waiting.size(); ++i) {
        const Vessel& v = waiting[i];
        const bool ok = v.direction == base.direction &&
                        v.kind != VesselKind::Oversized &&
                        fitsInChamber(taken, v) &&
                        (!skipped || v.kind == VesselKind::Ambulance);
        if (ok) {
            picked.push_back(i);
            taken.push_back(v);
        } else {
            skipped = true;
        }
    }
    return picked;
}

void passBatch(Lock& lock, const std::vector<Vessel>& batch, std::vector<std::string>& passed) {
    const bool up = batch.front().direction == Direction::Upstream;
    const Level entryLevel = up ? Level::Low : Level::High;
    const Gate entryGate = up ? Gate::Lower : Gate::Upper;
    const Gate exitGate = up ? Gate::Upper : Gate::Lower;

    if (lock.level() != entryLevel) {
        lock.pump();
    }
    lock.open(entryGate);
    for (const Vessel& v : batch) {
        lock.enter(v);
    }
    lock.close(entryGate);
    lock.pump();
    lock.open(exitGate);
    for (const Vessel& v : batch) {
        lock.exit(v.call_sign);
        passed.push_back(v.call_sign);
    }
    lock.close(exitGate);
}

}  // namespace

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
    ShiftReport report;

    std::vector<Vessel> waiting;
    std::unordered_set<std::string> seen;
    for (const Vessel& v : queue) {
        if (!seen.insert(v.call_sign).second) {
            report.refused.push_back({v.call_sign, "duplicate"});
        } else if (validateVessel(v)) {
            report.refused.push_back({v.call_sign, "invalid"});
        } else if (v.draft_dm > kLowDepthDm) {
            report.refused.push_back({v.call_sign, "draft"});
        } else {
            waiting.push_back(v);
        }
    }

    Lock lock;
    while (!waiting.empty()) {
        const std::vector<std::size_t> idx = collectBatch(waiting);

        std::vector<Vessel> batch;
        std::vector<Vessel> rest;
        std::size_t k = 0;
        for (std::size_t i = 0; i < waiting.size(); ++i) {
            if (k < idx.size() && idx[k] == i) {
                batch.push_back(waiting[i]);
                ++k;
            } else {
                rest.push_back(waiting[i]);
            }
        }
        passBatch(lock, batch, report.passed);
        waiting = std::move(rest);
    }

    report.pump_cycles = lock.pumpCycles();
    return report;
}

}  // namespace kitezh
