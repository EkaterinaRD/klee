#include "StateManager.h"

using namespace klee;

StateManager::StateManager(/* args */) {}

void StateManager::subscribe(Subscriber *s) {
  subscribers.push_back(s);
}

void StateManager::unsubscribe(Subscriber *s) {
  auto it = std::find(subscribers.begin(), subscribers.end(), s);
  subscribers.erase(it);
}

void StateManager::insertState(ExecutionState *state) {
  states.insert(state);
}

std::vector<ExecutionState *> StateManager::copyStates() {
  std::vector<ExecutionState *> tmp(states.begin(), states.end());
  return tmp;
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
    /*if (s) {
      s->update(state, addedStates, removedStates);
    }*/
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

const std::set<ExecutionState*, ExecutionStateIDCompare>* StateManager::getStates() {
  return &states;
}

StateManager::~StateManager() {}