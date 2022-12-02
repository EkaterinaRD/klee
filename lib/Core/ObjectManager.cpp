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
  //llvm::errs() << "add initial state: id: " << initialState->id << "("<< initialState << ")\n";
  emptyState = state->copy();
  emptyState->stack.clear();
  emptyState->isolated = true;
  //llvm::errs() << "add empty state: id: " << emptyState->id << "("<< emptyState << ")\n";
}

void ObjectManager::deleteInitialAndEmptySt() {
  //llvm::errs() << "delete initial state: id: " << initialState->id << "("<< initialState << ")\n";
  delete initialState;
  //llvm::errs() << "delete empty state: id: " << emptyState->id << "("<< emptyState << ")\n";
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
}

//add TargetConflictResult and ...Action

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
    result = new BranchResult(action->state, addedStates, removedStates,
                              addedPropagations, removedProgations);
    break;
  }
  case BidirectionalAction::Kind::ReachedStates: {
    ref<ReachedStatesAction> action = cast<ReachedStatesAction>(_action);
    result = new ReachedStatesResult(addedPropagations, removedProgations);
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
  //llvm::errs() << "add pob: id: " << newPob->id << "(" << newPob << ")\n";
  
  // if (_action->getKind() == BidirectionalAction::Kind::Forward) {
  //   for (auto state : states) {
  //     if (newPob->location == state->pc->parent && checkStack(state, newPob)) {
  //       Propagation prop(state, newPob);
  //       addedPropagations.push_back(prop);
  //     }
  //   }
  // } else if (_action->getKind() == BidirectionalAction::Kind::Branch) {
  //   for (auto istate : isolatedStates) {
  //     if (newPob->location == istate->pc->parent && checkStack(istate, newPob)) {
  //       Propagation prop(istate, newPob);
  //       addedPropagations.push_back(prop);
  //     }
  //   }
  // }
}

void ObjectManager::removePob(ProofObligation *pob) {
  removedPobs.push_back(pob);
  //llvm::errs() << "remove pob: id: " << pob ->id << "(" << pob << ")\n";
  
  for (auto prop : propagations) {
    if (prop.pob == pob) {
      removedProgations.push_back(prop);
      //llvm::errs() << "remove prop: state: " << prop.state->id << " pob: "<< prop.pob->id << "\n";
    }
  }
  
}

std::vector<ProofObligation *> ObjectManager::getPobs() {
  return addedPobs;
}

//переименовать в initiBranchState
ExecutionState *ObjectManager::initBranch(ref<InitializeAction> action) {
  KInstruction *loc = action->location;
  std::set<Target> &targets = action->targets;

  ExecutionState *state = nullptr;
  if (loc == initialState->initPC) {
    state = initialState->copy();
    state->isolated = true;
  } else 
    state = emptyState->withKInstruction(loc); 
  isolatedStates.insert(state);
  for (auto target : targets) {
    state->targets.insert(target);
  }
  //llvm::errs() << "add new isolate state: id: " << state->id << "(" << state << ")\n";

  // ExecutionState *copyState = state->copy();
  // llvm::errs() <<"copy isolate state("<< state->id <<"): id: "<< copyState->id <<"("<< copyState <<")\n";
  // addStateToPob(copyState);

  result = new InitializeResult(loc, *state);
  return state;
}

void ObjectManager::setTargetedConflict(ref<TargetedConflict> tc) {
  targetedConflict = tc;
}
///

//удалить
ExecutionState *ObjectManager::createState(llvm::Function *f, KModule *kmodule) {
  ExecutionState *state = new ExecutionState(kmodule->functionMap[f], kmodule->functionMap[f]->blockMap[&*f->begin()]);
  //llvm::errs() << "add empty state: id: " << emptyState->id << "("<< emptyState << ")\n";
  return state;
}

void ObjectManager::addState(ExecutionState *state) {

  addedStates.push_back(state);
  // if (state->isIsolated()) {
  //   llvm::errs() << "add isolate state: id: " << state->id << "("<< state << ")\n";
  // } else {
  //   llvm::errs() << "add state: id: " << state->id << "("<< state << ")\n";
  // }
}

//rename forkState?
ExecutionState *ObjectManager::branchState(ExecutionState *state) {
  ExecutionState *newState = state->branch();
 
  addedStates.push_back(newState);
  // if (newState->isIsolated()) {
  //   llvm::errs() << "add isolate state: id: " << newState->id << "("<< newState << ")\n";
  // } else {
  //   llvm::errs() << "add state: id: " << newState->id << "("<< newState << ")\n";
  // }

  return newState;
} 

