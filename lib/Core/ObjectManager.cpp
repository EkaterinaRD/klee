#include "ObjectManager.h"
#include "Composer.h"

using namespace klee;

ObjectManager::ObjectManager(/* args */) {}

void ObjectManager::subscribe(Subscriber *s) {
  subscribers.push_back(s);
  std::vector<ExecutionState *> newStates(states.begin(), states.end());
  std::vector<ExecutionState *> rStates;
  result = new ForwardResult(nullptr, newStates, rStates, addedPropagations, removedProgations);
  s->update(result);
}

void ObjectManager::subscribeAfterAll(Subscriber *s) {
  subscribersAfterAll.push_back(s);
}

void ObjectManager::unsubscribe(Subscriber *s) {
  auto it = std::find(subscribers.begin(), subscribers.end(), s);
  if (it != subscribers.end()) {
    subscribers.erase(it);
  } else {
    auto it2  = std::find(subscribersAfterAll.begin(), subscribersAfterAll.end(), s);
    if (it2 != subscribersAfterAll.end()) {
      subscribersAfterAll.erase(it2);
    }
  }
}

void ObjectManager::setInitialAndEmtySt(ExecutionState *state) {
  initialState = state->copy();
  emptyState = state->copy();
  emptyState->stack.clear();
  emptyState->isolated = true;
}

void ObjectManager::deleteInitialAndEmptySt() {
  delete initialState;
  delete emptyState;
}

ExecutionState *ObjectManager::getInitialState() {
  return initialState;
}

ExecutionState *ObjectManager::getEmptyState() {
  return emptyState;
}

void ObjectManager::setAction(ref<BidirectionalAction> action) {
  
  _action = action;
  if (action->getKind() == BidirectionalAction::Kind::Branch) {
    llvm::errs() << "state id: " << cast<BranchAction>(action)->state->id << "\n";
    llvm::errs() << "\n";
   }
}

void ObjectManager::setResult() {
  switch (_action->getKind()) {
  case BidirectionalAction::Kind::Forward: {
    ref<ForwardAction> action = cast<ForwardAction>(_action);
    result = new ForwardResult(action->state, addedStates, removedStates, 
                               addedPropagations, removedProgations,
                               targetedConflict);
    break;
  }
  case BidirectionalAction::Kind::Branch: {
    ref<BranchAction> action = cast<BranchAction>(_action);
    llvm::errs() << "branch action->state \n";
    llvm::errs() << "state id: " << action->state->id << "\n";
    llvm::errs() << "      addres: " << action->state <<  "\n";
    result = new BranchResult(action->state, addedStates, removedStates,
                              addedPropagations, removedProgations);
    break;
  }
  case BidirectionalAction::Kind::Backward: {
    ref<BackwardAction> action = cast<BackwardAction>(_action);
    result = new BackwardResult(addedPobs, action->state, action->pob,
                                addedPropagations, removedProgations);
    break;
  }
  case BidirectionalAction::Kind::Terminate: {
    result = new TerminateResult();
    break;
  }
  default:
    //initialize
    break;
  }
}
///

void ObjectManager::addPob(ProofObligation *newPob) {
  addedPobs.push_back(newPob);
  for (auto state : states) {
    if (newPob->location == state->pc->parent && checkStack(state, newPob)) {
      //Propagation p = new Propagation(state, newPob);
      Propagation prop(state, newPob);
      addedPropagations.push_back(prop);
    }
  }
}

void ObjectManager::removePob(ProofObligation *pob) {
  removedPobs.push_back(pob);
  for (auto prop : propagations) {
    /*if (p.pob == pob) {
      removedProgations.push_back(p);
    }*/
    if (prop.pob == pob) {
      removedProgations.push_back(prop);
    }
  }
  
}

std::vector<ProofObligation *> ObjectManager::getPobs() {
  return addedPobs;
}

ExecutionState *ObjectManager::initBranch(ref<InitializeAction> action) {
  KInstruction *loc = action->location;
  std::set<Target> &targets = action->targets;

  ExecutionState *state = nullptr;
  if (loc == initialState->initPC) {
    state = initialState->copy();
    state->isolated = true;
  } else {
    llvm::errs() << "empty state: id: " << emptyState->id <<" (" << emptyState <<")"<< " \n";
    state = emptyState->withKInstruction(loc);
    llvm::errs() << "init branch state: id: " << state->id <<" (" << state <<")"<< " \n";
  }
  isolatedStates.insert(state);
  for (auto target : targets) {
    state->targets.insert(target);
  }
  result = new InitializeResult(loc, *state);
  //возможно здесь нужен update result
  return state;
}

