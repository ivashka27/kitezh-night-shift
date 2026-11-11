#include "kitezh/lock.hpp"
#include "kitezh/geometry.hpp"

namespace kitezh {

Lock::Lock() = default;

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
            throw LockError("cannot open lower gate");
        }
        lower_gate_ = GateState::Open;
    } else {
        if (upper_gate_ == GateState::Open) {
            return;
        }
        if (level_ != Level::High || lower_gate_ == GateState::Open) {
            throw LockError("cannot open upper gate");
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
        throw LockError("cannot pump with open gates");
    }

    Level next_level = (level_ == Level::Low) ? Level::High : Level::Low;
    int next_depth = (next_level == Level::Low) ? kLowDepthDm : kHighDepthDm;

    for (const auto& v : inside_) {
        if (v.draft_dm > next_depth) {
            throw LockError("vessel draft exceeds water depth at target level");
        }
    }

    level_ = next_level;
    ++pump_cycles_;
}

void Lock::enter(const Vessel& vessel) {
    if (validateVessel(vessel).has_value()) {
        throw LockError("invalid vessel data");
    }

    for (const auto& v : inside_) {
        if (v.call_sign == vessel.call_sign) {
            throw LockError("vessel with this call sign is already inside");
        }
    }

    if (vessel.direction == Direction::Upstream) {
        if (lower_gate_ != GateState::Open) {
            throw LockError("lower gate must be open for upstream entry");
        }
    } else {
        if (upper_gate_ != GateState::Open) {
            throw LockError("upper gate must be open for downstream entry");
        }
    }

    int current_depth = (level_ == Level::Low) ? kLowDepthDm : kHighDepthDm;
    if (vessel.draft_dm > current_depth) {
        throw LockError("vessel draft exceeds current water depth");
    }

    if (vessel.kind == VesselKind::Oversized && !inside_.empty()) {
        throw LockError("oversized vessel can only enter empty chamber");
    }
    for (const auto& v : inside_) {
        if (v.kind == VesselKind::Oversized) {
            throw LockError("chamber already contains an oversized vessel");
        }
    }

    if (!fitsInChamber(inside_, vessel)) {
        throw LockError("vessel does not fit chamber length");
    }

    inside_.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {
    if (inside_.empty()) {
        throw LockError("chamber is empty");
    }

    const auto& head = inside_.front();
    if (head.call_sign != call_sign) {
        throw LockError("only the head vessel can exit");
    }

    if (head.direction == Direction::Upstream) {
        if (upper_gate_ != GateState::Open) {
            throw LockError("upper gate must be open for upstream exit");
        }
    } else {
        if (lower_gate_ != GateState::Open) {
            throw LockError("lower gate must be open for downstream exit");
        }
    }

    inside_.erase(inside_.begin());
}

}  // namespace kitezh