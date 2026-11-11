#include "kitezh/lock.hpp"

#include <stdexcept>

namespace kitezh {

Lock::Lock() {

}

Level Lock::level() const {
    return level_;
}

GateState Lock::gate(Gate gate) const {
    if (gate == Gate::Lower) {
        return lower_gate_;
    } else {
        return upper_gate_;
    }
}

int Lock::pumpCycles() const {
    return pump_cycles_;
}

std::vector<Vessel> Lock::inside() const {
    return inside_;
}
void Lock::open(Gate gate) {
    // 1. Если этот затвор уже открыт, ничего не делаем (просто выходим)
    if (gate == Gate::Lower && lower_gate_ == GateState::Open) {
        return;
    }
    if (gate == Gate::Upper && upper_gate_ == GateState::Open) {
        return;
    }
    // 2. Проверяем, не открыт ли уже другой затвор
    if (gate == Gate::Lower && upper_gate_ == GateState::Open) {
        throw LockError("Верхний затвор открыт, нельзя открыть нижний");
    }
    if (gate == Gate::Upper && lower_gate_ == GateState::Open) {
        throw LockError("Нижний затвор открыт, нельзя открыть верхний");
    }

    // 3. Проверяем, соответствует ли затвор уровню воды
    if (gate == Gate::Lower && level_ == Level::High) {
        throw LockError("Нельзя открыть нижний затвор на верхнем уровне");
    }
    if (gate == Gate::Upper && level_ == Level::Low) {
        throw LockError("Нельзя открыть верхний затвор на нижнем уровне");
    }

    // Если все проверки пройдены, откроем нужный
    if (gate == Gate::Lower) {
        lower_gate_ = GateState::Open;
    } else {
        upper_gate_ = GateState::Open;
    }
}

void Lock::close(Gate gate) {
    // если затвор уже закрыт, ничего не делаем
    if (gate == Gate::Lower && lower_gate_ == GateState::Closed) {
        return;
    }
    if (gate == Gate::Upper && upper_gate_ == GateState::Closed) {
        return;
    }

    // закрываем нужный затвор
    if (gate == Gate::Lower) {
        lower_gate_ = GateState::Closed;
    } else {
        upper_gate_ = GateState::Closed;
    }
}

void Lock::pump() {
    // 1. насос не работает, если хоть один затвор открыт.
    if (lower_gate_ == GateState::Open || upper_gate_ == GateState::Open) {
        throw LockError("Нельзя качать воду при открытых затворах");
    }

    // 2. определяем, каким будет следующий уровень и какая там глубина
    Level next_level;
    int next_depth;

    if (level_ == Level::Low) {
        next_level = Level::High;
        next_depth = 42;
    } else {
        next_level = Level::Low;
        next_depth = 28;
    }

    // 3. проверяем суда внутри. хватит ли им глубины на новом уровне
    for (int i = 0; i < inside_.size(); i++) {
        if (inside_[i].draft_dm > next_depth) {
            // Если осадка судна больше, чем глубина, насос ломается
            throw LockError("Насос не может посадить судно на грунт");
        }
    }

    // 4. Если всё хорошо, ьо меняем уровень и прибавляем 1 к счетчику тактов
    level_ = next_level;
    pump_cycles_ = pump_cycles_ + 1;
}

void Lock::enter(const Vessel& vessel) {
    // 1. проверяем корректность судна
    if (validateVessel(vessel).has_value()) {
        throw LockError("Неверные данные судна");
    }

    // 2. проверяем дубликаты позывных и негабарит
    for (int i = 0; i < inside_.size(); i++) {
        if (inside_[i].call_sign == vessel.call_sign) {
            throw LockError("Судно с таким позывным уже внутри");
        }
        if (inside_[i].kind == VesselKind::Oversized) {
            throw LockError("Внутри уже есть негабарит, больше никого нельзя");
        }
    }

    if (vessel.kind == VesselKind::Oversized && inside_.size() > 0) {
        throw LockError("Негабарит заходит только в пустую камеру");
    }

    // 3.проверяем глубину
    int current_depth;
    if (level_ == Level::Low) {
        current_depth = 28;
    } else {
        current_depth = 42;
    }

    if (vessel.draft_dm > current_depth) {
        throw LockError("Осадка судна больше текущей глубины");
    }

    // 4.проверяем, открыты ли нужные ворота для направления судна
    if (vessel.direction == Direction::Upstream && lower_gate_ != GateState::Open) {
        throw LockError("Для входа вверх должны быть открыты нижние ворота");
    }
    if (vessel.direction == Direction::Downstream && upper_gate_ != GateState::Open) {
        throw LockError("Для входа вниз должны быть открыты верхние ворота");
    }

    // 5. Проверяем, влезает ли по длине
    if (fitsInChamber(inside_, vessel) == false) {
        throw LockError("Судно не влезает в камеру по длине");
    }

    // если все проверки пройдены, добавляем судно в конец очереди
    inside_.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {
    /// 1. проверяем, что камера не пуста
    if (inside_.size() == 0) {
        throw LockError("Камера пуста");
    }

    // 2. проверяем, что это первое судно в очереди
    if (inside_[0].call_sign != call_sign) {
        throw LockError("Это судно не первое в очереди");
    }

    // 3. проверяем, открыты ли выходные ворота для этого судна
    if (inside_[0].direction == Direction::Upstream && upper_gate_ != GateState::Open) {
        throw LockError("Для выхода вверх должны быть открыты верхние ворота");
    }
    if (inside_[0].direction == Direction::Downstream && lower_gate_ != GateState::Open) {
        throw LockError("Для выхода вниз должны быть открыты нижние ворота");
    }

    // 4. Если всё в порядке, удаляем первое судно из списка
    inside_.erase(inside_.begin());
}

}  // namespace kitezh
