#include "kitezh/lock.hpp"

namespace kitezh {

Lock::Lock() = default;

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
    if ((gate == Gate::Lower &&
         (level_ != Level::Low || upper_ == GateState::Open)) ||
        (gate == Gate::Upper &&
         (level_ != Level::High || lower_ == GateState::Open))) {
        throw LockError("cannot open gate");
    }

    (gate == Gate::Lower ? lower_ : upper_) = GateState::Open;
}

void Lock::close(Gate gate) {
    (gate == Gate::Lower ? lower_ : upper_) = GateState::Closed;
}

void Lock::pump() {
    if (lower_ == GateState::Open || upper_ == GateState::Open) {
        throw LockError("gates are open");
    }

    const Level next = level_ == Level::Low ? Level::High : Level::Low;
    const int depth = next == Level::Low ? kLowDepthDm : kHighDepthDm;

    for (const auto& vessel : inside_) {
        if (vessel.draft_dm > depth) {
            throw LockError("insufficient depth");
        }
    }

    level_ = next;
    ++pump_cycles_;
}

void Lock::enter(const Vessel& vessel) {
    if (validateVessel(vessel)) {
        throw LockError("invalid vessel");
    }

    for (const auto& existing : inside_) {
        if (existing.call_sign == vessel.call_sign) {
            throw LockError("duplicate call sign");
        }

        if (existing.kind == VesselKind::Oversized) {
            throw LockError("oversized vessel inside");
        }
    }

    const int depth = level_ == Level::Low ? kLowDepthDm : kHighDepthDm;
    if (vessel.draft_dm > depth) {
        throw LockError("insufficient depth");
    }

    const Gate entrance = vessel.direction == Direction::Upstream
                              ? Gate::Lower
                              : Gate::Upper;

    if (gate(entrance) != GateState::Open) {
        throw LockError("entrance closed");
    }

    if (vessel.kind == VesselKind::Oversized && !inside_.empty()) {
        throw LockError("chamber occupied");
    }

    if (!fitsInChamber(inside_, vessel)) {
        throw LockError("chamber full");
    }

    inside_.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {
    if (inside_.empty() || inside_.front().call_sign != call_sign) {
        throw LockError("not first in chamber");
    }

    const Gate exit_gate = inside_.front().direction == Direction::Upstream
                               ? Gate::Upper
                               : Gate::Lower;

    if (gate(exit_gate) != GateState::Open) {
        throw LockError("exit closed");
    }

    inside_.erase(inside_.begin());
}

}  // namespace kitezh