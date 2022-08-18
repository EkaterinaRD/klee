#include "ObjectManager.h"

using namespace klee;

/*namespace klee {
cl::OptionCategory ExecCat("Execution option",
                           "This opntions control kind of execution");

cl::opt<bool> ReplayStateFromProofObligation(
    "replay-state-from-pob",
     cl::init(false),
     cl::desc("Replay state from proof obligation (default=false)"),
     cl::cat(ExecCat));
} // namespace klee

*/
ObjectManager::ObjectManager(/* args */) {}

void ObjectManager::subscribe(Subscriber *s) {
  subscribers.push_back(s);
}

void ObjectManager::subscribeAfterAll(Subscriber *s) {
  subscribersAfterAll.push_back(s);
}

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

bool ObjectManager::removeState(ExecutionState *state) {
  std::vector<ExecutionState *>::iterator itr = 
    std::find(removedStates.begin(), removedStates.end(), state);
  if (itr != removedStates.end()) {
    return false;
  }

  state->pc = state->prevPC;
  removedStates.push_back(state);
  return true;
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
  for (auto s : subscribers) {
    s->update(result);
  }
  for (auto s : subscribersAfterAll) {
    s->update(result);
  }

  if (isa<ForwardResult>(result)) {
    ref<ForwardResult> fr = cast<ForwardResult>(result);
    states.insert(fr->addedStates.begin(), fr->addedStates.end());
    for (auto state : fr->removedStates) {
      std::set<ExecutionState *>::iterator it2 = states.find(state);
      assert(it2 != states.end());
      states.erase(it2);
      delete state;
    }
  } else if (isa<BranchResult>(result)) {
    ref<BranchResult> brr = cast<BranchResult>(result);
    isolatedStates.insert(brr->addedStates.begin(), brr->addedStates.end());
    for (auto state : brr->removedStates) {
      std::set<ExecutionState *>::iterator it3 = isolatedStates.find(state);
      assert(it3 != isolatedStates.end());
      isolatedStates.erase(it3);
      delete state;
    }
  }

  addedStates.clear();
  removedStates.clear();
}

void ObjectManager::addRoot(ExecutionState *state) {
  for (auto s : subscribers) {
    s->addRoot(state);
  }
  for (auto s : subscribersAfterAll) {
    s->addRoot(state);
  }
}

void ObjectManager::replayStateFromPob(ProofObligation *pob) {
  assert(pob->location->instructions[0]->inst == emptyState->initPC->inst);

  ExecutionState *replayState = initialState->copy();
  for (const auto &constraint : pob->condition) {
    replayState->addConstraint(constraint, pob->condition.getLocation(constraint));
  }
  for (auto &symbolic : pob->sourcedSymbolics) {
    replayState->symbolics.push_back(symbolic);
  }

  replayState->targets.insert(Target(pob->root->location));
  states.insert(replayState);
  addRoot(replayState);
  result = new ForwardResult(nullptr, {replayState}, {});
  updateResult();
}

void ObjectManager::closeProofObligation(bool replayStateFromProofObligation) {

  if (isa<BackwardResult>(result)) {
    ref<BackwardResult> br = cast<BackwardResult>(result);
    for (auto pob : br->newPobs) {
      if (pob->location->getFirstInstruction() == emptyState->initPC) {
        if (replayStateFromProofObligation) {
          replayStateFromPob(pob);
        }
        for (auto s : subscribers) {
          s->closeProofObligation(pob);
        }
        for (auto s : subscribersAfterAll) {
          s->closeProofObligation(pob);
        }
      }
    }
  }

  /*for (auto s : subscribers) {
          s->closeProofObligation(pob);
        }
        for (auto s : subscribersAfterAll) {
          s->closeProofObligation(pob);
        }*/

  
}

ObjectManager::~ObjectManager() {}