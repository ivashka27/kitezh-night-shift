#include "kitezh/shift.hpp"

#include "kitezh/lock.hpp"

#include <stdexcept>

namespace kitezh {
    namespace {

       void Passing(std::vector<Vessel>& party,std::vector<std::string>& passed, Lock& lock) {
           Vessel first_ship = party[0];
           Direction dir = first_ship.direction;
           Level entry_level;
           Gate entry_gate;
           if (dir == Direction::Upstream) {
               entry_level = Level::Low;
               entry_gate = Gate::Lower;
           }
           else {
               entry_level = Level::High;
               entry_gate = Gate::Upper;
           }
           Gate exit_gate = (entry_gate == Gate::Lower)
            ? Gate::Upper
            : Gate::Lower;
           if (lock.level()!=entry_level) {
               lock.pump();
           }
           lock.open(entry_gate);
           for (int i = 0; i < party.size(); ++i) {
               lock.enter(party[i]);
           }
           lock.close(entry_gate);
           lock.pump();
           lock.open(exit_gate);
           for (int i = 0; i < party.size(); ++i) {
               lock.exit(party[i].call_sign);
               passed.push_back(party[i].call_sign);
           }
           lock.close(exit_gate);
       }

    } // namespace

ShiftReport runNightShift(const std::vector<Vessel>& q) {
    std::vector<Refusal> refused= {};
    std::vector<std::string> passed= {};
    std::vector<std::string> already_seen = {};
    Lock lock;
    int pump_cycles = 0;
    int n = q.size();
    std::vector<Vessel> maybe_in_party= {};
    for (int i = 0; i < n; ++i) {
        bool do_not = false;
        std::string name = q[i].call_sign;
        std::optional<std::string> valid = validateVessel(q[i]);
        int draft = q[i].draft_dm;
        for (int ii = 0; ii<already_seen.size(); ++ii) {
            if (already_seen[ii]==name) {
                refused.push_back(Refusal{name,"duplicate"});
                do_not = true;
                break;
            }
        }
        if (do_not) {
            continue;
        }
        already_seen.push_back(name);
        if (valid != std::nullopt) {
            do_not = true;
            refused.push_back(Refusal{name,"invalid"});
        }
        if (do_not) {
            continue;
        }
        if (draft > kLowDepthDm) {
            do_not = true;
            refused.push_back(Refusal{name,"draft"});
        }
        if (do_not) {
            continue;
        }
        if (do_not == false) maybe_in_party.push_back(q[i]);

    }
    while (maybe_in_party.size()>0) {
        std::vector<Vessel> party = {};
        Vessel first_ship = maybe_in_party[0];
        maybe_in_party.erase(maybe_in_party.begin());
        party.push_back(first_ship);
        if (first_ship.kind == VesselKind::Oversized) {
            Passing(party,passed,lock);
            continue;
        }
        int skipped = 0;
        for (int i = 0; i < maybe_in_party.size(); ++i) {
            bool skipped_on_this = false;
            bool do_not = false;
            Direction dir = maybe_in_party[i].direction;
            VesselKind kind = maybe_in_party[i].kind;
            bool fits = fitsInChamber(party,maybe_in_party[i]);
            if (dir != first_ship.direction) {
                do_not = true;
                skipped++;
            }
            else if (kind == VesselKind::Oversized) {
                do_not = true;
                skipped++;
            }
            else if (fits == false){
                do_not = true;
                skipped++;
            }
            else if (skipped > 0) {
                do_not = true;
                skipped++;
                skipped_on_this = true;
                if (kind == VesselKind::Ambulance and skipped_on_this == true) {
                    do_not = false;
                    skipped--;
                }
            }
            if (do_not==false) {
                party.push_back(maybe_in_party[i]);
                maybe_in_party.erase(maybe_in_party.begin()+i);
                --i;
            }

        }
    Passing(party,passed,lock);
    }
    pump_cycles = lock.pumpCycles();
    return ShiftReport{passed, refused, pump_cycles};
}

}  // namespace kitezh
