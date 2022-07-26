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

  std::set<ExecutionState*, ExecutionStateIDCompare> states;
  std::vector<ExecutionState *> addedStates;
  std::vector<ExecutionState *> removedStates;
public:
  StateManager(/* args */);

  void subscribe(Subscriber *s);
  void unsubscribe(Subscriber *s);

  void insertState(ExecutionState *state);
  std::vector<ExecutionState *> copyStates();
  bool emptyStates();
  int sizeStates();
  
  void addState(ExecutionState *state);
  bool removeState(ExecutionState &state);
  void updateStates(ExecutionState *state);

  //переименовать
  const std::set<ExecutionState*, ExecutionStateIDCompare>* getStates();

  ~StateManager();
};
} // namespace klee


#endif /*KLEE_STATEMANAGER_H*/