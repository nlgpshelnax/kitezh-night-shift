#include "kitezh/lock.hpp"

namespace kitezh {

Lock::Lock() : level_(Level::Low), lower_(GateState::Closed),
               upper_(GateState::Closed), cycles_(0) {}

Level Lock::level() const { return level_; }
GateState Lock::gate(Gate gate) const {
    if (gate == Gate::Lower) return lower_;
    return upper_;
}
int Lock::pumpCycles() const { return cycles_; }
std::vector<Vessel> Lock::inside() const { return inside_; }

void Lock::open(Gate gate) {
    if (gate == Gate::Lower) {
        if (level_ != Level::Low || upper_ == GateState::Open) throw LockError("cannot open lower gate");
        lower_ = GateState::Open;
    } else {
        if (level_ != Level::High || lower_ == GateState::Open) throw LockError("cannot open upper gate");
        upper_ = GateState::Open;
    }
}

void Lock::close(Gate gate) {
    if (gate == Gate::Lower) lower_ = GateState::Closed;
    else upper_ = GateState::Closed;
}

void Lock::pump() {
    if (lower_ == GateState::Open || upper_ == GateState::Open) throw LockError("gate is open");
    Level next;
    int depth;
    if (level_ == Level::Low) {
        next = Level::High;
        depth = kHighDepthDm;
    } else {
        next = Level::Low;
        depth = kLowDepthDm;
    }
    for (const Vessel& vessel : inside_) {
        if (vessel.draft_dm > depth) throw LockError("not enough depth");
    }
    level_ = next;
    cycles_++;
}

void Lock::enter(const Vessel& vessel) {
    if (validateVessel(vessel)) throw LockError("invalid vessel");
    for (const Vessel& other : inside_) {
        if (other.call_sign == vessel.call_sign) throw LockError("duplicate vessel");
        if (other.kind == VesselKind::Oversized) throw LockError("oversized vessel inside");
    }
    int depth = kLowDepthDm;
    if (level_ == Level::High) {
        depth = kHighDepthDm;
    }
    if (vessel.draft_dm > depth) throw LockError("not enough depth");
    Gate entrance = Gate::Lower;
    if (vessel.direction == Direction::Downstream) {
        entrance = Gate::Upper;
    }
    if (gate(entrance) != GateState::Open) throw LockError("entrance is closed");
    if (vessel.kind == VesselKind::Oversized && !inside_.empty()) throw LockError("not empty");
    if (!fitsInChamber(inside_, vessel)) throw LockError("not enough room");
    inside_.push_back(vessel);
}

void Lock::exit(const std::string& call_sign) {
    if (inside_.empty() || inside_[0].call_sign != call_sign) throw LockError("wrong vessel");
    Gate exit_gate = Gate::Upper;
    if (inside_[0].direction == Direction::Downstream) {
        exit_gate = Gate::Lower;
    }
    if (gate(exit_gate) != GateState::Open) throw LockError("exit is closed");
    inside_.erase(inside_.begin());
}

}  // namespace kitezh
