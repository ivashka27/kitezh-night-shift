
#pragma once

#include "kitezh/vessel.hpp"

#include <stdexcept>
#include <vector>

namespace kitezh {

enum class Level {
    Low,
    High,
};

enum class Gate {
    Lower,
    Upper,
};

enum class GateState {
    Closed,
    Open,
};

class LockError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Lock {
public:
    Lock();

    [[nodiscard]] Level level() const;
    [[nodiscard]] GateState gate(Gate gate) const;
    [[nodiscard]] int pumpCycles() const;

    [[nodiscard]] std::vector<Vessel> inside() const;

    void open(Gate gate);
    void close(Gate gate);
    void pump();
    void enter(const Vessel& vessel);
    void exit(const std::string& call_sign);

private:
    Level level_{Level::Low};
    GateState lower_gate_{GateState::Closed};
    GateState upper_gate_{GateState::Closed};
    int pump_cycles_{0};
    std::vector<Vessel> inside_;
};

}  // namespace kitezh
