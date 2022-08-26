#ifndef KLEE_OBJECTMANAGER_H
#define KLEE_OBJECTMANAGER_H

#include "ExecutionState.h"
#include "SearcherUtil.h"
#include "ProofObligation.h"
#include "Subscriber.h"

#include <set>
#include <vector>

namespace klee {
class ObjectManager {
private:
  std::vector<Subscriber *> subscribers;
  std::vector<Subscriber *> subscribersAfterAll;

  ref<ActionResult> result;
  ExecutionState *initialState;
  ExecutionState *emptyState;

  std::set<ExecutionState *, ExecutionStateIDCompare> states;
  std::set<ExecutionState *, ExecutionStateIDCompare> isolatedStates;
  std::vector<ExecutionState *> addedStates;
  std::vector<ExecutionState *> removedStates;
  std::vector<ProofObligation *> newPobs;
  //ref<TargetedConflict> targetedConflict;
public:
  ObjectManager(/* args */);

  void subscribe(Subscriber *s);
  void subscribeAfterAll(Subscriber *s);
  void unsubscribe(Subscriber *s);

  void setInitialAndEmtySt(ExecutionState *state);
  ExecutionState *getInitialState();
  ExecutionState *getEmptyState();

  void setAction(ref<ForwardAction> action);
  void setAction(ref<ForwardAction> action, ref<TargetedConflict> targetedConflict);
  void setAction(ref<BackwardAction> action);
  void setAction(ref<InitializeAction> action, ExecutionState &state);
  void setAction(ref<TerminateAction> action);

  //временно
  void insertState(ExecutionState *state);
  void setForwardResult(ref<ForwardResult> res);
  
  void addNewPob(ProofObligation *newPob);
  std::vector<ProofObligation *> getPobs();
  //void goBackward(ref<BackwardAction> action, TimerGroup &timers);
  ExecutionState *initBranch(ref<InitializeAction> action);
  //
  
  void addState(ExecutionState *state);
  void addIsolatedState(ExecutionState *state);
  void removeState(ExecutionState *state);

  bool emptyStates();
  bool emptyIsolatedStates();
  std::size_t sizeStates();
  void copyStatesTo(std::vector<ExecutionState *> &statesList);
  const std::set<ExecutionState *, ExecutionStateIDCompare> &getStates();
  const std::set<ExecutionState *, ExecutionStateIDCompare> &getIsolatedStates();

  void updateResult();
  std::vector<ExecutionState *> closeProofObligation(bool replayStateFromProofObligation);
  ExecutionState *replayStateFromPob(ProofObligation *pob);

  void setSearcher();

  ~ObjectManager();
};
} // namespace klee


#endif /*KLEE_OBJECTMANAGER_H*/