
#include "kitezh/lock.hpp"

#include "kitezh/geometry.hpp"

namespace kitezh {

void Lock::close(Gate gate_value) {
    if (gate_value == Gate::Lower) {
        lower_gate_ = GateState::Closed;
        return;
    }

    if (gate_value == Gate::Upper) {
        upper_gate_ = GateState::Closed;
        return;
    }

    throw LockError("unknown gate");
}

void Lock::enter(const Vessel& vessel) {
    if (validateVessel(vessel).has_value()) {
        throw LockError("invalid vessel");
    }

    for (const Vessel& current : inside_) {
        if (current.call_sign == vessel.call_sign) {
            throw LockError("duplicate call sign");
        }

        if (current.kind == VesselKind::Oversized) {
            throw LockError("oversized vessel occupies the chamber alone");
        }
    }

    if (vessel.kind == VesselKind::Oversized && !inside_.empty()) {
        throw LockError("oversized vessel occupies the chamber alone");
    }

    const int current_depth = level_ == Level::Low
        ? kLowDepthDm
        : kHighDepthDm;

    if (vessel.draft_dm > current_depth) {
        throw LockError("vessel draft exceeds current depth");
    }

    Gate entrance_gate;

    switch (vessel.direction) {
        case Direction::Upstream:
            entrance_gate = Gate::Lower;
            break;
        case Direction::Downstream:
            entrance_gate = Gate::Upper;
            break;
        default:
            throw LockError("unknown vessel direction");
    }

    if (gate(entrance_gate) != GateState::Open) {
        throw LockError("entrance gate is closed");
    }

    if (!fitsInChamber(inside_, vessel)) {
        throw LockError("vessel does not fit in the chamber");
    }

    inside_.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {
    if (inside_.empty()) {
        throw LockError("the chamber is empty");
    }

    const Vessel& first_vessel = inside_.front();

    if (first_vessel.call_sign != call_sign) {
        throw LockError("vessels must leave in entry order");
    }

    Gate exit_gate;

    switch (first_vessel.direction) {
        case Direction::Upstream:
            exit_gate = Gate::Upper;
            break;
        case Direction::Downstream:
            exit_gate = Gate::Lower;
            break;
        default:
            throw LockError("unknown vessel direction");
    }

    if (gate(exit_gate) != GateState::Open) {
        throw LockError("exit gate is closed");
    }

    inside_.erase(inside_.begin());
}

GateState Lock::gate(Gate gate_value) const {
    switch (gate_value) {
        case Gate::Lower:
            return lower_gate_;
        case Gate::Upper:
            return upper_gate_;
    }

    throw LockError("unknown gate");
}

std::vector<Vessel> Lock::inside() const {
    return inside_;
}

Level Lock::level() const {
    return level_;
}

Lock::Lock()
    : inside_(),
      level_(Level::Low),
      lower_gate_(GateState::Closed),
      pump_cycles_(0),
      upper_gate_(GateState::Closed) {
}

void Lock::open(Gate gate_value) {
    if (gate_value == Gate::Lower) {
        if (lower_gate_ == GateState::Open) {
            return;
        }

        if (level_ != Level::Low || upper_gate_ == GateState::Open) {
            throw LockError("lower gate cannot be opened");
        }

        lower_gate_ = GateState::Open;
        return;
    }

    if (gate_value == Gate::Upper) {
        if (upper_gate_ == GateState::Open) {
            return;
        }

        if (level_ != Level::High || lower_gate_ == GateState::Open) {
            throw LockError("upper gate cannot be opened");
        }

        upper_gate_ = GateState::Open;
        return;
    }

    throw LockError("unknown gate");
}

void Lock::pump() {
    if (lower_gate_ == GateState::Open || upper_gate_ == GateState::Open) {
        throw LockError("pump requires closed gates");
    }

    const int destination_depth = level_ == Level::Low
        ? kHighDepthDm
        : kLowDepthDm;

    for (const Vessel& vessel : inside_) {
        if (vessel.draft_dm > destination_depth) {
            throw LockError("vessel draft exceeds destination depth");
        }
    }

    level_ = level_ == Level::Low ? Level::High : Level::Low;
    ++pump_cycles_;
}

int Lock::pumpCycles() const {
    return pump_cycles_;
}

}  // namespace kitezh
