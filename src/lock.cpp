#include "kitezh/lock.hpp"

#include <stdexcept>

namespace kitezh {

Lock::Lock() {
    
}

Level Lock::level() const {
    return level_;
}

GateState Lock::gate(Gate selectedGate) const {
    if (selectedGate == Gate::Lower) {
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

void Lock::open(Gate selectedGate) {
    // Недопустимое открытие бросает LockError и не меняет камеру.
    
    if (selectedGate == Gate::Lower) {
        if (lower_gate_ == GateState::Open) {
            return;
        }
        if (level_ != Level::Low) {
            throw LockError("Wrong water level");
        }
        if (upper_gate_ == GateState::Open) {
            throw LockError("Upper gate is open");
        }
        lower_gate_ = GateState::Open;
        return;
    }

    if (upper_gate_ == GateState::Open) {
        return;
    }
    if (level_ != Level::High) {
        throw LockError("Wrong water level");
    }
    if (lower_gate_ == GateState::Open) {
        throw LockError("Lower gate is open");
    }
    upper_gate_ = GateState::Open;
}

void Lock::close(Gate selectedGate) {
    if (selectedGate == Gate::Lower) {
        lower_gate_ = GateState::Closed;
    } else {
        upper_gate_ = GateState::Closed;
    }
}

void Lock::pump() {
    // Отказ насоса не тратит такт и не сажает уже вошедшее судно на грунт.
    if (lower_gate_ == GateState::Open ||
        upper_gate_ == GateState::Open) {
        throw LockError("A gate is open");
    }
    Level newLevel;

    if (level_ == Level::Low) {
        newLevel = Level::High;
    } else {
        newLevel = Level::Low;
    }

    int newDepth = kLowDepthDm;
    if (newLevel == Level::High) {
        newDepth = kHighDepthDm;
    }
    
    for (const Vessel& ship : inside_) {
        if (ship.draft_dm > newDepth) {
            throw LockError("Not enough depth");
        }
    }

    pump_cycles_++;
    level_ = newLevel;
}

void Lock::enter(const Vessel& vessel) {
    // Внутри должна оказаться копия. Входной затвор сам не открывается.
    if (validateVessel(vessel).has_value()) {
        throw LockError("Invalid vessel");
    }
    for (const Vessel& ship : inside_) {
        if (ship.call_sign == vessel.call_sign) {
            throw LockError("Duplicate call sign");
        }
    }
    int currentDepth = kLowDepthDm;
    if (level_ == Level::High) {
        currentDepth = kHighDepthDm;
    }
    if (vessel.draft_dm > currentDepth) {
        throw LockError("Not enough depth");
    }

    if (vessel.direction == Direction::Upstream) {
        if (lower_gate_ != GateState::Open) {
            throw LockError("Lower gate is closed");
        }
    } else {
        if (upper_gate_ != GateState::Open) {
            throw LockError("Upper gate is closed");
        }
    }
    if (vessel.kind == VesselKind::Oversized) {
        if (!inside_.empty()) {
            throw LockError("Oversized must be alone");
        }
    }
    for (const Vessel& ship : inside_) {
        if (ship.kind == VesselKind::Oversized) {
            throw LockError("Oversized is already inside");
        }
    }

    if (!fitsInChamber(inside_, vessel)) {
        throw LockError("Not enough space");
    }

    inside_.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {
    // Выходит только голова очереди и только в свои ворота.
    if (inside_.empty()) {
        throw LockError("Chamber is empty");
    }

    const Vessel& first = inside_.front();

    if (first.call_sign != call_sign) {
        throw LockError("Wrong vessel order");
    }

    if (first.direction == Direction::Upstream) {
        if (upper_gate_ != GateState::Open) {
            throw LockError("Upper gate is closed");
        }
    } else {
        if (lower_gate_ != GateState::Open) {
            throw LockError("Lower gate is closed");
        }
    }

    inside_.erase(inside_.begin());
}

}  // namespace kitezh
