#ifndef KLEE_SEEDMAP_H
#define KLEE_SEEDMAP_H

#include "Subscriber.h"
#include "ExecutionState.h"
#include "SeedInfo.h"
#include "SearcherUtil.h"

#include <map>

namespace klee {
class SeedMap : public Subscriber {
private:
  std::map<ExecutionState *, std::vector<SeedInfo>> seedMap;
public:
  SeedMap(/* args */);

  void update(ref<ActionResult> result) override;
  void closeProofObligation(ProofObligation *pob) override {}
  void addRoot(ExecutionState *state) override {}

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