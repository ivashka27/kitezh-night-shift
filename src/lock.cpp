#include "kitezh/lock.hpp"

#include <stdexcept>

namespace kitezh {

Lock::Lock() {
    current_level_ = Level::Low;
    lower_gate_ = GateState::Closed;
    upper_gate_ = GateState::Closed;
    pump_cycles_ = 0;
}

Level Lock::level() const {
    return current_level_;
}

GateState Lock::gate(Gate gate) const {
    if (gate == Gate::Lower) {
        return lower_gate_;
    }
    return upper_gate_;
}

int Lock::pumpCycles() const {
    return pump_cycles_;
}

std::vector<Vessel> Lock::inside() const {
    return vessels_inside_;
}

void Lock::open(Gate gate) {
    if (gate == Gate::Lower) {
        if (lower_gate_ == GateState::Open) {
            return;
        }
        if (current_level_ != Level::Low || upper_gate_ == GateState::Open) {
            throw LockError("Cannot open lower gate");
        }
        lower_gate_ = GateState::Open;
    } else {
        if (upper_gate_ == GateState::Open) {
            return;
        }
        if (current_level_ != Level::High || lower_gate_ == GateState::Open) {
            throw LockError("Cannot open upper gate");
        }
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
        throw LockError("Both gates must be closed to pump");
    }

    Level target_level = Level::High;
    int target_depth = kHighDepthDm;
    if (current_level_ == Level::High) {
        target_level = Level::Low;
        target_depth = kLowDepthDm;
    }

    for (size_t i = 0; i < vessels_inside_.size(); ++i) {
        if (vessels_inside_[i].draft_dm > target_depth) {
            throw LockError("Draft is too large for the new level");
        }
    }

    current_level_ = target_level;
    pump_cycles_ += 1;
}

void Lock::enter(const Vessel& vessel) {
    if (validateVessel(vessel).has_value()) {
        throw LockError("Vessel is invalid");
    }

    for (size_t i = 0; i < vessels_inside_.size(); ++i) {
        if (vessels_inside_[i].call_sign == vessel.call_sign) {
            throw LockError("Call sign is already inside");
        }
    }

    int max_depth = (current_level_ == Level::Low) ? kLowDepthDm : kHighDepthDm;
    if (vessel.draft_dm > max_depth) {
        throw LockError("Vessel draft exceeds current depth");
    }

    if (vessel.direction == Direction::Upstream) {
        if (lower_gate_ != GateState::Open) {
            throw LockError("Lower gate must be open to enter upstream");
        }
    } else {
        if (upper_gate_ != GateState::Open) {
            throw LockError("Upper gate must be open to enter downstream");
        }
    }

    if (vessel.kind == VesselKind::Oversized && !vessels_inside_.empty()) {
        throw LockError("Oversized vessel can only enter an empty chamber");
    }
    if (!vessels_inside_.empty() && vessels_inside_[0].kind == VesselKind::Oversized) {
        throw LockError("Cannot enter when an oversized vessel is inside");
    }

    if (!fitsInChamber(vessels_inside_, vessel)) {
        throw LockError("Not enough space in chamber");
    }

    vessels_inside_.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {
    if (vessels_inside_.empty()) {
        throw LockError("Chamber is empty");
    }

    if (vessels_inside_[0].call_sign != call_sign) {
        throw LockError("Only the first vessel can exit");
    }

    if (vessels_inside_[0].direction == Direction::Upstream) {
        if (upper_gate_ != GateState::Open) {
            throw LockError("Upper gate must be open to exit upstream");
        }
    } else {
        if (lower_gate_ != GateState::Open) {
            throw LockError("Lower gate must be open to exit downstream");
        }
    }

    vessels_inside_.erase(vessels_inside_.begin());
}

}  // namespace kitezh