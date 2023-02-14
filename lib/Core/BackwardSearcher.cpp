#include "BackwardSearcher.h"
#include "ExecutionState.h"
#include "SearcherUtil.h"
#include "klee/Module/KInstruction.h"
#include <algorithm>
#include <climits>
#include <cstddef>
#include <utility>

namespace klee {

RecencyRankedSearcher::RecencyRankedSearcher(unsigned _maxPropagation)
    : maxPropagations(_maxPropagation) {}

bool RecencyRankedSearcher::empty() { return propagatePobToStates.empty(); }

std::pair<ProofObligation *, ExecutionState *>
RecencyRankedSearcher::selectAction() {
  auto &pobStates = *propagatePobToStates.begin();
  ProofObligation *pob = pobStates.first;
  auto &states = pobStates.second;
  Target t(pob->location);
  unsigned leastUsedCount = UINT_MAX;
  ExecutionState *leastUsedState = nullptr;
  for (ExecutionState *state : states) {
    if (pob->propagationCount[state] < leastUsedCount) {
      leastUsedCount = pob->propagationCount[state];
      leastUsedState = state;
    }
  }
  assert(leastUsedState);
  states.erase(leastUsedState);
  if (states.empty())
    propagatePobToStates.erase(pob);
  return std::make_pair(pob, leastUsedState);
}

void RecencyRankedSearcher::updatePropagations(std::vector<Propagation> &addedPropagations,
                                               std::vector<Propagation> &removedPropagations) {
  for (auto aprop : addedPropagations) {
    propagatePobToStates[aprop.pob].insert(aprop.state);
  }
  for (auto rprop : removedPropagations) {
    propagatePobToStates[rprop.pob].erase(rprop.state);
    bool propIsNull = propagatePobToStates[rprop.pob].empty();
    if(propIsNull)
      propagatePobToStates.erase(rprop.pob);
  }
}

void RecencyRankedSearcher::removePob(ProofObligation *pob) {
  propagatePobToStates.erase(pob);
}

}; // namespace klee
