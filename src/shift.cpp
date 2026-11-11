
#include "kitezh/shift.hpp"

#include "kitezh/geometry.hpp"
#include "kitezh/lock.hpp"

#include <string>
#include <unordered_set>
#include <vector>

namespace kitezh {
namespace {

void conductBatch(
    Lock& lock,
    const std::vector<const Vessel*>& batch,
    ShiftReport& report
);

Gate entryGateFor(Direction direction);

Level entryLevelFor(Direction direction);

Gate exitGateFor(Direction direction);

bool fitsPointerBatch(
    const std::vector<const Vessel*>& inside,
    const Vessel& next
);

std::vector<const Vessel*> makeBatch(std::vector<const Vessel*>& waiting);

std::vector<const Vessel*> prepareQueue(
    const std::vector<Vessel>& queue,
    std::vector<Refusal>& refused
);

void conductBatch(
    Lock& lock,
    const std::vector<const Vessel*>& batch,
    ShiftReport& report
) {
    if (batch.empty()) {
        return;
    }

    const Direction direction = batch.front()->direction;
    const Level required_level = entryLevelFor(direction);
    const Gate entry_gate = entryGateFor(direction);
    const Gate exit_gate = exitGateFor(direction);

    if (lock.level() != required_level) {
        lock.pump();
    }

    lock.open(entry_gate);

    for (const Vessel* vessel : batch) {
        lock.enter(*vessel);
    }

    lock.close(entry_gate);
    lock.pump();
    lock.open(exit_gate);

    for (const Vessel* vessel : batch) {
        lock.exit(vessel->call_sign);
        report.passed.push_back(vessel->call_sign);
    }

    lock.close(exit_gate);
}

Gate entryGateFor(Direction direction) {
    switch (direction) {
        case Direction::Upstream:
            return Gate::Lower;
        case Direction::Downstream:
            return Gate::Upper;
    }

    throw LockError("unknown vessel direction");
}

Level entryLevelFor(Direction direction) {
    switch (direction) {
        case Direction::Upstream:
            return Level::Low;
        case Direction::Downstream:
            return Level::High;
    }

    throw LockError("unknown vessel direction");
}

Gate exitGateFor(Direction direction) {
    switch (direction) {
        case Direction::Upstream:
            return Gate::Upper;
        case Direction::Downstream:
            return Gate::Lower;
    }

    throw LockError("unknown vessel direction");
}

bool fitsPointerBatch(
    const std::vector<const Vessel*>& inside,
    const Vessel& next
) {
    long long occupied_length = next.length_m;

    for (const Vessel* vessel : inside) {
        occupied_length += vessel->length_m;
        occupied_length += kGapM;
    }

    return occupied_length <= kChamberLengthM;
}

std::vector<const Vessel*> makeBatch(std::vector<const Vessel*>& waiting) {
    std::vector<const Vessel*> batch;
    std::vector<const Vessel*> remaining;

    if (waiting.empty()) {
        return batch;
    }

    const Vessel& first_vessel = *waiting.front();
    batch.push_back(waiting.front());

    if (first_vessel.kind == VesselKind::Oversized) {
        for (std::size_t index = 1; index < waiting.size(); ++index) {
            remaining.push_back(waiting[index]);
        }

        waiting.swap(remaining);
        return batch;
    }

    bool has_skipped_vessel = false;

    for (std::size_t index = 1; index < waiting.size(); ++index) {
        const Vessel& candidate = *waiting[index];

        const bool same_direction = candidate.direction == first_vessel.direction;
        const bool is_regular_size = candidate.kind != VesselKind::Oversized;
        const bool has_room = fitsPointerBatch(batch, candidate);
        const bool may_join_after_skip =
            !has_skipped_vessel || candidate.kind == VesselKind::Ambulance;

        if (same_direction && is_regular_size && has_room && may_join_after_skip) {
            batch.push_back(waiting[index]);
        } else {
            has_skipped_vessel = true;
            remaining.push_back(waiting[index]);
        }
    }

    waiting.swap(remaining);
    return batch;
}

std::vector<const Vessel*> prepareQueue(
    const std::vector<Vessel>& queue,
    std::vector<Refusal>& refused
) {
    std::unordered_set<std::string> known_call_signs;
    std::vector<const Vessel*> waiting;

    for (const Vessel& vessel : queue) {
        const bool is_new_call_sign = known_call_signs.insert(vessel.call_sign).second;

        if (!is_new_call_sign) {
            refused.push_back(Refusal{vessel.call_sign, "duplicate"});
            continue;
        }

        if (validateVessel(vessel).has_value()) {
            refused.push_back(Refusal{vessel.call_sign, "invalid"});
            continue;
        }

        if (vessel.draft_dm > kLowDepthDm) {
            refused.push_back(Refusal{vessel.call_sign, "draft"});
            continue;
        }

        waiting.push_back(&vessel);
    }

    return waiting;
}

}  // namespace

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
    ShiftReport report;
    std::vector<const Vessel*> waiting = prepareQueue(queue, report.refused);
    Lock lock;

    while (!waiting.empty()) {
        const std::vector<const Vessel*> batch = makeBatch(waiting);
        conductBatch(lock, batch, report);
    }

    report.pump_cycles = lock.pumpCycles();
    return report;
}

}  // namespace kitezh
