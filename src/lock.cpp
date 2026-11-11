#include "kitezh/lock.hpp"

#include <stdexcept>

namespace kitezh {

Lock::Lock()
    : level_(Level::Low),
      lower_gate_(GateState::Closed),
      upper_gate_(GateState::Closed),
      pump_cycles_(0),
      inside_() {}

Level Lock::level() const {
    return level_;
}

GateState Lock::gate(Gate gate) const {
    return (gate == Gate::Lower) ? lower_gate_ : upper_gate_;
}

Gate Lock::gate_by_direction(Direction dir) const {
    return (dir == Direction::Upstream) ? Gate::Upper : Gate::Lower;   
}

int Lock::pumpCycles() const {
    return pump_cycles_;
}

std::vector<Vessel> Lock::inside() const {
    return inside_;
}

void Lock::open(Gate gate) {
    if (gate == Gate::Lower) {
        if (lower_gate_ == GateState::Open) {
            return;
        }

        if (level_ != Level::Low ||
            upper_gate_ == GateState::Open) {
            throw LockError("Cannot open lower gate");
        }

        lower_gate_ = GateState::Open;
    }
    else {
        if (upper_gate_ == GateState::Open) {
            return;
        }

        if (level_ != Level::High ||
            lower_gate_ == GateState::Open) {
            throw LockError("Cannot open upper gate");
        }

        upper_gate_ = GateState::Open;
    }
}

void Lock::close(Gate gate) {
    if (gate == Gate::Lower) {
        lower_gate_ = GateState::Closed;
    }
    else {
        upper_gate_ = GateState::Closed;
    }
}

void Lock::pump() {
    if (lower_gate_ == GateState::Open ||
        upper_gate_ == GateState::Open) {
        throw LockError("Gates must be closed");
    }

    int next_depth = (level_ == Level::High) ? kLowDepthDm : kHighDepthDm;

    for (const auto& vessel : inside_) {
        if (vessel.draft_dm > next_depth) {
            throw LockError("Depth");
        }
    }

    level_ = (level_ == Level::Low) ? Level::High : Level::Low;
    pump_cycles_++;
}

void Lock::enter(const Vessel& vessel) {
    if (validateVessel(vessel).has_value()) {
        throw LockError("Invalid vessel");
    }

    for (auto& current : inside_) {
        if (current.call_sign == vessel.call_sign) {
            throw LockError("Duplicate call sign");
        }
    }

    int depth = (level_ == Level::Low) ? kLowDepthDm : kHighDepthDm;

    if (vessel.draft_dm > depth) {
        throw LockError("Depth");
    }

    if (gate(gate_by_direction(vessel.direction)) != GateState::Open) {
        throw LockError("Entry gate is closed");
    }

    if (vessel.kind == VesselKind::Oversized) {
        if (!inside_.empty()) {
            throw LockError("Oversized vessel requires empty");
        }
    }

    for (auto& current : inside_) {
        if (current.kind == VesselKind::Oversized) {
            throw LockError("Cannot enter with oversized vessel");
        }
    }

    if (!fitsInChamber(inside_, vessel)) {
        throw LockError("Chamber is full");
    }

    inside_.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {
    if (inside_.empty()) {
        throw LockError("Chamber is empty");
    }

    if (inside_.front().call_sign != call_sign) {
        throw LockError("Wrong vessel");
    }

    if (gate(gate_by_direction(inside_.front().direction)) != GateState::Open) {
        throw LockError("Exit gate is closed");
    }

    inside_.erase(inside_.begin());
}

}  // namespace kitezh
