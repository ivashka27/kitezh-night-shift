#include "kitezh/lock.hpp"
#include "kitezh/geometry.hpp"
#include "kitezh/vessel.hpp"
#include <algorithm>
#include <stdexcept>

namespace kitezh {

Lock::Lock()
    : level_(Level::Low),
      lower_gate_(GateState::Closed),
      upper_gate_(GateState::Closed),
      pump_cycles_(0),
      inside_() {
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
    GateState& selected =
        (gate == Gate::Lower) ? lower_gate_ : upper_gate_;

    // Повторное открытие ничего не делает.
    if (selected == GateState::Open) {
        return;
    }

    // Нижние ворота открываются только на нижнем уровне.
    if (gate == Gate::Lower && level_ != Level::Low) {
        throw LockError("Cannot open lower gate at high level");
    }

    // Верхние ворота открываются только на верхнем уровне.
    if (gate == Gate::Upper && level_ != Level::High) {
        throw LockError("Cannot open upper gate at low level");
    }

    // Одновременно два затвора открыты быть не могут.
    if (gate == Gate::Lower && upper_gate_ == GateState::Open) {
        throw LockError("Upper gate is already open");
    }

    if (gate == Gate::Upper && lower_gate_ == GateState::Open) {
        throw LockError("Lower gate is already open");
    }

    selected = GateState::Open;
}

void Lock::close(Gate gate) {
    GateState& selected =
        (gate == Gate::Lower) ? lower_gate_ : upper_gate_;

    // Повторное закрытие ничего не делает.
    if (selected == GateState::Closed) {
        return;
    }

    selected = GateState::Closed;
}

void Lock::pump() {
    // Насос работает только при закрытых затворах.
    if (lower_gate_ != GateState::Closed ||
        upper_gate_ != GateState::Closed) {
        throw LockError("Cannot pump with an open gate");
    }

    // Определяем глубину после переключения уровня.
    const Level new_level =
        (level_ == Level::Low) ? Level::High : Level::Low;

    const int new_depth =
        (new_level == Level::Low)
            ? kLowDepthDm
            : kHighDepthDm;

    // Проверяем все суда до изменения состояния камеры.
    for (const Vessel& vessel : inside_) {
        if (vessel.draft_dm > new_depth) {
            throw LockError("Vessel draft exceeds new water depth");
        }
    }

    // Только после успешных проверок меняем уровень и счётчик.
    level_ = new_level;
    ++pump_cycles_;
}

void Lock::enter(const Vessel& vessel) {
    // Проверяем корректность записи.
    if (validateVessel(vessel).has_value()) {
        throw LockError("Invalid vessel");
    }

    // Позывной должен быть уникальным внутри камеры.
    for (const Vessel& current : inside_) {
        if (current.call_sign == vessel.call_sign) {
            throw LockError("Duplicate call sign");
        }
    }

    // Осадка не должна превышать текущую глубину.
    const int current_depth =
        (level_ == Level::Low)
            ? kLowDepthDm
            : kHighDepthDm;

    if (vessel.draft_dm > current_depth) {
        throw LockError("Vessel draft exceeds current water depth");
    }

    // Определяем входные ворота по направлению судна.
    const Gate entry_gate =
        (vessel.direction == Direction::Upstream)
            ? Gate::Lower
            : Gate::Upper;

    if (gate(entry_gate) != GateState::Open) {
        throw LockError("Entry gate is closed");
    }

    // Негабарит может находиться в камере только один.
    if (vessel.kind == VesselKind::Oversized) {
        if (!inside_.empty()) {
            throw LockError("Oversized vessel must enter alone");
        }
    } else {
        for (const Vessel& current : inside_) {
            if (current.kind == VesselKind::Oversized) {
                throw LockError("Cannot enter beside oversized vessel");
            }
        }
    }

    // Проверяем, хватает ли места по длине.
    if (!fitsInChamber(inside_, vessel)) {
        throw LockError("Not enough chamber space");
    }

    // Добавляем копию судна.
    inside_.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {
    // Нельзя выйти из пустой камеры.
    if (inside_.empty()) {
        throw LockError("Chamber is empty");
    }

    // Выходить может только первое вошедшее судно.
    const Vessel& first = inside_.front();

    if (first.call_sign != call_sign) {
        throw LockError("Vessels must exit in entry order");
    }

    // Определяем выходные ворота.
    const Gate exit_gate =
        (first.direction == Direction::Upstream)
            ? Gate::Upper
            : Gate::Lower;

    if (gate(exit_gate) != GateState::Open) {
        throw LockError("Exit gate is closed");
    }

    // Удаляем первое судно.
    inside_.erase(inside_.begin());
}

}  // namespace kitezh
