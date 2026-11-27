#include "kitezh/lock.hpp"
#include <algorithm>
#include <limits>

namespace kitezh {

Lock::Lock() = default;

Level Lock::level() const {
    return current_level_;
}

GateState Lock::gate(Gate gate) const {
    if (gate == Gate::Lower) return lower_gate_state_;
    return upper_gate_state_;
}

int Lock::pumpCycles() const {
    return pump_cycles_count_;
}

std::vector<Vessel> Lock::inside() const {
    return inside_vessels_;
}

static bool canOpenGate(Level level, Gate gate) {
    if (gate == Gate::Lower) return level == Level::Low;
    if (gate == Gate::Upper) return level == Level::High;
    return false;
}

void Lock::open(Gate gate) {
    if (gate == Gate::Lower && lower_gate_state_ == GateState::Open) return;
    if (gate == Gate::Upper && upper_gate_state_ == GateState::Open) return;

    if (!canOpenGate(current_level_, gate)) {
        throw LockError("Cannot open gate at this level");
    }

    if (gate == Gate::Lower && upper_gate_state_ == GateState::Open) throw LockError("Upper gate is open");
    if (gate == Gate::Upper && lower_gate_state_ == GateState::Open) throw LockError("Lower gate is open");

    if (gate == Gate::Lower) lower_gate_state_ = GateState::Open;
    else upper_gate_state_ = GateState::Open;
}

void Lock::close(Gate gate) {
    if (gate == Gate::Lower && lower_gate_state_ == GateState::Closed) return;
    if (gate == Gate::Upper && upper_gate_state_ == GateState::Closed) return;

    if (gate == Gate::Lower) lower_gate_state_ = GateState::Closed;
    else upper_gate_state_ = GateState::Closed;
}

void Lock::pump() {
    if (lower_gate_state_ == GateState::Open || upper_gate_state_ == GateState::Open) {
        throw LockError("Cannot pump with open gates");
    }

    Level new_level = (current_level_ == Level::Low) ? Level::High : Level::Low;
    int new_depth = (new_level == Level::High) ? 42 : 28;

    for (const auto& v : inside_vessels_) {
        if (v.draft_dm > new_depth) {
            throw LockError("Draft exceeds new depth");
        }
    }

    current_level_ = new_level;
    ++pump_cycles_count_;
}

void Lock::enter(const Vessel& vessel) {
    if (validateVessel(vessel).has_value()) throw LockError("Invalid vessel");

    for (const auto& v : inside_vessels_) {
        if (v.call_sign == vessel.call_sign) throw LockError("Duplicate call sign");
    }

    int current_depth = (current_level_ == Level::Low) ? 28 : 42;
    if (vessel.draft_dm > current_depth) throw LockError("Draft too big for current level");

    Gate expected_gate = (vessel.direction == Direction::Upstream) ? Gate::Lower : Gate::Upper;
    if (gate(expected_gate) != GateState::Open) throw LockError("Input gate is closed");

    bool is_oversized = (vessel.length_m > 36) || (vessel.kind == VesselKind::Oversized);
    
    bool has_oversized_inside = false;
    for (const auto& v : inside_vessels_) {
        if (v.length_m > 36 || v.kind == VesselKind::Oversized) {
            has_oversized_inside = true;
            break;
        }
    }
    
    if (is_oversized && !inside_vessels_.empty()) {
        throw LockError("Oversized only alone");
    }
    
    if (has_oversized_inside) {
        throw LockError("Cannot enter with oversized inside");
    }

    if (!fitsInChamber(inside_vessels_, vessel)) throw LockError("Does not fit");

    inside_vessels_.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {
    if (inside_vessels_.empty()) throw LockError("Chamber is empty");
    if (inside_vessels_[0].call_sign != call_sign) throw LockError("Not the head of the queue");

    Vessel vessel = inside_vessels_[0];
    Gate expected_gate = (vessel.direction == Direction::Upstream) ? Gate::Upper : Gate::Lower;
    if (gate(expected_gate) != GateState::Open) throw LockError("Output gate is closed");

    inside_vessels_.erase(inside_vessels_.begin());
}

}  // namespace kitezh