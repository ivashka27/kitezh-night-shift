#include "kitezh/shift.hpp"

#include "kitezh/lock.hpp"

#include <stdexcept>

#include <vector>
#include <string>

namespace kitezh {

bool containsCallSign(const std::vector<std::string>& list, const std::string& name) {
    for (size_t i = 0; i < list.size(); i++) {
        if (list[i] == name) {
            return true;
        }
    }
    return false;
}

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
    ShiftReport report;
    std::vector<std::string> seen_names;
    std::vector<Vessel> valid_ships;

    for (size_t i = 0; i < queue.size(); i++) {
        Vessel ship = queue[i];

        if (containsCallSign(seen_names, ship.call_sign)) {
            report.refused.push_back({ship.call_sign, "duplicate"});
        } else if (validateVessel(ship).has_value()) {
            seen_names.push_back(ship.call_sign);
            report.refused.push_back({ship.call_sign, "invalid"});
        } else if (ship.draft_dm > 28) {
            seen_names.push_back(ship.call_sign);
            report.refused.push_back({ship.call_sign, "draft"});
        } else {
            seen_names.push_back(ship.call_sign);
            valid_ships.push_back(ship);
        }
    }

    Lock lock;

    while (!valid_ships.empty()) {
        std::vector<Vessel> batch;
        std::vector<Vessel> remaining_ships;

        Vessel leader = valid_ships[0];
        batch.push_back(leader);

        Direction target_direction = leader.direction;
        bool has_skipped = false;

        if (leader.kind == VesselKind::Oversized) {
            has_skipped = true;
        }

        for (size_t i = 1; i < valid_ships.size(); i++) {
            Vessel current = valid_ships[i];

            if (leader.kind == VesselKind::Oversized) {
                remaining_ships.push_back(current);
                continue;
            }

            bool same_direction = (current.direction == target_direction);
            bool not_oversized = (current.kind != VesselKind::Oversized);
            bool fits_by_meters = fitsInChamber(batch, current);

            if (same_direction && not_oversized && fits_by_meters) {
                if (!has_skipped || current.kind == VesselKind::Ambulance) {
                    batch.push_back(current);
                    continue;
                }
            }

            has_skipped = true;
            remaining_ships.push_back(current);
        }

        valid_ships = remaining_ships;

        Gate enter_gate;
        Gate exit_gate;
        Level enter_level;

        if (target_direction == Direction::Upstream) {
            enter_gate = Gate::Lower;
            exit_gate = Gate::Upper;
            enter_level = Level::Low;
        } else {
            enter_gate = Gate::Upper;
            exit_gate = Gate::Lower;
            enter_level = Level::High;
        }

        if (lock.level() != enter_level) {
            lock.pump();
        }

        lock.open(enter_gate);
        for (size_t i = 0; i < batch.size(); i++) {
            lock.enter(batch[i]);
        }
        lock.close(enter_gate);

        lock.pump();

        lock.open(exit_gate);
        for (size_t i = 0; i < batch.size(); i++) {
            lock.exit(batch[i].call_sign);
            report.passed.push_back(batch[i].call_sign);
        }
        lock.close(exit_gate);
    }

    report.pump_cycles = lock.pumpCycles();
    return report;
}

} // namespace kitezh