#ifndef KLEE_OBJECTMANAGER_H
#define KLEE_OBJECTMANAGER_H

#include "ExecutionState.h"
#include "SearcherUtil.h"
#include "ProofObligation.h"
#include "Subscriber.h"
// #include "Database.h"

#include "llvm/IR/Function.h"

#include <set>
#include <vector>

namespace klee {

struct Lemma;

// struct Lemma {
//   Path path;
//   ExprHashSet constraints;

//   bool operator==(const Lemma &other) {
//     return this->path == other.path && this->constraints == other.constraints;
//   }
//   Lemma(const Path &path) : path(path) {}
// };

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
  /// Used to track states that have been added during the current
  /// instructions step.
  /// \invariant \ref addedStates is a subset of \ref states.
  /// \invariant \ref addedStates and \ref removedStates are disjoint.
  std::vector<ExecutionState *> addedStates;
  /// Used to track states that have been removed during the current
  /// instructions step.
  /// \invariant \ref removedStates is a subset of \ref states.
  /// \invariant \ref addedStates and \ref removedStates are disjoint.
  std::vector<ExecutionState *> removedStates;
  ref<TargetedConflict> targetedConflict;

  std::vector<ProofObligation *> pobs;
  // ?
  // std::set<ProofObligations *> pobs
  std::vector<ProofObligation *> addedPobs;
  std::vector<ProofObligation *> removedPobs;

  std::vector<Propagation> propagations;
  // ?
  // std::set<Propagation> propagations;
  std::vector<Propagation> addedPropagations;
  std::vector<Propagation> removedProgations;

  std::set<ExecutionState *, ExecutionStateIDCompare> targetedStates;

  std::map<KBlock *, std::set<Lemma *>> locationMap;
  std::map<Path, std::set<Lemma *>> pathMap;
  std::set<Lemma *> lemmas;

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

  void addPob(ProofObligation *newPob);
  void removePob(ProofObligation *pob);
  std::vector<ProofObligation *> getPobs();
  ExecutionState *initBranch(ref<InitializeAction> action);

  void setTargetedConflict(ref<TargetedConflict> tc);

  void setReachedStates();
  
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

  void summarize(const ProofObligation *pob,
                 const Conflict &conflict,
                 const ExprHashMap<ref<Expr>> &rebuildMap);

  ~ObjectManager();

private:
  void setResult();

  void addStateToPob(ExecutionState *state);
  void addPobToState(ProofObligation *pob);
  void createPropagations();

  bool checkStack(ExecutionState * state, ProofObligation *pob);

  ExecutionState *replayStateFromPob(ProofObligation *pob);
};
} // namespace klee


#endif /*KLEE_OBJECTMANAGER_H*/