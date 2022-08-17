#include "ObjectManager.h"

using namespace klee;

ObjectManager::ObjectManager(/* args */) {}

void ObjectManager::setInitialAndEmtySt(ExecutionState *state) {
  initialState = state->copy();
  emptyState = state->copy();
  emptyState->stack.clear();
  emptyState->isolated = true;
}

void ObjectManager::setAction(ref<ForwardAction> action, ref<TargetedConflict> targerConflict) {
  result = new ForwardResult(action->state, addedStates, removedStates, targerConflict);
}

void ObjectManager::setAction(ref<ForwardAction> action) {
  switch (action->getKind()) {
  case BidirectionalAction::Kind::Forward:
    result = new ForwardResult(action->state, addedStates, removedStates);
    break;
  case BidirectionalAction::Kind::Branch:
    result = new BranchResult(action->state, addedStates, removedStates);
  default:
    break;
  }
}

void ObjectManager::setAction(ref<BackwardAction> action, std::vector<ProofObligation *> newPobs) {
  result = new BackwardResult(newPobs, action->pob);
} 

void ObjectManager::setAction(ref<InitializeAction> action, ExecutionState &state) {
  result = new InitializeResult(action->location, state);
}

void ObjectManager::setAction(ref<TerminateAction> action) {
  result = new TerminateResult();
}

///

void ObjectManager::insertState(ExecutionState *state) {
  states.insert(state);
}

void ObjectManager::setForwardResult(ref<ForwardResult> res) {
  result = new ForwardResult(res->current, res->addedStates, res->removedStates);
}

///

void ObjectManager::addState(ExecutionState *state) {
  addedStates.push_back(state);
}

void ObjectManager::addIsolatedState(ExecutionState *state) {
  isolatedStates.insert(state);
}

void ObjectManager::removeState(ExecutionState *state) {
  std::vector<ExecutionState *>::iterator itr = 
    std::find(removedStates.begin(), removedStates.end(), state);
  if (itr == removedStates.end()) {
    std::vector<ExecutionState *>::iterator ita = 
      std::find(addedStates.begin(), addedStates.end(), state);
    if (ita == addedStates.end()) {
      state->pc = state->prevPC;
      removedStates.push_back(state);
    } else {
      addedStates.erase(ita);
    }
  }
}

bool ObjectManager::emptyStates() {
  return states.empty();
}

bool ObjectManager::emptyIsolatedStates() {
  return isolatedStates.empty();
}

std::size_t ObjectManager::sizeStates() {
  return states.size();
}

void ObjectManager::copyStatesTo(std::vector<ExecutionState *> &statesList) {
  statesList.insert(statesList.begin(), states.begin(), states.end());
}

const std::set<ExecutionState *, ExecutionStateIDCompare> &ObjectManager::getStates() {
  return states;
}

const std::set<ExecutionState *, ExecutionStateIDCompare> &ObjectManager::getIsolatedStates() {
 return isolatedStates;
}

void ObjectManager::updateResult() {
  if (isa<ForwardResult>(result)) {
    ref<ForwardResult> fr = cast<ForwardResult>(result);
    states.insert(fr->addedStates.begin(), fr->addedStates.end());
    for (auto state : fr->removedStates) {
      std::set<ExecutionState *>::iterator it2 = states.find(state);
      assert(it2 != states.end());
      states.erase(it2);
      //delete state;
    }
  } else if (isa<BranchResult>(result)) {
    ref<BranchResult> brr = cast<BranchResult>(result);
    isolatedStates.insert(brr->addedStates.begin(), brr->addedStates.end());
    for (auto state : brr->removedStates) {
      std::set<ExecutionState *>::iterator it3 = isolatedStates.find(state);
      assert(it3 != isolatedStates.end());
      isolatedStates.erase(it3);
      //delete state;
    }
  }

  addedStates.clear();
  removedStates.clear();
}

ObjectManager::~ObjectManager() {}