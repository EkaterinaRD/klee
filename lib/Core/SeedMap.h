#ifndef KLEE_SEEDMAP_H
#define KLEE_SEEDMAP_H

#include "Subscriber.h"
#include "SeedInfo.h"
#include "ExecutionState.h"

#include <map>
#include <vector>

namespace klee
{
class SeedMap final : public Subscriber {
private:
  std::map<ExecutionState*, std::vector<SeedInfo> > seedMap;
public:
  void update(ExecutionState *current,
                        const std::vector<ExecutionState *> &addedStates,
                        const std::vector<ExecutionState *> &removedStates) override;

  bool empty();
  std::map<ExecutionState*, std::vector<SeedInfo> >::iterator upper_bound(ExecutionState *state);
  std::map<ExecutionState*, std::vector<SeedInfo> >::iterator find(ExecutionState *state);
  std::map<ExecutionState*, std::vector<SeedInfo> >::iterator begin();
  std::map<ExecutionState*, std::vector<SeedInfo> >::iterator end();
  //переименовать
  std::vector<SeedInfo> &at(ExecutionState *state);
  void erase(std::map<ExecutionState*, std::vector<SeedInfo> >::iterator it);
  void erase(ExecutionState *state);
  void push_back(ExecutionState *result, std::vector<SeedInfo>::iterator siit);
  std::size_t count(ExecutionState *state);
};
} // namespace klee

#endif /*KLEE_SEEDMAP_H*/