void ObjectManager::setTargetedConflict(ref<TargetedConflict> tc) {
  targetedConflict = tc;
}
///

ExecutionState *ObjectManager::createState(llvm::Function *f, KModule *kmodule) {
  ExecutionState *state = new ExecutionState(kmodule->functionMap[f], kmodule->functionMap[f]->blockMap[&*f->begin()]);
  return state;
}

void ObjectManager::addState(ExecutionState *state) {
  llvm::errs() << "add state: id: " << state->id <<" (" << state <<")"<< " \n";

  addedStates.push_back(state);
  
  for (auto pob : pobs) {
    if (state->pc->parent == pob->location && checkStack(state, pob)) {
      Propagation prop(state, pob);
      addedPropagations.push_back(prop);
    }
  }
}

ExecutionState *ObjectManager::branchState(ExecutionState *state) {
  ExecutionState *newState = state->branch();
 
  llvm::errs() << "add branch state: id: " << newState->id <<" (" << newState <<")"<< " \n";
  
  addedStates.push_back(newState);
  return newState;
} 

void ObjectManager::removeState(ExecutionState *state) {
  std::vector<ExecutionState *>::iterator itr = 
    std::find(removedStates.begin(), removedStates.end(), state);
  assert(itr == removedStates.end());

  state->pc = state->prevPC;
  llvm::errs() << "remove state: id: " << state->id <<" (" << state <<")"<< " \n";
  removedStates.push_back(state);
  
  for (auto prop : propagations) {
    /*if (prop.state == state) {
      //removedPropagations.push_back(prop);
      removedProgations.push_back(prop);
    }*/
    if (prop.state == state) {
      removedProgations.push_back(prop);
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
  setResult();

  //checkStack();

  //update subscribers
  for (auto s : subscribers) {
    s->update(result);
  }
  for (auto s : subscribersAfterAll) {
    s->update(result);
  }

  //update propagations
  for (auto prop : addedPropagations){
    propagations.push_back(prop);
  }
  for (auto prop : removedProgations) {
    std::vector<Propagation>::iterator it =
      std::find(propagations.begin(), propagations.end(), prop);
    assert(it != propagations.end());
    propagations.erase(it);
  }
  addedPropagations.clear();
  //removedPropagations.clear();
  removedProgations.clear();

  //update states
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


  //update pobs
  for (auto pob : addedPobs) {
    pobs.push_back(pob);
  }
  //pobs.erase(pob)
  for (auto pob : removedPobs) {
    pob->detachParent();
    delete pob;
  }

  addedPobs.clear();
  removedPobs.clear();
}

ExecutionState *ObjectManager::replayStateFromPob(ProofObligation *pob) {
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
  result = new ForwardResult(nullptr, {replayState}, {}, {}, {});
  updateResult();
  return replayState;
}

std::vector<ExecutionState *> ObjectManager::closeProofObligation(bool replayStateFromProofObligation) {

  std::vector<ExecutionState *> replayStates;
  if (isa<BackwardResult>(result)) {
    ref<BackwardResult> br = cast<BackwardResult>(result);
    for (auto pob : br->newPobs) {
      if (pob->location->getFirstInstruction() == emptyState->initPC) {
        if (replayStateFromProofObligation) {
          replayStates.push_back(replayStateFromPob(pob));
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
  return replayStates;
}


bool ObjectManager::checkStack(ExecutionState *state, ProofObligation *pob) {
  if (state->stack.size() == 0) {
    return true;
  }

  size_t range = std::min(state->stack.size() - 1, pob->stack.size());
  auto stateIt = state->stack.rbegin();
  auto pobIt = pob->stack.rbegin();

  for (size_t i = 0; i < range; ++i) {
    KInstruction *stateInstr = stateIt->caller;
    KInstruction *pobInstr = *pobIt;
    if (stateInstr != pobInstr) {
      return false;
    }
    stateIt++;
    pobIt++;
  }
  return true;
}

ObjectManager::~ObjectManager() {
  pobs.clear();
  propagations.clear();
}