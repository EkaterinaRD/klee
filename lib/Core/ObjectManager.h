#ifndef KLEE_OBJECTMANAGER_H
#define KLEE_OBJECTMANAGER_H

#include "ExecutionState.h"
#include "SearcherUtil.h"
#include "ProofObligation.h"
#include "Subscriber.h"
#include "llvm/IR/Function.h"

#include <set>
#include <vector>

namespace klee {


class ObjectManager {
private:
  std::vector<Subscriber *> subscribers;
  std::vector<Subscriber *> subscribersAfterAll;

  ref<BidirectionalAction> _action;
  ref<ActionResult> result;
  ExecutionState *initialState;
  ExecutionState *emptyState;

  std::set<ExecutionState *, ExecutionStateIDCompare> states;
  std::set<ExecutionState *, ExecutionStateIDCompare> isolatedStates;
  std::vector<ExecutionState *> addedStates;
  std::vector<ExecutionState *> removedStates;
  ref<TargetedConflict> targetedConflict;

  std::vector<ProofObligation *> pobs;
  std::vector<ProofObligation *> addedPobs;
  std::vector<ProofObligation *> removedPobs;

  std::vector<Propagation> propagations;
  std::vector<Propagation> addedPropagations;
  std::vector<Propagation> removedProgations;
  //std::map<Target, std::unordered_set<ExecutionState *>> mapTargetToStates;
  std::set<ExecutionState *, ExecutionStateIDCompare> targetedStates;
public:
  ObjectManager(/* args */);

  void subscribe(Subscriber *s);
  void subscribeAfterAll(Subscriber *s);
  void unsubscribe(Subscriber *s);

  void setInitialAndEmtySt(ExecutionState *state);
  void deleteInitialAndEmptySt();
  ExecutionState *getInitialState();
  ExecutionState *getEmptyState();

  void setAction(ref<BidirectionalAction> action);
  void setResult();

  //временно
  void addPob(ProofObligation *newPob);
  void removePob(ProofObligation *pob);
  std::vector<ProofObligation *> getPobs();
  ExecutionState *initBranch(ref<InitializeAction> action);

  void setTargetedConflict(ref<TargetedConflict> tc);

  void addStateToPob(ExecutionState *state);
  void addPobToState(ProofObligation *pob);
  // void addPobToState(ProofObligation *pob);
  //
  void createPropagations();
  void doSomething();
  
  ExecutionState *createState(llvm::Function *f, KModule *kmodule);
  void addState(ExecutionState *state);
  ExecutionState *branchState(ExecutionState *state);
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

  //void setSearcher();
  bool checkStack(ExecutionState * state, ProofObligation *pob);

  ~ObjectManager();
};
} // namespace klee


#endif /*KLEE_OBJECTMANAGER_H*/