#include "kitezh/lock.hpp"

namespace kitezh {

    namespace {
        int depthFor(Level l) {
            return (l == Level::Low) ? kLowDepthDm : kHighDepthDm;
        }
    }

    Lock::Lock() = default;

    Level Lock::level() const { return level_; }

    GateState Lock::gate(Gate g) const {
        return (g == Gate::Lower) ? lower_ : upper_;
    }

    int Lock::pumpCycles() const { return cycles_; }

    std::vector<Vessel> Lock::inside() const { return inside_; }

    void Lock::open(Gate g) {
        if (g == Gate::Lower) {
            if (level_ != Level::Low) throw LockError("lower gate needs low level");
            if (lower_ == GateState::Open) return;
            if (upper_ == GateState::Open) throw LockError("upper already open");
            lower_ = GateState::Open;
        }
        else {
            if (level_ != Level::High) throw LockError("upper gate needs high level");
            if (upper_ == GateState::Open) return;
            if (lower_ == GateState::Open) throw LockError("lower already open");
            upper_ = GateState::Open;
        }
    }

    void Lock::close(Gate g) {
        if (g == Gate::Lower) {
            if (lower_ == GateState::Closed) return;
            lower_ = GateState::Closed;
        }
        else {
            if (upper_ == GateState::Closed) return;
            upper_ = GateState::Closed;
        }
    }

    void Lock::pump() {
        if (lower_ == GateState::Open || upper_ == GateState::Open)
            throw LockError("gates must be closed");
        int newDepth = depthFor((level_ == Level::Low) ? Level::High : Level::Low);
        for (const auto& v : inside_) {
            if (v.draft_dm > newDepth) throw LockError("not enough depth");
        }
        level_ = (level_ == Level::Low) ? Level::High : Level::Low;
        ++cycles_;
    }

    void Lock::enter(const Vessel& vessel) {
        if (validateVessel(vessel)) throw LockError("invalid vessel");
        for (const auto& v : inside_)
            if (v.call_sign == vessel.call_sign) throw LockError("duplicate call sign");
        if (vessel.draft_dm > depthFor(level_)) throw LockError("draft too deep");
        Gate inGate = (vessel.direction == Direction::Upstream) ? Gate::Lower : Gate::Upper;
        if (gate(inGate) != GateState::Open) throw LockError("input gate closed");
        if (vessel.kind == VesselKind::Oversized) {
            if (!inside_.empty()) throw LockError("oversized needs empty chamber");
        }
        else {
            for (const auto& v : inside_)
                if (v.kind == VesselKind::Oversized) throw LockError("oversized inside");
        }
        if (!fitsInChamber(inside_, vessel)) throw LockError("does not fit");
        inside_.push_back(vessel);
    }

    void Lock::exit(const std::string& call_sign) {
        if (inside_.empty()) throw LockError("chamber empty");
        if (inside_.front().call_sign != call_sign) throw LockError("not head of queue");
        Gate outGate = (inside_.front().direction == Direction::Upstream) ? Gate::Upper : Gate::Lower;
        if (gate(outGate) != GateState::Open) throw LockError("output gate closed");
        inside_.erase(inside_.begin());
    }

}  // namespace kitezh