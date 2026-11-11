#include "kitezh/lock.hpp"

#include <stdexcept>

namespace kitezh {

Lock::Lock() {
    level_ = Level::Low;
    lower_gate_ = GateState::Closed;
    upper_gate_ = GateState::Closed;
    pump_cycles_ = 0;
    inside_.clear();
}

Level Lock::level() const {
    return level_;
}

GateState Lock::gate(Gate gate) const {
    return (gate == Gate::Lower) ? lower_gate_ : upper_gate_;
}

int Lock::pumpCycles() const {
    return pump_cycles_;
}

std::vector<Vessel> Lock::inside() const {
    return inside_;
}

void Lock::open(Gate gate) {
    // Недопустимое открытие бросает LockError и не меняет камеру.
    if (gate == Gate::Lower) {
        if (level_ != Level::Low || upper_gate_ == GateState::Open) {
            throw LockError("нельзя открыть нижний затвор");
        }
        lower_gate_ = GateState::Open;
    } else {
        if (level_ != Level::High || lower_gate_ == GateState::Open) {
            throw LockError("нельзя открыть верхний затвор");
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
    // Отказ насоса не тратит такт и не сажает уже вошедшее судно на грунт.
    if (lower_gate_ != GateState::Closed || upper_gate_ != GateState::Closed) {
        throw LockError("нельзя переключать уровень при открытом затворе");
    }
    const Level next_level = (level_ == Level::Low) ? Level::High : Level::Low;
    const int next_depth = (next_level == Level::Low) ? kLowDepthDm : kHighDepthDm;
    for (const Vessel& v : inside_) {
        if (v.draft_dm > next_depth) {
            throw LockError("судну внутри не хватает глубины на новом уровне");
        }
    }
    level_ = next_level;
    ++pump_cycles_;
}

void Lock::enter(const Vessel& vessel) {
    // Внутри должна оказаться копия. Входной затвор сам не открывается.
    if (validateVessel(vessel).has_value()) {
        throw LockError("невалидная запись о судне");
    }
    if (vessel.direction == Direction::Upstream) {
        if (lower_gate_ != GateState::Open) {
            throw LockError("нижний затвор закрыт для входа судна вверх");
        }
    } else {
        if (upper_gate_ != GateState::Open) {
            throw LockError("верхний затвор закрыт для входа судна вниз");
        }
    }
    const int current_depth = (level_ == Level::Low) ? kLowDepthDm : kHighDepthDm;
    if (vessel.draft_dm > current_depth) {
        throw LockError("осадка превышает глубину текущего уровня");
    }
    for (const Vessel& existing : inside_) {
        if (existing.call_sign == vessel.call_sign) {
            throw LockError("дубликат позывного в камере");
        }
    }
    if (!inside_.empty()) {
        if (vessel.kind == VesselKind::Oversized ||
            inside_.front().kind == VesselKind::Oversized) {
            throw LockError("негабаритное судно шлюзуется только в одиночку");
        }
    }
    if (!fitsInChamber(inside_, vessel)) {
        throw LockError("судно не помещается в камеру по длине");
    }
    inside_.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {
    // Выходит только голова очереди и только в свои ворота.
    if (inside_.empty() || inside_.front().call_sign != call_sign) {
        throw LockError("выходить может только первое судно в очереди камеры");
    }
    if (inside_.front().direction == Direction::Upstream) {
        if (upper_gate_ != GateState::Open) {
            throw LockError("верхний затвор закрыт для выхода судна вверх");
        }
    } else {
        if (lower_gate_ != GateState::Open) {
            throw LockError("нижний затвор закрыт для выхода судна вниз");
        }
    }
    inside_.erase(inside_.begin());
}

} // namespace kitezh
