#include "kitezh/shift.hpp"
#include "kitezh/lock.hpp"

namespace kitezh {

ShiftReport runNightShift(const std::vector<Vessel>& queue) {
    ShiftReport report;
    std::vector<std::string> seen;
    std::vector<Vessel> waiting;
    Lock lock;

    for (const Vessel& vessel : queue) {
        bool duplicate = false;
        for (const std::string& name : seen) {
            if (name == vessel.call_sign) duplicate = true;
        }
        seen.push_back(vessel.call_sign);
        if (duplicate) report.refused.push_back({vessel.call_sign, "duplicate"});
        else if (validateVessel(vessel)) report.refused.push_back({vessel.call_sign, "invalid"});
        else if (vessel.draft_dm > kLowDepthDm) report.refused.push_back({vessel.call_sign, "draft"});
        else waiting.push_back(vessel);
    }

    while (!waiting.empty()) {
        std::vector<Vessel> batch;
        batch.push_back(waiting[0]);
        waiting.erase(waiting.begin());
        bool skipped = false;
        if (batch[0].kind != VesselKind::Oversized) {
            std::size_t i = 0;
            while (i < waiting.size()) {
                Vessel vessel = waiting[i];
                if (vessel.direction == batch[0].direction &&
                    vessel.kind != VesselKind::Oversized &&
                    fitsInChamber(batch, vessel) &&
                    (!skipped || vessel.kind == VesselKind::Ambulance)) {
                    batch.push_back(vessel);
                    waiting.erase(waiting.begin() + i);
                } else {
                    skipped = true;
                    i++;
                }
            }
        }

        bool up = batch[0].direction == Direction::Upstream;
        Level entrance_level = Level::Low;
        Gate entrance = Gate::Lower;
        Gate exit_gate = Gate::Upper;
        if (!up) {
            entrance_level = Level::High;
            entrance = Gate::Upper;
            exit_gate = Gate::Lower;
        }
        if (lock.level() != entrance_level) lock.pump();
        lock.open(entrance);
        for (const Vessel& vessel : batch) lock.enter(vessel);
        lock.close(entrance);
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

}  // namespace kitezh
