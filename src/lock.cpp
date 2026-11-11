#include "kitezh/lock.hpp"

namespace kitezh {

Lock::Lock() = default;

Level Lock::level() const {
    return level_;
}

GateState Lock::gate(Gate a) const {
    if (a == Gate::Lower) {
        return lower_gate_;
    }
    return upper_gate_;
}

int Lock::pumpCycles() const {
    return pump_cycles_;
}

std::vector<Vessel> Lock::inside() const {
    return inside_vessels_;
}

void Lock::open(Gate a) {
    if (a == Gate::Lower) {
        if (lower_gate_ == GateState::Open) {
            return;
        }
        if (level_ != Level::Low) {
            throw LockError("cannot open lower gate: water level is high");
        }
        if (upper_gate_ == GateState::Open) {
            throw LockError("both gates cannot be open");
        }
        lower_gate_ = GateState::Open;
    } else {
        if (upper_gate_ == GateState::Open) {
            return;
        }
        if (level_ != Level::High) {
            throw LockError("cannot open upper gate: water level is low");
        }
        if (lower_gate_ == GateState::Open) {
            throw LockError("both gates cannot be open");
        }
        upper_gate_ = GateState::Open;
    }
}

void Lock::close(Gate a) {
    if (a == Gate::Lower) {
        lower_gate_ = GateState::Closed;
    } else {
        upper_gate_ = GateState::Closed;
    }
}

void Lock::pump() {
    if (lower_gate_ == GateState::Open || upper_gate_ == GateState::Open) {
        throw LockError("cannot pump with opened gates");
    }

    Level target_level = (level_ == Level::Low) ? Level::High : Level::Low;
    int target_depth = (target_level == Level::Low) ? kLowDepthDm : kHighDepthDm;

    for (const auto& ship : inside_vessels_) {
        if (ship.draft_dm > target_depth) {
            throw LockError("vessel will run aground");
        }
    }

    level_ = target_level;
    pump_cycles_++;
}

void Lock::enter(const Vessel& vessel) {
    if (validateVessel(vessel).has_value()) {
        throw LockError("invalid vessel data");
    }

    int cur_depth = (level_ == Level::Low) ? kLowDepthDm : kHighDepthDm;
    if (vessel.draft_dm > cur_depth) {
        throw LockError("draft exceeds chamber depth");
    }

    if (vessel.direction == Direction::Upstream && lower_gate_ != GateState::Open) {
        throw LockError("lower gate is closed");
    }
    if (vessel.direction == Direction::Downstream && upper_gate_ != GateState::Open) {
        throw LockError("upper gate is closed");
    }

    for (const auto& ship : inside_vessels_) {
        if (ship.call_sign == vessel.call_sign) {
            throw LockError("duplicate call sign in lock");
        }
        if (ship.kind == VesselKind::Oversized) {
            throw LockError("cannot share lock with oversized vessel");
        }
    }

    if (vessel.kind == VesselKind::Oversized && !inside_vessels_.empty()) {
        throw LockError("oversized vessel requires empty lock");
    }

    if (!fitsInChamber(inside_vessels_, vessel)) {
        throw LockError("no space left in chamber");
    }

    inside_vessels_.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {
    if (inside_vessels_.empty()) {
        throw LockError("lock is empty");
    }

    const Vessel& first = inside_vessels_.front();
    if (first.call_sign != call_sign) {
        throw LockError("vessel is not at the head of queue");
    }

    if (first.direction == Direction::Upstream && upper_gate_ != GateState::Open) {
        throw LockError("upper exit gate is closed");
    }
    if (first.direction == Direction::Downstream && lower_gate_ != GateState::Open) {
        throw LockError("lower exit gate is closed");
    }

    inside_vessels_.erase(inside_vessels_.begin());
}

} // namespace kitezh