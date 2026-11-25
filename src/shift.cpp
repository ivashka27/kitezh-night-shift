#include "kitezh/shift.hpp"
#include "kitezh/lock.hpp"
#include <stdexcept>
#include "kitezh/vessel.hpp"
#include <vector>
#include <string>

namespace kitezh {

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
    ShiftReport report;
    std::vector<Vessel> waiting;
    std::vector<std::string> seen;

    for (int i = 0; i < queue.size(); i++) {
        Vessel v = queue[i];    
        
        bool is_dup = false;
        for (int j = 0; j < seen.size(); j++) {
            if (seen[j] == v.call_sign) {
                is_dup = true;
                break;
            }
        }

        if (is_dup == true) {
            report.refused.push_back({v.call_sign, "duplicate"});
            continue;
        }
        
        seen.push_back(v.call_sign);

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

    Lock lock;

    while (waiting.size() > 0) {
        Vessel first = waiting[0];
        waiting.erase(waiting.begin());

        std::vector<Vessel> batch;
        batch.push_back(first);

        if (first.kind != VesselKind::Oversized) {
            bool skipped_someone = false;
            int i = 0;
            
            while (i < waiting.size()) {
                Vessel cand = waiting[i];
                bool can_take = false;

                if (cand.direction == first.direction && cand.kind != VesselKind::Oversized) {
                    if (fitsInChamber(batch, cand) == true) {
                        if (skipped_someone == false) {
                            can_take = true;
                        } else if (cand.kind == VesselKind::Ambulance) {
                            can_take = true;
                        }
                    }
                }

                if (can_take == true) {
                    batch.push_back(cand);
                    waiting.erase(waiting.begin() + i);
                } else {
                    skipped_someone = true;
                    i++;
                }
            }
        }

        if (first.direction == Direction::Upstream) {
            if (lock.level() != Level::Low) {
                lock.pump();
            }
            lock.open(Gate::Lower);
            
            for (int i = 0; i < batch.size(); i++) {
                lock.enter(batch[i]);
            }
            lock.close(Gate::Lower);

            lock.pump();
            lock.open(Gate::Upper);
            
            for (int i = 0; i < batch.size(); i++) {
                lock.exit(batch[i].call_sign);
                report.passed.push_back(batch[i].call_sign);
            }
            lock.close(Gate::Upper);
            
        } else {
            if (lock.level() != Level::High) {
                lock.pump();
            }
            lock.open(Gate::Upper);
            
            for (int i = 0; i < batch.size(); i++) {
                lock.enter(batch[i]);
            }
            lock.close(Gate::Upper);

            lock.pump();
            lock.open(Gate::Lower);
            
            for (int i = 0; i < batch.size(); i++) {
                lock.exit(batch[i].call_sign);
                report.passed.push_back(batch[i].call_sign);
            }
            lock.close(Gate::Lower);
        }
    }

    report.pump_cycles = lock.pumpCycles();
    return report;
}

}