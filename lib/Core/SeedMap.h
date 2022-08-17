#ifndef KLEE_SEEDMAP_H
#define KLEE_SEEDMAP_H

#include "ExecutionState.h"
#include "SeedInfo.h"

#include <map>

namespace klee {
class SeedMap {
private:
  std::map<ExecutionState *, std::vector<SeedInfo>> seedMap;
public:
  SeedMap(/* args */);

  std::map< ExecutionState*, std::vector<SeedInfo> >::iterator upperBound(ExecutionState *state);
  std::map< ExecutionState*, std::vector<SeedInfo> >::iterator find(ExecutionState *state);
  std::map< ExecutionState*, std::vector<SeedInfo> >::iterator end();
  std::map< ExecutionState*, std::vector<SeedInfo> >::iterator begin();
  void erase(std::map< ExecutionState*, std::vector<SeedInfo> >::iterator it);
  void erase(ExecutionState *state);
  void pushBack(ExecutionState *state, std::vector<SeedInfo>::iterator siit);
  std::size_t count(ExecutionState *state);
  std::vector<SeedInfo> &at(ExecutionState *state);
  bool empty();

  ~SeedMap();
};
} // namespace klee


#endif /*KLEE_SEEDMAP_H*/