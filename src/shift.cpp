#include "kitezh/shift.hpp"

#include "kitezh/lock.hpp"

#include <stdexcept>
#include <vector>
#include <algorithm>
#include <string>

namespace kitezh {
    ShiftReport runNightShift(const std::vector<Vessel> &queue) {
        ShiftReport report;

        std::vector<Vessel> accepted;
        std::vector<std::string> seen_call_signs;


        for (const Vessel &vessel: queue) {
            const bool duplicate =
                    std::find(seen_call_signs.begin(),
                              seen_call_signs.end(),
                              vessel.call_sign) != seen_call_signs.end();

            if (duplicate) {
                report.refused.push_back({vessel.call_sign, "duplicate"});
                continue;
            }

            seen_call_signs.push_back(vessel.call_sign);

            if (validateVessel(vessel).has_value()) {
                report.refused.push_back({vessel.call_sign, "invalid"});
                continue;
            }

            if (vessel.draft_dm > 28) {
                report.refused.push_back({vessel.call_sign, "draft"});
                continue;
            }

            accepted.push_back(vessel);
        }


        Lock lock;
        std::vector<Vessel> pending = accepted;

        while (!pending.empty()) {
            std::vector<Vessel> batch;
            const Direction direction = pending.front().direction;

            batch.push_back(pending.front());
            pending.erase(pending.begin());

            if (batch.front().kind != VesselKind::Oversized) {
                std::vector<bool> selected(pending.size(), false);


                for (std::size_t i = 0; i < pending.size(); ++i) {
                    const Vessel &candidate = pending[i];

                    if (candidate.direction != direction) {
                        continue;
                    }

                    if (candidate.kind != VesselKind::Ambulance) {
                        bool waiting_ambulance = false;

                        for (std::size_t j = 0; j < pending.size(); ++j) {
                            if (!selected[j] &&
                                pending[j].direction == direction &&
                                pending[j].kind == VesselKind::Ambulance) {
                                waiting_ambulance = true;
                                break;
                                }
                        }

                        if (waiting_ambulance) {
                            continue;
                        }
                    }

                    if (candidate.kind == VesselKind::Oversized) {
                        continue;
                    }

                    if (fitsInChamber(batch, candidate)) {
                        batch.push_back(candidate);
                        selected[i] = true;
                    }
                }


                std::vector<Vessel> remaining;

                for (std::size_t i = 0; i < pending.size(); ++i) {
                    if (!selected[i]) {
                        remaining.push_back(pending[i]);
                    }
                }

                pending = remaining;
            }


            const Level entry_level =
                    direction == Direction::Upstream ? Level::Low : Level::High;

            if (lock.level() != entry_level) {
                lock.pump();
            }

            const Gate entry_gate =
                    direction == Direction::Upstream ? Gate::Lower : Gate::Upper;

            const Gate exit_gate =
                    direction == Direction::Upstream ? Gate::Upper : Gate::Lower;

            lock.open(entry_gate);

            for (const Vessel &vessel: batch) {
                lock.enter(vessel);
            }

            lock.close(entry_gate);


            lock.pump();
            lock.open(exit_gate);

            for (const Vessel &vessel: batch) {
                lock.exit(vessel.call_sign);
                report.passed.push_back(vessel.call_sign);
            }

            lock.close(exit_gate);
        }

        report.pump_cycles = lock.pumpCycles();

        return report;
    }
}


