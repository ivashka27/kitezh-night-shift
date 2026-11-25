#include "kitezh/lock.hpp"
#include "kitezh/geometry.hpp"

namespace kitezh {

    Lock::Lock() = default;

    Level Lock::level() const {
        return level_;
    }

    GateState Lock::gate(Gate gate) const {
        return gate == Gate::Lower ? lower_gate_ : upper_gate_;
    }

    int Lock::pumpCycles() const {
        return pump_cycles_;
    }

    std::vector<Vessel> Lock::inside() const {
        return inside_;
    }

    void Lock::open(Gate gate) {
        if (gate == Gate::Lower) {
            if (level_ != Level::Low) {
                throw LockError("Cannot open lower gate at high level");
            }
            lower_gate_ = GateState::Open;
        }
        else {
            if (level_ != Level::High) {
                throw LockError("Cannot open upper gate at low level");
            }
            upper_gate_ = GateState::Open;
        }
    }

    void Lock::close(Gate gate) {
        if (gate == Gate::Lower) {
            lower_gate_ = GateState::Closed;
        }
        else {
            upper_gate_ = GateState::Closed;
        }
    }

    void Lock::pump() {
        if (lower_gate_ != GateState::Closed || upper_gate_ != GateState::Closed) {
            throw LockError("Cannot pump with open gate");
        }
        Level target_level = (level_ == Level::Low) ? Level::High : Level::Low;
        int target_depth = (target_level == Level::Low) ? kLowDepthDm : kHighDepthDm;

        for (const auto& v : inside_) {
            if (v.draft_dm > target_depth) {
                throw LockError("Draft exceeds depth at target level");
            }
        }

        level_ = target_level;
        ++pump_cycles_;
    }

    void Lock::enter(const Vessel& vessel) {
        if (validateVessel(vessel).has_value()) {
            throw LockError("Invalid vessel");
        }
        for (const auto& v : inside_) {
            if (v.call_sign == vessel.call_sign) {
                throw LockError("Duplicate call sign in chamber");
            }
        }
        int current_depth = (level_ == Level::Low) ? kLowDepthDm : kHighDepthDm;
        if (vessel.draft_dm > current_depth) {
            throw LockError("Draft exceeds current depth");
        }
        if (vessel.direction == Direction::Upstream) {
            if (lower_gate_ != GateState::Open) {
                throw LockError("Lower gate must be open for upstream entry");
            }
        }
        else {
            if (upper_gate_ != GateState::Open) {
                throw LockError("Upper gate must be open for downstream entry");
            }
        }
        if (!inside_.empty()) {
            if (vessel.kind == VesselKind::Oversized || inside_.front().kind == VesselKind::Oversized) {
                throw LockError("Oversized vessel cannot share chamber");
            }
        }
        if (!fitsInChamber(inside_, vessel)) {
            throw LockError("Vessel does not fit in chamber");
        }

        inside_.push_back(vessel);
    }

    void Lock::exit(const std::string& call_sign) {
        if (inside_.empty()) {
            throw LockError("Chamber is empty");
        }
        const Vessel& head = inside_.front();
        if (head.call_sign != call_sign) {
            throw LockError("Vessel is not at the head of the queue");
        }
        if (head.direction == Direction::Upstream) {
            if (upper_gate_ != GateState::Open) {
                throw LockError("Upper gate must be open for upstream exit");
            }
        }
        else {
            if (lower_gate_ != GateState::Open) {
                throw LockError("Lower gate must be open for downstream exit");
            }
        }

        inside_.erase(inside_.begin());
    }

} // namespace kitezh
