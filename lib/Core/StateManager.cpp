#include "StateManager.h"

using namespace klee;

StateManager::StateManager() {}

void StateManager::subscribe(Subscriber *s) {
  subscribers.push_back(s);
}

void StateManager::subscribeAfterAll(Subscriber *s) {
  subscribersAfterAll.push_back(s);
}

void StateManager::unsubscribe(Subscriber *s) {
  auto it = std::find(subscribers.begin(), subscribers.end(), s);
  if (it != subscribers.end()) {
    subscribers.erase(it);
  } else {
    auto it2 = std::find(subscribersAfterAll.begin(), subscribersAfterAll.end(), s);
    if (it2 != subscribersAfterAll.end()) {
      subscribersAfterAll.erase(it2);
    }
  }
}

void StateManager::copyStatesTo(std::vector<ExecutionState *> &stateList) {
  stateList.insert(stateList.begin(), states.begin(), states.end());
}

bool StateManager::emptyStates() {
  return states.empty();
}

int StateManager::sizeStates() {
  return states.size();
}

void StateManager::addState(ExecutionState *state) {
    addedStates.push_back(state);
}

bool StateManager::removeState(ExecutionState &state) {
  std::vector<ExecutionState *>::iterator itr =
      std::find(removedStates.begin(), removedStates.end(), &state);

  if (itr != removedStates.end()) {
      return false;
  }
  state.pc = state.prevPC;
  removedStates.push_back(&state);

  return true;
}

void StateManager::updateStates(ExecutionState *state) {
  for (auto s : subscribers) {
    s->update(state, addedStates, removedStates);
  }

  for (auto s : subscribersAfterAll) {
    s->update(state, addedStates, removedStates);
  }

  states.insert(addedStates.begin(), addedStates.end());

  for (std::vector<ExecutionState *>::iterator it = removedStates.begin(),
                                               ie = removedStates.end();
        it != ie; ++it) {
      ExecutionState *es = *it;
      std::set<ExecutionState*>::iterator it2 = states.find(es);
      assert(it2!=states.end());
      states.erase(it2);
      delete es;
    }

    addedStates.clear();
    removedStates.clear();
}

const std::set<ExecutionState*, ExecutionStateIDCompare> &StateManager::getStates() {
  return states;
}

void StateManager::setSearcher() {
  std::vector<ExecutionState *> newStates(states.begin(), states.end());
  //updateStates(nullptr, newStates, {});
  for (auto s : subscribers) {
    s->update(nullptr, newStates, {});
  }
  for (auto s : subscribersAfterAll) {
    s->update(nullptr, newStates, {});
  }
}

StateManager::~StateManager() {}