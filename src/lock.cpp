#include "kitezh/lock.hpp"

#include <algorithm>

namespace kitezh {

namespace {

int depthDm(Level level) {
    return level == Level::Low ? kLowDepthDm : kHighDepthDm;
}

}  // namespace

Lock::Lock()
    : level_(Level::Low),
      lower_(GateState::Closed),
      upper_(GateState::Closed),
      pump_cycles_(0),
      inside_() {}

GateState& Lock::slot(Gate gate) {
    return gate == Gate::Lower ? lower_ : upper_;
}

const GateState& Lock::slot(Gate gate) const {
    return gate == Gate::Lower ? lower_ : upper_;
}

Level Lock::level() const {
    return level_;
}

GateState Lock::gate(Gate gate) const {
    return slot(gate);
}

int Lock::pumpCycles() const {
    return pump_cycles_;
}

std::vector<Vessel> Lock::inside() const {
    return inside_;
}

void Lock::open(Gate gate) {
    if (slot(gate) == GateState::Open) {
        return;
    }
    const Level need = gate == Gate::Lower ? Level::Low : Level::High;
    if (level_ != need) {
        throw LockError("gate cannot open at this level");
    }
    const Gate other = gate == Gate::Lower ? Gate::Upper : Gate::Lower;
    if (slot(other) == GateState::Open) {
        throw LockError("other gate is open");
    }
    slot(gate) = GateState::Open;
}

void Lock::close(Gate gate) {
    slot(gate) = GateState::Closed;
}

void Lock::pump() {
    if (lower_ != GateState::Closed || upper_ != GateState::Closed) {
        throw LockError("gates must be closed to pump");
    }
    const Level next = level_ == Level::Low ? Level::High : Level::Low;
    const int depth = depthDm(next);
    for (const Vessel& v : inside_) {
        if (v.draft_dm > depth) {
            throw LockError("not enough depth");
        }
    }
    level_ = next;
    ++pump_cycles_;
}

void Lock::enter(const Vessel& vessel) {
    if (auto err = validateVessel(vessel)) {
        throw LockError("invalid vessel: " + *err);
    }
    for (const Vessel& v : inside_) {
        if (v.call_sign == vessel.call_sign) {
            throw LockError("call sign already inside");
        }
    }
    if (vessel.draft_dm > depthDm(level_)) {
        throw LockError("draft exceeds depth");
    }
    const Gate entry = vessel.direction == Direction::Upstream ? Gate::Lower : Gate::Upper;
    if (slot(entry) != GateState::Open) {
        throw LockError("entry gate is closed");
    }
    if (vessel.kind == VesselKind::Oversized && !inside_.empty()) {
        throw LockError("oversized needs an empty chamber");
    }
    for (const Vessel& v : inside_) {
        if (v.kind == VesselKind::Oversized) {
            throw LockError("nobody enters with oversized");
        }
    }
    if (!fitsInChamber(inside_, vessel)) {
        throw LockError("does not fit");
    }
    inside_.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {
    if (inside_.empty()) {
        throw LockError("chamber is empty");
    }
    const Vessel& head = inside_.front();
    if (head.call_sign != call_sign) {
        throw LockError("not the head of the queue");
    }
    const Gate out = head.direction == Direction::Upstream ? Gate::Upper : Gate::Lower;
    if (slot(out) != GateState::Open) {
        throw LockError("exit gate is closed");
    }
    inside_.erase(inside_.begin());
}

}  // namespace kitezh
