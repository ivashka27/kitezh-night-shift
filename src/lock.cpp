#include "kitezh/lock.hpp"

namespace kitezh {

Lock::Lock() {
    waterLevel = Level::Low;
    lowerGate = GateState::Closed;
    upperGate = GateState::Closed;
    pumpCount = 0;
}

Level Lock::level() const {
    return waterLevel;
}

GateState Lock::gate(Gate gate) const {
    if (gate == Gate::Lower) {
        return lowerGate;
    }
    return upperGate;
}

int Lock::pumpCycles() const {
    return pumpCount;
}

std::vector<Vessel> Lock::inside() const {
    return vessels;
}
void Lock::open(Gate gate) {
    if (gate == Gate::Lower) {
        if (lowerGate == GateState::Open) {
            return;
        }
        if (waterLevel != Level::Low || upperGate == GateState::Open) {
            throw LockError("Нельзя открыть нижний затвор");
        }
        lowerGate = GateState::Open;
    } else {
        if (upperGate == GateState::Open) {
            return;
        }
        if (waterLevel != Level::High || lowerGate == GateState::Open) {
            throw LockError("Нельзя открыть верхний затвор");
        }
        upperGate = GateState::Open;
    }
}

void Lock::close(Gate gate) {
    if (gate == Gate::Lower) {
        lowerGate = GateState::Closed;
    } else {
        upperGate = GateState::Closed;
    }
}

void Lock::pump() {
    if (upperGate == GateState::Open || lowerGate == GateState::Open) {
        throw LockError("Затворы должны быть закрыты");
    }
    int next_depth;
    Level next_level;
    if (waterLevel == Level::Low) {
        next_level = Level::High;
        next_depth = kHighDepthDm;
    } else {
        next_level = Level::Low;
        next_depth = kLowDepthDm;
    }
    for (const Vessel& vessel : vessels) {
        if (vessel.draft_dm > next_depth) {
            throw LockError("Судну не хватает глубины");
        }
    }
    waterLevel = next_level;
    pumpCount += 1;
}

void Lock::enter(const Vessel& vessel) {
    if (validateVessel(vessel).has_value()) {
        throw LockError("Некорректные данные судна");
    }
    for (const Vessel& current : vessels) {
        if (current.call_sign == vessel.call_sign) {
            throw LockError("Судно с таким позывным уже находится в камере");
        }
    }
    int current_depth;
    if (waterLevel == Level::Low) {
        current_depth = kLowDepthDm;
    } else {
        current_depth = kHighDepthDm;
    }
    if (vessel.draft_dm > current_depth) {
        throw LockError("Судну не хватает глубины");
    }
    if (vessel.direction == Direction::Upstream) {
        if (lowerGate != GateState::Open) {
            throw LockError("Нижний затвор закрыт");
        }
    } else {
        if (upperGate != GateState::Open) {
            throw LockError("Верхний затвор закрыт");
        }
    }
    if (vessel.kind == VesselKind::Oversized && !vessels.empty()) {
        throw LockError("Камера должна быть пустой");
    }
    for (const Vessel& current : vessels) {
        if (current.kind == VesselKind::Oversized) {
            throw LockError("В камере уже находится негабаритное судно");
        }
    }
    if (!fitsInChamber(vessels, vessel)) {
        throw LockError("Судно не помещается в камере");
    }
    vessels.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {
    if (vessels.empty()) {
        throw LockError("Камера пуста");
    }
    const Vessel& first = vessels.front();
    if (first.call_sign != call_sign) {
        throw LockError("Это судно не первое в очереди");
    }
    if (first.direction == Direction::Upstream) {
        if (upperGate != GateState::Open) {
            throw LockError("Верхний затвор закрыт");
        }
    } else {
        if (lowerGate != GateState::Open) {
            throw LockError("Нижний затвор закрыт");
        }
    }
    vessels.erase(vessels.begin());
}

}  // namespace kitezh
