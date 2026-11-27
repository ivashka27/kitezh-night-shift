#include "kitezh/shift.hpp"
#include "kitezh/lock.hpp"
#include "kitezh/vessel.hpp"
#include <vector>
#include <string>
#include <optional>

namespace kitezh {

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
    ShiftReport report;

    std::vector<Vessel> valid_queue;
    std::vector<std::string> seen;

    for (const auto& v : queue) {
        bool is_dup = false;
        for (const auto& s : seen) {
            if (s == v.call_sign) {
                is_dup = true;
                break;
            }
        }

        if (is_dup) {
            report.refused.push_back({v.call_sign, "duplicate"});
        } else if (validateVessel(v).has_value()) {
            report.refused.push_back({v.call_sign, "invalid"});
            seen.push_back(v.call_sign);
        } else if (v.draft_dm > 28) {
            report.refused.push_back({v.call_sign, "draft"});
            seen.push_back(v.call_sign);
        } else {
            seen.push_back(v.call_sign);
            valid_queue.push_back(v);
        }
    }

    Lock lock;
    
    while (!valid_queue.empty()) {
        const Vessel base = valid_queue[0];
        Direction current_dir = base.direction;
        
        std::vector<Vessel> batch;
        batch.push_back(base);
        bool skipped = false;

        if (base.kind != VesselKind::Oversized) {
            for (size_t j = 1; j < valid_queue.size(); ++j) {
                const Vessel& next = valid_queue[j];

                if (next.direction != current_dir) {
                    skipped = true;
                    continue;
                }
                if (next.kind == VesselKind::Oversized) {
                    skipped = true;
                    continue;
                }

                bool fits = true;
                int current_len = 0;
                for (const auto& b : batch) current_len += b.length_m;
                current_len += (batch.size() - 1); 

                int next_len = current_len + 1 + next.length_m;
                if (next_len > 36) fits = false;

                if (fits) {
                    if (skipped && next.kind != VesselKind::Ambulance) {
                        skipped = true; 
                        continue;
                    }
                    batch.push_back(next);
                } else {
                    skipped = true;
                }
            }
        }

        for (auto it = valid_queue.begin(); it != valid_queue.end(); ) {
            bool found = false;
            for (const auto& b : batch) {
                if (it->call_sign == b.call_sign) {
                    found = true;
                    break;
                }
            }
            if (found) {
                it = valid_queue.erase(it);
            } else {
                ++it;
            }
        }

        Level target_entry_level = (current_dir == Direction::Upstream) ? Level::Low : Level::High;
        if (lock.level() != target_entry_level) {
            lock.pump();
        }

        Gate in_gate = (current_dir == Direction::Upstream) ? Gate::Lower : Gate::Upper;
        lock.open(in_gate);
        for (const auto& b : batch) {
            lock.enter(b);
        }
        lock.close(in_gate);

        lock.pump();

        Gate out_gate = (current_dir == Direction::Upstream) ? Gate::Upper : Gate::Lower;
        lock.open(out_gate);
        for (const auto& b : batch) {
            lock.exit(b.call_sign);
            report.passed.push_back(b.call_sign);
        }
        lock.close(out_gate);
    }

    report.pump_cycles = lock.pumpCycles();
    return report;
}

}  // namespace kitezh