#include "kitezh/lock.hpp"

#include <stdexcept>

namespace kitezh {

Lock::Lock() {
    currentLevel = Level::Low;
    lowerGate = GateState::Closed;
    upperGate = GateState::Closed;
    cycles = 0;
}

Level Lock::level() const {
    return currentLevel;
}

GateState Lock::gate(Gate gate) const {
    if (gate == Gate::Lower) {
        return lowerGate;
    }
    return upperGate;
}

int Lock::pumpCycles() const {
    return cycles;
}

std::vector<Vessel> Lock::inside() const {
    return vessels;
}

void Lock::open(Gate gate) {
    if (gate == Gate::Lower) {
        if (lowerGate == GateState::Open) {
            return;
        }
        if (currentLevel != Level::Low || upperGate == GateState::Open) {
            throw LockError("cannot open lower gate");
        }
        lowerGate = GateState::Open;
    } else {
        if (upperGate == GateState::Open) {
            return;
        }
        if (currentLevel != Level::High || lowerGate == GateState::Open) {
            throw LockError("cannot open upper gate");
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
    if (lowerGate == GateState::Open || upperGate == GateState::Open) {
        throw LockError("gates are open");
    }

    Level newLevel = currentLevel == Level::Low ? Level::High : Level::Low;
    int newDepth = newLevel == Level::Low ? kLowDepthDm : kHighDepthDm;

    for (const Vessel& vessel : vessels) {
        if (vessel.draft_dm > newDepth) {
            throw LockError("draft too large");
        }
    }

    currentLevel = newLevel;
    ++cycles;
}

void Lock::enter(const Vessel& vessel) {
    if (validateVessel(vessel).has_value()) {
        throw LockError("invalid vessel");
    }

    for (const Vessel& current : vessels) {
        if (current.call_sign == vessel.call_sign) {
            throw LockError("duplicate call sign");
        }
    }

    int depth = currentLevel == Level::Low ? kLowDepthDm : kHighDepthDm;

    if (vessel.draft_dm > depth) {
        throw LockError("draft too large");
    }

    if (vessel.direction == Direction::Upstream) {
        if (lowerGate != GateState::Open) {
            throw LockError("lower gate is closed");
        }
    } else {
        if (upperGate != GateState::Open) {
            throw LockError("upper gate is closed");
        }
    }

    if (vessel.kind == VesselKind::Oversized && !vessels.empty()) {
        throw LockError("oversized vessel must be alone");
    }

    for (const Vessel& current : vessels) {
        if (current.kind == VesselKind::Oversized) {
            throw LockError("oversized vessel inside");
        }
    }

    if (!fitsInChamber(vessels, vessel)) {
        throw LockError("not enough space");
    }

    vessels.push_back(vessel);
}

void Lock::exit(const std::string& callSign) {
    if (vessels.empty()) {
        throw LockError("chamber is empty");
    }

    const Vessel& first = vessels.front();

    if (first.call_sign != callSign) {
        throw LockError("wrong vessel");
    }

    if (first.direction == Direction::Upstream) {
        if (upperGate != GateState::Open) {
            throw LockError("upper gate is closed");
        }
    } else {
        if (lowerGate != GateState::Open) {
            throw LockError("lower gate is closed");
        }
    }

    vessels.erase(vessels.begin());
}

}  // namespace kitezh
