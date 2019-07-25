#ifndef GUARD_NEURALNET_SCHEDULER_HPP
#define GUARD_NEURALNET_SCHEDULER_HPP

#include <vector>
#include <popart/names.hpp>

namespace popart {

class Scheduler {

public:
  Scheduler() {}

  // get as much of a schedule as possible. If the Ops with all their
  // ordering constraints form a DAG, this schedule will contain all
  // the Ops
  std::vector<Op *> getPartialOpSchedule(const OpsBeforeKey &,
                                         const Graph &) const;
};

} // namespace popart

#endif