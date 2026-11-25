
#include "kitezh/lock.hpp"

#include <algorithm>

namespace kitezh {

namespace {

int waterDepth(Level level) {
    return level == Level::Low ? kLowDepthDm : kHighDepthDm;
}

}  // namespace

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
    GateState& requested = gate == Gate::Lower ? lower_gate_ : upper_gate_;
    GateState other = gate == Gate::Lower ? upper_gate_ : lower_gate_;

    if (requested == GateState::Open) {
        return;
    }

    if (other == GateState::Open) {
        throw LockError("another gate is open");
    }

    if (gate == Gate::Lower && level_ != Level::Low) {
        throw LockError("lower gate requires low water");
    }

    if (gate == Gate::Upper && level_ != Level::High) {
        throw LockError("upper gate requires high water");
    }

    requested = GateState::Open;
}

void Lock::close(Gate gate) {
    if (gate == Gate::Lower) {
        lower_gate_ = GateState::Closed;
    } else {
        upper_gate_ = GateState::Closed;
    }
}

void Lock::pump() {
    if (lower_gate_ == GateState::Open ||
        upper_gate_ == GateState::Open) {
        throw LockError("both gates must be closed");
    }

    Level newLevel =
        level_ == Level::Low ? Level::High : Level::Low;

    for (const Vessel& vessel : inside_) {
        if (vessel.draft_dm > waterDepth(newLevel)) {
            throw LockError("vessel draft exceeds water depth");
        }
    }

    level_ = newLevel;
    ++pump_cycles_;
}

void Lock::enter(const Vessel& vessel) {
    if (validateVessel(vessel).has_value()) {
        throw LockError("invalid vessel");
    }

    for (const Vessel& current : inside_) {
        if (current.call_sign == vessel.call_sign) {
            throw LockError("duplicate call sign");
        }
    }

    if (vessel.draft_dm > waterDepth(level_)) {
        throw LockError("vessel draft exceeds water depth");
    }

    if (vessel.direction == Direction::Upstream &&
        lower_gate_ != GateState::Open) {
        throw LockError("lower gate must be open for upstream vessel");
    }

    if (vessel.direction == Direction::Downstream &&
        upper_gate_ != GateState::Open) {
        throw LockError("upper gate must be open for downstream vessel");
    }

    if (vessel.kind == VesselKind::Oversized) {
        if (!inside_.empty()) {
            throw LockError("oversized vessel must enter alone");
        }
    } else {
        for (const Vessel& current : inside_) {
            if (current.kind == VesselKind::Oversized) {
                throw LockError("oversized vessel occupies the chamber");
            }
        }
    }

    if (!fitsInChamber(inside_, vessel)) {
        throw LockError("vessels do not fit in chamber");
    }

    inside_.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {
    if (inside_.empty()) {
        throw LockError("chamber is empty");
    }

    const Vessel& first = inside_.front();

    if (first.call_sign != call_sign) {
        throw LockError("vessels must exit in FIFO order");
    }

    if (first.direction == Direction::Upstream &&
        upper_gate_ != GateState::Open) {
        throw LockError("upper gate must be open for upstream vessel");
    }

    if (first.direction == Direction::Downstream &&
        lower_gate_ != GateState::Open) {
        throw LockError("lower gate must be open for downstream vessel");
    }

    inside_.erase(inside_.begin());
}

}  // namespace kitezh
