
#include "kitezh/lock.hpp"

namespace kitezh {

Lock::Lock() {
}

Level Lock::level() const {
    return level_;
}

GateState Lock::gate(Gate gate) const {
    if (gate == Gate::Lower) {
        return lower_;
    }
    return upper_;
}

int Lock::pumpCycles() const {
    return cycles_;
}

std::vector<Vessel> Lock::inside() const {
    return vessels_;
}

void Lock::open(Gate gate) {
    if (gate == Gate::Lower) {
        if (lower_ == GateState::Open) {
            return;
        }

        if (level_ != Level::Low || upper_ == GateState::Open) {
            throw LockError("Cannot open lower gate");
        }

        lower_ = GateState::Open;
    } else {
        if (upper_ == GateState::Open) {
            return;
        }

        if (level_ != Level::High || lower_ == GateState::Open) {
            throw LockError("Cannot open upper gate");
        }

        upper_ = GateState::Open;
    }
}

void Lock::close(Gate gate) {
    if (gate == Gate::Lower) {
        lower_ = GateState::Closed;
    } else {
        upper_ = GateState::Closed;
    }
}

void Lock::pump() {
    if (lower_ == GateState::Open || upper_ == GateState::Open) {
        throw LockError("Gates are open");
    }

    Level next_level;

    if (level_ == Level::Low) {
        next_level = Level::High;
    } else {
        next_level = Level::Low;
    }

    int depth = kLowDepthDm;

    if (next_level == Level::High) {
        depth = kHighDepthDm;
    }

    for (const Vessel& v : vessels_) {
        if (v.draft_dm > depth) {
            throw LockError("Not enough depth");
        }
    }

    level_ = next_level;
    cycles_++;
}

void Lock::enter(const Vessel& vessel) {
    if (validateVessel(vessel).has_value()) {
        throw LockError("Invalid vessel");
    }

    for (const Vessel& v : vessels_) {
        if (v.call_sign == vessel.call_sign) {
            throw LockError("Duplicate call sign");
        }
    }

    int depth = kLowDepthDm;

    if (level_ == Level::High) {
        depth = kHighDepthDm;
    }

    if (vessel.draft_dm > depth) {
        throw LockError("Not enough depth");
    }

    if (vessel.direction == Direction::Upstream) {
        if (lower_ != GateState::Open) {
            throw LockError("Lower gate is closed");
        }
    } else {
        if (upper_ != GateState::Open) {
            throw LockError("Upper gate is closed");
        }
    }

    if (vessel.kind == VesselKind::Oversized && !vessels_.empty()) {
        throw LockError("Oversized vessel");
    }

    for (const Vessel& v : vessels_) {
        if (v.kind == VesselKind::Oversized) {
            throw LockError("Oversized vessel inside");
        }
    }

    if (!fitsInChamber(vessels_, vessel)) {
        throw LockError("Not enough space");
    }

    vessels_.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {
    if (vessels_.empty()) {
        throw LockError("Chamber is empty");
    }

    if (vessels_[0].call_sign != call_sign) {
        throw LockError("Wrong vessel");
    }

    if (vessels_[0].direction == Direction::Upstream) {
        if (upper_ != GateState::Open) {
            throw LockError("Upper gate is closed");
        }
    } else {
        if (lower_ != GateState::Open) {
            throw LockError("Lower gate is closed");
        }
    }

    vessels_.erase(vessels_.begin());
}

} // namespace kitezh
