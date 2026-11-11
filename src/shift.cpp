
#include "kitezh/shift.hpp"
#include "kitezh/lock.hpp"
#include "kitezh/geometry.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace kitezh {

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
	ShiftReport report;

	std::vector<Vessel> pending;
	std::vector<std::string> seen;

	for (const Vessel& vessel : queue) {
		const bool duplicate =
			std::find(
				seen.begin(),
				seen.end(),
				vessel.call_sign
			) != seen.end();

		if (duplicate) {
			report.refused.push_back(
				{vessel.call_sign, "duplicate"}
			);
			continue;
		}

		seen.push_back(vessel.call_sign);

		if (validateVessel(vessel).has_value()) {
			report.refused.push_back(
				{vessel.call_sign, "invalid"}
			);
			continue;
		}

		if (vessel.draft_dm > kLowDepthDm) {
			report.refused.push_back(
				{vessel.call_sign, "draft"}
			);
			continue;
		}

		pending.push_back(vessel);
	}

	Lock lock;

	while (!pending.empty()) {
		std::vector<Vessel> batch;

		const Direction dir = pending.front().direction;

		batch.push_back(pending.front());
		pending.erase(pending.begin());

		if (batch.front().kind != VesselKind::Oversized) {
			std::vector<bool> selected(pending.size(), false);

			bool skipped = false;

			for (std::size_t i = 0; i < pending.size(); ++i) {
				const Vessel& cur = pending[i];

				const bool same_dir = cur.direction == dir;
				const bool normal = cur.kind != VesselKind::Oversized;
				const bool fits = same_dir and normal and fitsInChamber(batch,
					cur);

				if (fits and
					(!skipped or cur.kind == VesselKind::Ambulance)) {
					batch.push_back(cur);
					selected[i] = true;
				} else {
					skipped = true;
				}
			}

			std::vector<Vessel> rest;

			for (std::size_t i = 0; i < pending.size(); ++i) {
				if (!selected[i]) {
					rest.push_back(pending[i]);
				}
			}

			pending = rest;
		}

		const Level entry_level =
			dir == Direction::Upstream
				? Level::Low
				: Level::High;

		const Gate entry_gate =
			dir == Direction::Upstream
				? Gate::Lower
				: Gate::Upper;

		const Gate exit_gate =
			dir == Direction::Upstream
				? Gate::Upper
				: Gate::Lower;

		if (lock.level() != entry_level) {
			lock.pump();
		}

		lock.open(entry_gate);

		for (const Vessel& vessel : batch) {
			lock.enter(vessel);
		}

		lock.close(entry_gate);
		lock.pump();
		lock.open(exit_gate);

		for (const Vessel& vessel : batch) {
			lock.exit(vessel.call_sign);
			report.passed.push_back(vessel.call_sign);
		}

		lock.close(exit_gate);
	}

	report.pump_cycles = lock.pumpCycles();

	return report;
}

} // namespace kitezh
