#include "kitezh/shift.hpp"
#include "kitezh/lock.hpp"
#include "kitezh/geometry.hpp"

#include <unordered_set>

namespace kitezh {

    ShiftReport runNightShift(const std::vector<Vessel>& queue) {
        ShiftReport report;
        std::vector<Vessel> valid_queue;
        std::unordered_set<std::string> seen_call_signs;

        for (const auto& v : queue) {
            if (seen_call_signs.count(v.call_sign) > 0) {
                report.refused.push_back({ v.call_sign, "duplicate" });
                continue;
            }
            seen_call_signs.insert(v.call_sign);

            if (validateVessel(v).has_value()) {
                report.refused.push_back({ v.call_sign, "invalid" });
                continue;
            }

            if (v.draft_dm > kLowDepthDm) {
                report.refused.push_back({ v.call_sign, "draft" });
                continue;
            }

            valid_queue.push_back(v);
        }

        Lock lock;
        std::vector<bool> processed(valid_queue.size(), false);
        size_t remaining = valid_queue.size();

        while (remaining > 0) {
            size_t first_idx = 0;
            while (processed[first_idx]) {
                ++first_idx;
            }

            std::vector<Vessel> batch;
            batch.push_back(valid_queue[first_idx]);
            processed[first_idx] = true;
            --remaining;

            Direction batch_dir = valid_queue[first_idx].direction;

            if (valid_queue[first_idx].kind != VesselKind::Oversized) {
                bool skipped = false;
                for (size_t i = first_idx + 1; i < valid_queue.size(); ++i) {
                    if (processed[i]) {
                        continue;
                    }
                    const Vessel& candidate = valid_queue[i];
                    bool is_emergency = (candidate.kind == VesselKind::Ambulance);

                    if (candidate.direction == batch_dir &&
                        candidate.kind != VesselKind::Oversized &&
                        fitsInChamber(batch, candidate) &&
                        (!skipped || is_emergency)) {
                        batch.push_back(candidate);
                        processed[i] = true;
                        --remaining;
                    }
                    else {
                        skipped = true;
                    }
                }
            }

            Level entry_level = (batch_dir == Direction::Upstream) ? Level::Low : Level::High;
            Gate entry_gate = (batch_dir == Direction::Upstream) ? Gate::Lower : Gate::Upper;
            Gate exit_gate = (batch_dir == Direction::Upstream) ? Gate::Upper : Gate::Lower;

            if (lock.level() != entry_level) {
                lock.pump();
            }

            lock.open(entry_gate);
            for (const auto& v : batch) {
                lock.enter(v);
            }
            lock.close(entry_gate);

            lock.pump();

            lock.open(exit_gate);
            for (const auto& v : batch) {
                lock.exit(v.call_sign);
                report.passed.push_back(v.call_sign);
            }
            lock.close(exit_gate);
        }

        report.pump_cycles = lock.pumpCycles();
        return report;
    }

} // namespace kitezh
