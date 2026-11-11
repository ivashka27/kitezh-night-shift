#include "kitezh/lock.hpp"

#include <string>

namespace kitezh {

namespace {

int depthOf(Level level) {
    if (level == Level::Low) {
        return kLowDepthDm;
    }
    return kHighDepthDm;
}

}  // namespace

Lock::Lock()
    : level_(Level::Low),
      lower_(GateState::Closed),
      upper_(GateState::Closed),
      pump_cycles_(0) {}

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
    return pump_cycles_;
}

std::vector<Vessel> Lock::inside() const {
    return inside_;
}

void Lock::open(Gate gate) {
    if (gate == Gate::Lower) {
        if (lower_ == GateState::Open) {
            return;
        }
        if (level_ != Level::Low) {
            throw LockError("нижний затвор открывается только на нижнем уровне");
        }
        if (upper_ == GateState::Open) {
            throw LockError("оба затвора не могут быть открыты");
        }
        lower_ = GateState::Open;
    } else {
        if (upper_ == GateState::Open) {
            return;
        }
        if (level_ != Level::High) {
            throw LockError("верхний затвор открывается только на верхнем уровне");
        }
        if (lower_ == GateState::Open) {
            throw LockError("оба затвора не могут быть открыты");
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
        throw LockError("насос работает только при закрытых затворах");
    }

    Level new_level = Level::High;
    if (level_ == Level::High) {
        new_level = Level::Low;
    }

    for (const Vessel& v : inside_) {
        if (v.draft_dm > depthOf(new_level)) {
            throw LockError("судну не хватит глубины на новом уровне");
        }
    }

    level_ = new_level;
    pump_cycles_ += 1;
}

void Lock::enter(const Vessel& vessel) {
    if (validateVessel(vessel).has_value()) {
        throw LockError("запись о судне некорректна");
    }

    for (const Vessel& v : inside_) {
        if (v.call_sign == vessel.call_sign) {
            throw LockError("такой позывной уже внутри");
        }
    }

    if (vessel.draft_dm > depthOf(level_)) {
        throw LockError("осадка больше текущей глубины");
    }

    GateState entry = lower_;
    if (vessel.direction == Direction::Downstream) {
        entry = upper_;
    }
    if (entry != GateState::Open) {
        throw LockError("входной затвор закрыт");
    }

    if (vessel.kind == VesselKind::Oversized && !inside_.empty()) {
        throw LockError("негабарит входит только в пустую камеру");
    }
    for (const Vessel& v : inside_) {
        if (v.kind == VesselKind::Oversized) {
            throw LockError("внутри негабарит");
        }
    }

    if (!fitsInChamber(inside_, vessel)) {
        throw LockError("не хватает места");
    }

    inside_.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {
    if (inside_.empty()) {
        throw LockError("камера пуста");
    }

    const Vessel& head = inside_[0];
    if (head.call_sign != call_sign) {
        throw LockError("выходит только первое судно");
    }

    GateState exit_gate = upper_;
    if (head.direction == Direction::Downstream) {
        exit_gate = lower_;
    }
    if (exit_gate != GateState::Open) {
        throw LockError("выходной затвор закрыт");
    }

    inside_.erase(inside_.begin());
}

}  // namespace kitezh
