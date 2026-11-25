#include "kitezh/lock.hpp"
#include "kitezh/vessel.hpp"
#include <stdexcept>

namespace kitezh {

Lock::Lock() {
    level_ = Level::Low;
    lower_gate_ = GateState::Closed;
    upper_gate_ = GateState::Closed;
    pump_cycles_ = 0;
}

Level Lock::level() const {
    return level_;
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
    return inside_;
}

void Lock::open(Gate gate) {
    if (gate == Gate::Lower) {
        if (lower_gate_ == GateState::Open) {
            return;
        }
        if (upper_gate_ == GateState::Open) {
            throw LockError("error");
        }
        if (level_ != Level::Low) {
            throw LockError("error");
        }
        lower_gate_ = GateState::Open;
    } else {
        if (upper_gate_ == GateState::Open) {
            return;
        }
        if (lower_gate_ == GateState::Open) {
            throw LockError("error");
        }
        if (level_ != Level::High) {
            throw LockError("error");
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
    if (lower_gate_ != GateState::Closed || upper_gate_ != GateState::Closed) {
        throw LockError("error");
    }

    Level new_level;
    int new_depth;

    if (level_ == Level::Low) {
        new_level = Level::High;
        new_depth = kHighDepthDm;
    } else {
        new_level = Level::Low;
        new_depth = kLowDepthDm;
    }

    for (int i = 0; i < inside_.size(); i++) {
        if (inside_[i].draft_dm > new_depth) {
            throw LockError("error");
        }
    }

    level_ = new_level;
    pump_cycles_ = pump_cycles_ + 1;
}

void Lock::enter(const Vessel& vessel) {
    if (validateVessel(vessel).has_value()) {
        throw LockError("error");
    }

    for (int i = 0; i < inside_.size(); i++) {
        if (inside_[i].call_sign == vessel.call_sign) {
            throw LockError("error");
        }
    }

    int current_depth;
    if (level_ == Level::Low) {
        current_depth = kLowDepthDm;
    } else {
        current_depth = kHighDepthDm;
    }
    
    if (vessel.draft_dm > current_depth) {
        throw LockError("error");
    }

    if (vessel.direction == Direction::Upstream) {
        if (lower_gate_ != GateState::Open) {
            throw LockError("error");
        }
    } else {
        if (upper_gate_ != GateState::Open) {
            throw LockError("error");
        }
    }

    if (vessel.kind == VesselKind::Oversized && inside_.size() > 0) {
        throw LockError("error");
    }

    for (int i = 0; i < inside_.size(); i++) {
        if (inside_[i].kind == VesselKind::Oversized) {
            throw LockError("error");
        }
    }

    if (fitsInChamber(inside_, vessel) == false) {
        throw LockError("error");
    }

    inside_.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {
    if (inside_.size() == 0) {
        throw LockError("error");
    }

    if (inside_[0].call_sign != call_sign) {
        throw LockError("error");
    }

    if (inside_[0].direction == Direction::Upstream) {
        if (upper_gate_ != GateState::Open) {
            throw LockError("error");
        }
    } else {
        if (lower_gate_ != GateState::Open) {
            throw LockError("error");
        }
    }

    inside_.erase(inside_.begin());
}

}  // namespace kitezh
