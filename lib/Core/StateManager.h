#ifndef KLEE_STATEMANAGER_H
#define KLEE_STATEMANAGER_H

#include "Subscriber.h"
#include "ExecutionState.h"

#include <set>
#include <vector>

namespace klee
{
class StateManager
{
private:
  std::vector<Subscriber *> subscribers;
  std::vector<Subscriber *> subscribersAfterAll;

  std::set<ExecutionState*, ExecutionStateIDCompare> states;
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
public:
  StateManager();

  void subscribe(Subscriber *s);
  void subscribeAfterAll(Subscriber *s);
  void unsubscribe(Subscriber *s);

  void copyStatesTo(std::vector<ExecutionState *> &stateList);
  bool emptyStates();
  int sizeStates();
  
  void addState(ExecutionState *state);
  bool removeState(ExecutionState &state);
  void updateStates(ExecutionState *state);

  //переименовать
  const std::set<ExecutionState*, ExecutionStateIDCompare> &getStates();

  ~StateManager();
};
} // namespace klee


#endif /*KLEE_STATEMANAGER_H*/