#include "kitezh/lock.hpp"

#include <stdexcept>

namespace kitezh {

Lock::Lock() {}

Level Lock::level() const {
    return level_;
}

GateState Lock::gate(Gate g) const {
   return (g == Gate::Lower) ? lower_gate_ : upper_gate_;
}

int Lock::pumpCycles() const {
    return pump_cycles_;
}

std::vector<Vessel> Lock::inside() const {
    return inside_;
}

void Lock::open(Gate g) {
    if (g == Gate::Lower) {
        if (lower_gate_ == GateState::Open) {
            return;
        }
        if (level_ != Level::Low || upper_gate_ == GateState::Open) {
            throw LockError("gate error");
        }
        lower_gate_ = GateState::Open;
    } else {
        if (upper_gate_ == GateState::Open) {
            return;
        }
        if (level_ != Level::High || lower_gate_ == GateState::Open) {
            throw LockError("gate error");
        }
        upper_gate_ = GateState::Open;
    }
}

void Lock::close(Gate g) {
    if (g == Gate::Lower) {
        lower_gate_ = GateState::Closed;
    } else {
        upper_gate_ = GateState::Closed;
    }
}

void Lock::pump() {
    if (lower_gate_ == GateState::Open || upper_gate_ == GateState::Open) {
        throw LockError("pump error");
    }

    Level next_level = (level_ == Level::Low) ? Level::High : Level::Low;
    int next_depth = (next_level == Level::Low) ? kLowDepthDm : kHighDepthDm;

    for (const auto& v : inside_) {
        if (v.draft_dm > next_depth) {
            throw LockError("vessel draft exceeds new level depth");
        }
    }
    level_ = next_level;
    pump_cycles_++;
}

void Lock::enter(const Vessel& vessel) {
    if (validateVessel(vessel).has_value()) {
        throw LockError("Invalid vessel");
    }

    for (const auto& v : inside_) {
        if (v.call_sign == vessel.call_sign) {
            throw LockError("duplicate call sign inside lock");
        }
    }

    int current_depth = (level_ == Level::Low) ? kLowDepthDm : kHighDepthDm;
    if (vessel.draft_dm > current_depth) {
        throw LockError("vessel draft exceeds current depth");
    }

    if (vessel.direction == Direction::Upstream && lower_gate_ != GateState::Open) {
        throw LockError("lower gate must be open for upstream entry");
    }
    if (vessel.direction == Direction::Downstream && upper_gate_ != GateState::Open) {
        throw LockError("upper gate must be open for downstream entry");
    }

    if (vessel.kind == VesselKind::Oversized && !inside_.empty()) {
        throw LockError("oversized vessel requires an empty chamber");
    }
    if (!inside_.empty() && inside_.front().kind == VesselKind::Oversized) {
        throw LockError("cannot enter chamber occupied by an oversized vessel");
    }

    if (!fitsInChamber(inside_, vessel)) {
        throw LockError("vessel does not fit in chamber");
    }

    inside_.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {
    if (inside_.empty() || inside_.front().call_sign != call_sign) {
        throw LockError("vessel is not at the front of the chamber queue");
    }

    const Vessel& head = inside_.front();

    if (head.direction == Direction::Upstream && upper_gate_ != GateState::Open) {
        throw LockError("upper gate must be open for upstream exit");
    }
    if (head.direction == Direction::Downstream && lower_gate_ != GateState::Open) {
        throw LockError("lower gate must be open for downstream exit");
    }

    inside_.erase(inside_.begin());
}

}  // namespace kitezh
