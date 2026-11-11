
#include "kitezh/lock.hpp"
#include "kitezh/geometry.hpp"

namespace kitezh {

Lock::Lock() = default;

Level Lock::level() const {
    return level_;
}

GateState Lock::gate(Gate gate) const {
    return gate == Gate::Lower ? lowerstate_ : upperstate_;
}

int Lock::pumpCycles() const {
    return pumpcycles_;
}

std::vector<Vessel> Lock::inside() const {
    return inside_;
}

void Lock::open(Gate gate) {
    if (gate == Gate::Lower) {
        if (lowerstate_ == GateState::Open) return;

        if (level_ != Level::Low or upperstate_ == GateState::Open)
            throw LockError("Нельзя открыть нижний затвор");

        lowerstate_ = GateState::Open;
    } else {
        if (upperstate_ == GateState::Open) return;

        if (level_ != Level::High or lowerstate_ == GateState::Open)
            throw LockError("Нельзя открыть верхний затвор");

        upperstate_ = GateState::Open;
    }
}

void Lock::close(Gate gate) {
    if (gate == Gate::Lower)
        lowerstate_ = GateState::Closed;
    else
        upperstate_ = GateState::Closed;
}

void Lock::pump() {
    if (lowerstate_ == GateState::Open or upperstate_ == GateState::Open)
        throw LockError("Открыт затвор");

    Level next = level_ == Level::Low ? Level::High : Level::Low;
    int depth = next == Level::Low ? kLowDepthDm : kHighDepthDm;

    for (const auto& v : inside_) {
        if (v.draft_dm > depth)
            throw LockError("Недостаточная глубина");
    }

    level_ = next;
    ++pumpcycles_;
}

void Lock::enter(const Vessel& vessel) {
    if (auto error = validateVessel(vessel))
        throw LockError(*error);

    if (vessel.direction == Direction::Upstream) {
        if (lowerstate_ != GateState::Open)
            throw LockError("Нижний затвор закрыт");
    } else {
        if (upperstate_ != GateState::Open)
            throw LockError("Верхний затвор закрыт");
    }

    int depth = level_ == Level::Low ? kLowDepthDm : kHighDepthDm;
    if (vessel.draft_dm > depth)
        throw LockError("Слишком большая осадка");

    for (const auto& v : inside_) {
        if (v.call_sign == vessel.call_sign)
            throw LockError("Позывной уже занят");
    }

    bool oversized = vessel.kind == VesselKind::Oversized;

    for (const auto& v : inside_) {
        if (oversized or v.kind == VesselKind::Oversized)
            throw LockError("Нельзя разместить судно рядом с негабаритом");
    }

    if (!fitsInChamber(inside_, vessel))
        throw LockError("Судно не помещается");

    inside_.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {
    if (inside_.empty() or inside_.front().call_sign != call_sign)
        throw LockError("Нельзя выпустить это судно");

    const auto& v = inside_.front();

    if (v.direction == Direction::Upstream) {
        if (upperstate_ != GateState::Open)
            throw LockError("Верхний затвор закрыт");
    } else {
        if (lowerstate_ != GateState::Open)
            throw LockError("Нижний затвор закрыт");
    }

    inside_.erase(inside_.begin());
}

} // namespace kitezh
