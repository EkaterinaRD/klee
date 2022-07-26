#ifndef KLEE_SUBSCRIBER_H
#define KLEE_SUBSCRIBER_H

#include "ExecutionState.h"
#include <vector>

namespace klee
{
class Subscriber
{
public:
  virtual void update(ExecutionState *current,
                        const std::vector<ExecutionState *> &addedStates,
                        const std::vector<ExecutionState *> &removedStates) = 0;
};
} // namespace klee


#endif /*KLEE_SUBSCRIBER_H*/