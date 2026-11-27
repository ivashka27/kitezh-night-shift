#include "kitezh/lock.hpp"
#include "kitezh/geometry.hpp"

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
    return gate == Gate::Lower ? lower_gate_ : upper_gate_;
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

void Lock::close(Gate gate) {
    if (gate == Gate::Lower) {
        lower_gate_ = GateState::Closed;
        
    } else {
        upper_gate_ = GateState::Closed;
    }
}

void Lock::pump() {
    if (lower_gate_ == GateState::Open || upper_gate_ == GateState::Open) {
        throw LockError("gate open");
    }

    Level next_level = Level::High;
    int next_depth = kHighDepthDm;

    if (level_ == Level::High) {
        next_level = Level::Low;
        next_depth = kLowDepthDm;
    }

    for (size_t i = 0; i < inside_.size(); ++i) {
        if (inside_[i].draft_dm > next_depth) {
            throw LockError("too deep");
        }
    }

    level_ = next_level;
    ++pump_cycles_;
}

void Lock::enter(const Vessel& vessel) {
    std::optional<std::string> err = validateVessel(vessel);

    if (err.has_value()) {
        throw LockError(*err);
    }

    int depth = kLowDepthDm;

    if (level_ == Level::High) {
        depth = kHighDepthDm;
    }

    if (vessel.draft_dm > depth) {
        throw LockError("bad draft for level");
    }

    Gate need_gate = Gate::Upper;
    if (vessel.direction == Direction::Upstream) {
        need_gate = Gate::Lower;
    }

    if (gate(need_gate) != GateState::Open) {
        throw LockError("gate closed");
    }

    for (size_t i = 0; i < inside_.size(); ++i) {
        if (inside_[i].call_sign == vessel.call_sign) {
            throw LockError("same call sign");
        }
    }

    if (vessel.kind == VesselKind::Oversized && !inside_.empty()) {
        throw LockError("oversized alone");
    }
    if (!inside_.empty() && inside_[0].kind == VesselKind::Oversized) {
        throw LockError("oversized inside");
    }

    if (!fitsInChamber(inside_, vessel)) {
        throw LockError("no space");
    }

    inside_.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {

    if (inside_.empty() || inside_[0].call_sign != call_sign) {
        throw LockError("wrong exit order");
    }

    Gate exit_gate = Gate::Lower;

    if (inside_[0].direction == Direction::Upstream) {
        exit_gate = Gate::Upper;
    }

    if (gate(exit_gate) != GateState::Open) {
        throw LockError("exit gate closed");
    }

    inside_.erase(inside_.begin());
}

}  // namespace kitezh