void ObjectManager::removeState(ExecutionState *state) {
  std::vector<ExecutionState *>::iterator itr = 
    std::find(removedStates.begin(), removedStates.end(), state);
  assert(itr == removedStates.end());

  state->pc = state->prevPC;
  removedStates.push_back(state);
  //llvm::errs() << "remove state: id: " << state->id << "("<< state << ")\n";
  
  for (auto prop : propagations) {
    if (prop.state == state) {
      removedProgations.push_back(prop);
      //llvm::errs() << "remove prop: state: " << prop.state->id << " pob: "<< prop.pob->id << "\n";
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

void ObjectManager::addStateToPob(ExecutionState *state) {
  for (auto pob : pobs) {
    if (state->pc->parent == pob->location && checkStack(state, pob)) {
      assert(state->path.getFinalBlock() == pob->path.getInitialBlock() &&
               "Paths are not compatible.");
      Propagation prop(state, pob);
      //llvm::errs() << "add prop: state: " << state->id << " pob: " << pob->id << "\n";
      
      addedPropagations.push_back(prop);
      if (!state->isIsolated()) {
        ++state->backwardStepsLeftCounter;
      }
    }
  }
}

void ObjectManager::addPobToState(ProofObligation *pob) {
  //на самом то деле надо добовлять побы к пропан из состояний, которые достгли таргет
  //и потому думаю надо создать отдельный список таких состояний
  //однако как это извлечь из серчера, который и вычисляет эти ричд состояния
  for (auto state : targetedStates) {
    if (state->pc->parent == pob->location && checkStack(state, pob)) {
      assert(state->path.getFinalBlock() == pob->path.getInitialBlock() &&
               "Paths are not compatible.");
      Propagation prop(state, pob);
      //llvm::errs() << "add prop: state: " << state->id << " pob: " << pob->id << "\n";
      addedPropagations.push_back(prop);
    }
  }
}

void ObjectManager::doSomething() {
  ref<ReachedStatesAction> act = cast<ReachedStatesAction>(_action);
  //по-хорошему сюда asser надобно
  //std::map<Target, std::unordered_set<ExecutionState *>> reached = act->reached;
  std::vector<ExecutionState *> reachedStates = act->reached;

  for (auto state : reachedStates) {
    addStateToPob(state);
  }
    
  //createPropagations();
}

void ObjectManager::createPropagations() {

  switch (_action->getKind()) {
  case BidirectionalAction::Kind::Forward: {
    // ref<ForwardResult> res = cast<ForwardResult>(result);
    ref<ForwardAction> act = cast<ForwardAction>(_action);
    //ExecutionState *current = cast<ForwardAction>(_action)->state;
    addStateToPob(act->state);

    for (auto state : addedStates) {
      addStateToPob(state);
    }
    //возможно лишнее, тк при форвард побы появляются после апдейт
    for (auto pob : addedPobs) {
      addPobToState(pob);
    }
    break;
  }
  // case BidirectionalAction::Kind::Branch: {
  //   if (!addedStates.empty()) {
  //     for (auto state : isolatedStates) {
  //       state = state->copy();
  //       addStateToPob(state);
  //     }
  //   }

  //   break;
  // }
  case BidirectionalAction::Kind::ReachedStates: {
    // ref<ReachedStatesAction> act = cast<ReachedStatesAction>(_action);
    // std::map<Target, std::unordered_set<ExecutionState *>> reached = act->reached;

    // for (auto &targetStates : reached) {
    //   for (auto state : targetStates.second) {
    //     if (targetStates.first.atReturn() && state->stack.size() > 0) {
    //       continue;
    //     }
    //     // ExecutionState *copyState = state->copy();
    //     // addStateToPob(copyState);
        
    //     //add to targetedStates;
    //     addStateToPob(state->copy());
    //   }
    // }

    for (auto state : targetedStates) {
      addStateToPob(state);
    }

    break;
  }
  case BidirectionalAction::Kind::Backward: {
    for (auto pob : addedPobs) {
      addPobToState(pob);
    }
  }
  
  default:
    break;
  }

}

void ObjectManager::updateResult() {
  //props here
  createPropagations();
  setResult();

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
    // propagations.insert(prop);
  }
  for (auto prop : removedProgations) {
    std::vector<Propagation>::iterator it =
      std::find(propagations.begin(), propagations.end(), prop);
    assert(it != propagations.end());
    propagations.erase(it);
  }
  addedPropagations.clear();
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
    if (!addedPobs.empty()) {
      for (auto pob : addedPobs) {
        addPobToState(pob);
      }
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
    //llvm::errs() << "delete pob: id: " << pob->id << "("<< pob << ")\n";
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
  std::vector<Propagation> aProps = {};
  std::vector<Propagation> rProps = {};
  result = new ForwardResult(nullptr, {replayState}, {}, aProps, rProps);
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