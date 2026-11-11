#include "kitezh/lock.hpp"

namespace kitezh {

namespace {

Level opposite(Level level) {
    return level == Level::Low ? Level::High : Level::Low;
}

Gate entryGate(Direction direction) {
    return direction == Direction::Upstream ? Gate::Lower : Gate::Upper;
}

Gate exitGate(Direction direction) {
    return direction == Direction::Upstream ? Gate::Upper : Gate::Lower;
}

}  // namespace

Lock::Lock() = default;

int Lock::depthAt(Level level) {
    return level == Level::Low ? kLowDepthDm : kHighDepthDm;
}

Level Lock::level() const {
    return level_;
}

GateState Lock::gate(Gate gate) const {
    return gate == Gate::Lower ? lower_ : upper_;
}

int Lock::pumpCycles() const {
    return pump_cycles_;
}

std::vector<Vessel> Lock::inside() const {
    return inside_;
}

void Lock::open(Gate gate) {
    if (this->gate(gate) == GateState::Open) {
        return;
    }
    const Level required = gate == Gate::Lower ? Level::Low : Level::High;
    if (level_ != required) {
        throw LockError("open: затвор не соответствует уровню воды");
    }
    const Gate other = gate == Gate::Lower ? Gate::Upper : Gate::Lower;
    if (this->gate(other) == GateState::Open) {
        throw LockError("open: второй затвор уже открыт");
    }
    (gate == Gate::Lower ? lower_ : upper_) = GateState::Open;
}

void Lock::close(Gate gate) {
    (gate == Gate::Lower ? lower_ : upper_) = GateState::Closed;
}

void Lock::pump() {
    if (lower_ == GateState::Open || upper_ == GateState::Open) {
        throw LockError("pump: затвор открыт");
    }
    const Level target = opposite(level_);
    for (const Vessel& vessel : inside_) {
        if (vessel.draft_dm > depthAt(target)) {
            throw LockError("pump: судно " + vessel.call_sign + " сядет на грунт");
        }
    }
    level_ = target;
    ++pump_cycles_;
}

void Lock::enter(const Vessel& vessel) {
    if (const auto error = validateVessel(vessel)) {
        throw LockError("enter: " + *error);
    }
    for (const Vessel& other : inside_) {
        if (other.call_sign == vessel.call_sign) {
            throw LockError("enter: позывной уже внутри");
        }
    }
    if (vessel.draft_dm > depthAt(level_)) {
        throw LockError("enter: не хватает глубины");
    }
    if (gate(entryGate(vessel.direction)) != GateState::Open) {
        throw LockError("enter: входной затвор закрыт");
    }
    if (!inside_.empty()) {
        if (vessel.kind == VesselKind::Oversized) {
            throw LockError("enter: негабарит входит только в пустую камеру");
        }
        if (inside_.front().kind == VesselKind::Oversized) {
            throw LockError("enter: камера занята негабаритом");
        }
    }
    if (!fitsInChamber(inside_, vessel)) {
        throw LockError("enter: не хватает длины камеры");
    }
    inside_.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {
    if (inside_.empty()) {
        throw LockError("exit: камера пуста");
    }
    const Vessel& head = inside_.front();
    if (head.call_sign != call_sign) {
        throw LockError("exit: выходит только первое вошедшее судно");
    }
    if (gate(exitGate(head.direction)) != GateState::Open) {
        throw LockError("exit: выходной затвор закрыт");
    }
    inside_.erase(inside_.begin());
}

}  // namespace kitezh
