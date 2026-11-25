#include "kitezh/lock.hpp"

#include <stdexcept>

namespace kitezh {

Lock::Lock():
    level_(Level::Low),
      lower_gate_state_(GateState::Closed),
      upper_gate_state_(GateState::Closed),
      pump_cycles_(0) {

}

Level Lock::level() const {
    return level_;
}

GateState Lock::gate(Gate gate) const {
    if (gate == Gate::Lower) {
        return lower_gate_state_;
    }
    else {
        return upper_gate_state_;
    }
}

int Lock::pumpCycles() const {
    return pump_cycles_;
}

std::vector<Vessel> Lock::inside() const {
    return inside_;
}

void Lock::open(Gate gate) {
    if (gate == Gate::Lower) {
        if (level_ != Level::Low) {
            throw LockError("iddqd");
        }
        if (upper_gate_state_ != GateState::Closed) {
            throw LockError("aezakmi");
        }
        lower_gate_state_ = GateState::Open;
    } else {
        if (level_ != Level::High) {
            throw LockError("err");
        }

        if (lower_gate_state_ != GateState::Closed) {
            throw LockError("bbbb");
        }
        upper_gate_state_ = GateState::Open;
    }


}

void Lock::close(Gate gate) {
    if (gate == Gate::Lower) {
        lower_gate_state_ = GateState::Closed;
    } else {
        upper_gate_state_ = GateState::Closed;
    }
}

void Lock::pump() {
    if ((lower_gate_state_ == GateState::Open )or  (upper_gate_state_ == GateState::Open)) {
        throw LockError("bbbb");
    }
    int n = inside_.size();
    Level opposite_level = (level_ == Level::Low) ? Level::High : Level::Low;
    if (opposite_level==Level::High) {
        for (int i = 0; i<n; ++i) {
            if (inside_[i].draft_dm>kHighDepthDm) {
                throw LockError("bbbb");
            }
        }
    }
    if (opposite_level==Level::Low) {
        for (int i = 0; i<n; ++i) {
            if (inside_[i].draft_dm>kLowDepthDm) {
                throw LockError("bbbb");
            }
        }
    }
    level_ = opposite_level;
    ++pump_cycles_;
}

void Lock::enter(const Vessel& vessel) {
    if (validateVessel(vessel)) throw LockError("err");
    int n = inside_.size();
    for (int i = 0; i<n; ++i) {
        if (inside_[i].call_sign == vessel.call_sign) {
            throw LockError("bbbb");
        }
    }
    if (vessel.draft_dm > ((level_ == Level::Low) ? kLowDepthDm : kHighDepthDm)) {
        throw LockError("err");
    }
    if (vessel.direction == Direction::Upstream and lower_gate_state_ == GateState::Closed) {
        throw LockError("err");
    }
    if (vessel.direction == Direction::Downstream and upper_gate_state_ == GateState::Closed) {
        throw LockError("err");
    }
    if (vessel.kind == VesselKind::Oversized and n>0) {
        throw LockError("err");
    }
    for (int i = 0; i<n; ++i) {
        if (inside_[i].kind==VesselKind::Oversized) {
            throw LockError("bbbb");
        }
    }
    if (!fitsInChamber(inside_,vessel)) {
        throw LockError("bbbb");
    }
    inside_.push_back(vessel);
}

void Lock::exit(const std::string& name) {
    int n = inside_.size();
    if (n<=0)  throw LockError("bbbb");
    if (name != inside_[0].call_sign) throw LockError("яусталочень");
    if (inside_[0].direction==Direction::Upstream and upper_gate_state_ == GateState::Closed) {
        throw LockError("bbbb");
    }
    if (inside_[0].direction==Direction::Downstream and lower_gate_state_ == GateState::Closed) {
        throw LockError("bbbb");
    }
    inside_.erase(inside_.begin());
}

}  // namespace kitezh
