#include "kitezh/lock.hpp"

namespace kitezh {

Lock::Lock() = default;

Level Lock::level() const {
    return level_;
}

GateState Lock::gate(Gate gate) const {
    return gate == Gate::Lower ? lower_gate_ : upper_gate_;
}

int Lock::pumpCycles() const {
    return pump_cycles_;
}

std::vector<Vessel> Lock::inside() const {
    return inside_;
}

void Lock::open(Gate gate) {
    const Gate other = gate == Gate::Lower ? Gate::Upper : Gate::Lower;
    const Level required = gate == Gate::Lower ? Level::Low : Level::High;
    if (level_ != required || this->gate(other) == GateState::Open) {
        throw LockError("Cannot open gate at this level or with the other gate open");
    }
    if (gate == Gate::Lower) {
        lower_gate_ = GateState::Open;
    } else {
        upper_gate_ = GateState::Open;
    }
}

void Lock::close(Gate gate) {
    if (gate == Gate::Lower) {
        lower_gate_ = GateState::Closed;
    } else {
        upper_gate_ = GateState::Closed;
    }
}

void Lock::pump() {
    if (lower_gate_ == GateState::Open || upper_gate_ == GateState::Open) {
        throw LockError("Close both gates before pumping");
    }
    const Level next = level_ == Level::Low ? Level::High : Level::Low;
    const int depth = next == Level::Low ? kLowDepthDm : kHighDepthDm;
    for (const Vessel& vessel : inside_) {
        if (vessel.draft_dm > depth) {
            throw LockError("Not enough depth at the destination level");
        }
    }
    level_ = next;
    ++pump_cycles_;
}

void Lock::enter(const Vessel& vessel) {
    if (validateVessel(vessel)) {
        throw LockError("Invalid vessel");
    }
    const int depth = level_ == Level::Low ? kLowDepthDm : kHighDepthDm;
    if (vessel.draft_dm > depth) {
        throw LockError("Not enough depth to enter");
    }
    const Gate entrance = vessel.direction == Direction::Upstream ? Gate::Lower : Gate::Upper;
    if (gate(entrance) != GateState::Open) {
        throw LockError("Entrance gate is closed");
    }
    if (vessel.kind == VesselKind::Oversized && !inside_.empty()) {
        throw LockError("Oversized vessel needs an empty chamber");
    }
    for (const Vessel& current : inside_) {
        if (current.call_sign == vessel.call_sign) {
            throw LockError("Call sign is already inside");
        }
        if (current.kind == VesselKind::Oversized) {
            throw LockError("Oversized vessel cannot share the chamber");
        }
    }
    if (!fitsInChamber(inside_, vessel)) {
        throw LockError("Vessel does not fit in the chamber");
    }
    inside_.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {
    if (inside_.empty() || inside_.front().call_sign != call_sign) {
        throw LockError("Only the first vessel may exit");
    }
    const Gate departure = inside_.front().direction == Direction::Upstream ? Gate::Upper : Gate::Lower;
    if (gate(departure) != GateState::Open) {
        throw LockError("Exit gate is closed");
    }
    inside_.erase(inside_.begin());
}

}  // namespace kitezh
