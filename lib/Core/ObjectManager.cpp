#include "ObjectManager.h"
#include "Composer.h"
#include "CoreStats.h"
#include "klee/Expr/ExprBuilder.h"
#include "klee/Expr/ExprUtil.h"
#include "klee/Support/ErrorHandling.h"
#include "klee/Support/OptionCategories.h"
#include "llvm/Support/MemoryBuffer.h"

using namespace klee;

namespace {
llvm::cl::opt<bool> DebugSummary(
    "debug-summary",
    llvm::cl::desc(""),
    llvm::cl::init(false),
    llvm::cl::cat(klee::DebugCat));
}

#ifndef divider
#define divider(n) std::string(n, '-') + "\n"
#endif

ObjectManager::ObjectManager() {}

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

void ObjectManager::addPob(ProofObligation *newPob) {
  addedPobs.push_back(newPob);
  // db->pob_write(newPob);
  storePob(newPob);
  if (newPob->id > maxIdPob) {
    maxIdPob = newPob->id;
  }
}

void ObjectManager::removePob(ProofObligation *pob) {
  removedPobs.push_back(pob);
  
  for (auto prop : propagations) {
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
  } else 
    state = emptyState->withKInstruction(loc); 
  isolatedStates.insert(state);
  for (auto target : targets) {
    state->targets.insert(target);
  }

  result = new InitializeResult(loc, *state);
  return state;
}

void ObjectManager::setTargetedConflict(ref<TargetedConflict> tc) {
  targetedConflict = tc;
}

void ObjectManager::setMaxIdState(std::uint32_t newMaxId) {
  if (newMaxId > maxIdState) {
    maxIdState = newMaxId;
  }
}

void ObjectManager::addState(ExecutionState *state) {
  addedStates.push_back(state);
  setMaxIdState(state->getID());
}

ExecutionState *ObjectManager::branchState(ExecutionState *state) {
  ExecutionState *newState = state->branch();
 
  addedStates.push_back(newState);
  setMaxIdState(state->getID());

  return newState;
} 

void ObjectManager::removeState(ExecutionState *state) {
  std::vector<ExecutionState *>::iterator itr = 
    std::find(removedStates.begin(), removedStates.end(), state);
  assert(itr == removedStates.end());

  state->pc = state->prevPC;
  removedStates.push_back(state);
  
  for (auto prop : propagations) {
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

void ObjectManager::addStateToPob(ExecutionState *state) {
  for (auto pob : pobs) {
    if (state->pc->parent == pob->location && checkStack(state, pob)) {
      assert(state->path.getFinalBlock() == pob->path.getInitialBlock() &&
               "Paths are not compatible.");
      Propagation prop(state, pob);
      
      addedPropagations.push_back(prop);
      if (!state->isIsolated()) {
        ++state->backwardStepsLeftCounter;
      }
    }
  }
}

void ObjectManager::addPobToState(ProofObligation *pob) {
  for (auto state : targetedStates) {
    if (state->pc->parent == pob->location && checkStack(state, pob)) {
      assert(state->path.getFinalBlock() == pob->path.getInitialBlock() &&
               "Paths are not compatible.");
      Propagation prop(state, pob);
      addedPropagations.push_back(prop);
    }
  }
}

void ObjectManager::setReachedStates() {
  ref<ReachedStatesAction> act = cast<ReachedStatesAction>(_action);
  std::vector<ExecutionState *> reachedStates = act->reached;

  for (auto state : reachedStates) {
    saveState(*state, true);
    addStateToPob(state);
  }
}

void ObjectManager::createPropagations() {

  switch (_action->getKind()) {
  case BidirectionalAction::Kind::Forward: {

    for (auto state : addedStates) {
      addStateToPob(state);
    }
    for (auto pob : addedPobs) {
      addPobToState(pob);
    }
    break;
  }
  case BidirectionalAction::Kind::ReachedStates: {
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
  //create propagations
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
  for (auto pob : removedPobs) {
    pob->detachParent();
    delete pob;
  }

  addedPobs.clear();
  removedPobs.clear();
}

void ObjectManager::summarize(const ProofObligation *pob,
                              const Conflict &conflict,
                              const ExprHashMap<ref<Expr>> &rebuildMap) {
  std::string label;
  llvm::raw_string_ostream label_stream(label);

  label_stream << "Add lemma for pob at: " << pob->location->getIRLocation() << (pob->atReturn() ? "(at return)" : "") << "\n";

  label_stream << "Pob at "
                 << (pob->atReturn()
                         ? pob->location->getFirstInstruction()->getSourceLocation()
                         : pob->location->getLastInstruction()->getSourceLocation())
                 << "\n";

  const Conflict::core_ty &core = conflict.core;
  const Path &path = conflict.path;
  auto &locationLemmas = locationMap[pob->location];
  if(locationLemmas.empty()) {
    ++stats::summarizedLocationCount;
  }
  Lemma *newLemma = new Lemma(path);
  label_stream << "Constraints are:\n";

  for (auto &constraint : core) {
    ref<Expr> condition = constraint.first;
    if (rebuildMap.count(condition)) {
      ref<Expr> lemmaExpr = Expr::createIsZero(rebuildMap.at(condition));
      newLemma->constraints.insert(lemmaExpr);
      label_stream << lemmaExpr->toString() << "\n";
    }
  }

  label_stream << "State Path is:\n";
  label_stream << path.toString() << "\n";
  label_stream << "Pob Path is:\n";
  label_stream << pob->path.toString() << "\n";
  label_stream << "\n";

  (*summaryFile) << label_stream.str();

  bool exists = false;

  for (auto lemma : pathMap[path]) {
    if (*lemma == *newLemma) {
      exists = true;
      break;
    }
  }

  if (!exists) {
    pathMap[path].insert(newLemma);
    locationMap[path.getFinalBlock()].insert(newLemma);
    lemmas.insert(newLemma);

    if (DebugSummary) {
      llvm::errs() << label_stream.str();
      llvm::errs() << "Summary for pob at " << pob->location->getIRLocation() << "\n";
      llvm::errs() << "Paths:\n";
      llvm::errs() << newLemma->path.toString() << "\n";

      ExprHashSet summary;
      for (auto &expr : newLemma->constraints) {
        summary.insert(expr);
      }
      llvm::errs() << "Lemma:\n";
      llvm::errs() << "\n" << divider(30);
      for (auto &expr : summary) {
        llvm::errs() << divider(30);
        llvm::errs() << expr << "\n";
        llvm::errs() << divider(30);
      }
      llvm::errs() << divider(30);
      llvm::errs() << "\n";
    }
  } else {
    delete newLemma;
  }

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

void ObjectManager::storePob(ProofObligation *pob) {
  std::set<KFunction *> functions = pob->path.getFunctionsInPath();
  for (auto f : functions) {
    db->functionhash_write(std::string(f->function->getName()), module->functionHash(f));
  }
  db->pob_write(pob);
  for (auto e : pob->condition) {
    if (!exprDBMap.count(e))
      exprDBMap[e] = db->expr_write(e);
    assert(exprDBMap.count(e) && "No expr in DB");
    auto instr = pob->condition.getLocation(e);
    std::string str_instr = instr->parent->parent->function->getName().str();
    str_instr += " ";
    str_instr += std::to_string(instr->dest);
    // str_instr += instr->parent->parent->inst2reg[instr];
    db->pobsConstr_write(pob->id, exprDBMap[e], str_instr);
    storeArray(e);
  }
}

void ObjectManager::saveState(ExecutionState &state, bool terminated) {
  state.setNode(terminated);
  statesDB.push_back(state.node);
}

void ObjectManager::storeLemmas() {
  
  for (auto lemma : lemmas) {
    if(!lemmaDBMap.count(lemma)) {
      
      std::set<KFunction *> functions = lemma->path.getFunctionsInPath();
      for (auto f : functions) {
        db->functionhash_write(std::string(f->function->getName()),
                                module->functionHash(f));
      }
      lemmaDBMap[lemma] = db->lemma_write(lemma->path);
      uint64_t idLemma = lemmaDBMap[lemma];

      for (auto constraint : lemma->constraints) {

        if (!exprDBMap.count(constraint)) 
          exprDBMap[constraint] = db->expr_write(constraint);
        uint64_t idConstr = exprDBMap[constraint];
        assert(lemmaDBMap.count(lemma) && "No lemma in DB");
        assert(exprDBMap.count(constraint) && "No expr in DB");
        db->constraint_write(idConstr, idLemma);

        // std::vector<const Array *> arrays;
        // klee::findSymbolicObjects(constraint, arrays);
        // for (auto array : arrays) {
        //   if (!arrayDBMap.count(array))
        //     arrayDBMap[array] = db->array_write(array);
        //   uint64_t idArray = arrayDBMap[array];
        //   assert(exprDBMap.count(constraint) && "No expr in DB");
        //   assert(arrayDBMap.count(array) && "No array in DB");
        //   db->arraymap_write(idArray, idConstr);
        // }
        // for (auto array : arrays) {
        //   assert(arrayDBMap.count(array) && "No child in DB");
        //   uint64_t idChild = arrayDBMap.count(array);
        //   for (auto parent : array->parents) {
        //     assert(arrayDBMap.count(parent) && "No parent in DB");
        //     uint64_t idParent = arrayDBMap.count(parent);
        //     db->parent_write(idChild, idParent);
        //   }
        // }

        storeArray(constraint);


      }
    }
  }
}

void ObjectManager::storeArray(ref<Expr> e) {
  uint64_t idConstr = exprDBMap[e];
  std::vector<const Array *> arrays;
  klee::findSymbolicObjects(e, arrays);
  for (auto array : arrays) {
    if (!arrayDBMap.count(array))
      arrayDBMap[array] = db->array_write(array);
    uint64_t idArray = arrayDBMap[array];
    assert(exprDBMap.count(e) && "No expr in DB");
    assert(arrayDBMap.count(array) && "No array in DB");
    db->arraymap_write(idArray, idConstr);
  }
  for (auto array : arrays) {
    assert(arrayDBMap.count(array) && "No child in DB");
    // uint64_t idChild = arrayDBMap.count(array);
    uint64_t idChild = arrayDBMap[array];
    for (auto parent : array->parents) {
      assert(arrayDBMap.count(parent) && "No parent in DB");
      // uint64_t idParent = arrayDBMap.count(parent);
      uint64_t idParent = arrayDBMap[parent];
      db->parent_write(idChild, idParent);
    }
  }
}

void ObjectManager::makeArray(const std::map<uint64_t, std::string> &arrays,
                              uint64_t id) {
  if (arrayReverseDBMap.count(id))
    return;

  for (auto parent_id : arrayParentMap[id]) {
    makeArray(arrays, parent_id);
  }
  auto buf = llvm::MemoryBuffer::getMemBuffer(arrays.at(id));
  parser->ResetLexer(buf.get());
  auto array = parser->ParseSingleArray();
  arrayDBMap[array] = id;
  arrayReverseDBMap[id] = array;
}

void ObjectManager::makeExprs(const std::map<uint64_t, std::string> &exprs) {
  for (auto expr_pair : exprs) {
    auto buf = llvm::MemoryBuffer::getMemBuffer(expr_pair.second);
    parser->ResetLexer(buf.get());
    auto expr = parser->ParseSingleExpr();
    exprDBMap[expr] = expr_pair.first;
    exprReverseDBMap[expr_pair.first] = expr; 
  }
}

void ObjectManager::loadLemmas() {
  // builder = createDefaultExprBuilder();
  // parser = expr::Parser::Create("DBParser", 
  //                               llvm::MemoryBuffer::getMemBuffer("").get(),
  //                               builder, arrayCache, false);


  // auto arrays = db->arrays_retrieve();
  // auto parents = db->parents_retrieve();
  // for (const auto &parent : parents) {
  //   arrayParentMap[parent.first].insert(parent.second);
  // }
  // for (const auto &array : arrays) {
  //   makeArray(arrays, array.first);
  // }

  // auto exprs = db->exprs_retrieve();
  // makeExprs(exprs);

  auto DBLemmas = db->lemmas_retrieve();
  // auto DBHashMap = db->functionhash_retrieve();
  for (const auto &lemma : DBLemmas) {
    ref<Path> path = parse(lemma.second.path, module, DBHashMap);
    if (!path) {
      db->lemma_delete(lemma.first);
      continue;
    }

    Lemma *l = new Lemma(*path);
    for (auto expr_id : lemma.second.exprs) {
      l->constraints.insert(exprReverseDBMap[expr_id]);
    }
    pathMap[l->path].insert(l);
    locationMap[l->path.getFinalBlock()].insert(l);
    lemmas.insert(l);
    lemmaDBMap[l] = lemma.first;
  }

  for (const auto &hash : DBHashMap) {
    if (!module->functionNameMap.count(hash.first) || 
        module->functionHash(module->functionNameMap[hash.first]) != 
        DBHashMap[hash.first]) {
      
      db->hash_delete(hash.first);
    }
  }

  db->exprs_purge();
  db->arrays_purge();

  // delete parser;
  // delete builder;
}

void ObjectManager::loadStates(ExecutionState *startState) {
  auto DBStates = db->states_retrieve();
  for (auto node : DBStates) {
    
    ExecutionState *newState;
    
    if (node.first == 0) {
        newState = startState;
        newState->setMaxID(maxIdState);
    } else {
        newState = new ExecutionState(node.first, maxIdState);
        newState->node.state_id = newState->getID();

        auto initLoc = parseInstruction(node.second.initLoc, module, DBHashMap);
        newState->initPC = initLoc->parent->instructions;
        while (newState->initPC != initLoc) {
          ++newState->initPC;
        }
    }

    newState->node.initPC = newState->initPC;

    auto currLoc = parseInstruction(node.second.currLoc, module, DBHashMap);
    newState->node.currPC = currLoc->parent->instructions;
    while (newState->node.currPC != currLoc) {
      ++newState->node.currPC;
    }
    // if (node.second.terminated == false) {

    // }
    
    newState->node.countInstrs = node.second.countInstr;
    newState->node.executionPath = node.second.choiceBranch;
   
    size_t index = 0;
    while (index < node.second.solverResult.size()) {
      if (node.second.solverResult[index] == '1') {
        newState->node.solverResult.push_back(Solver::True);
      } else if (node.second.solverResult[index] == '0') {
        newState->node.solverResult.push_back(Solver::Unknown);
      } else if (node.second.solverResult[index] == '-') {
        index++;
        if (node.second.solverResult[index] == '1') {
          newState->node.solverResult.push_back(Solver::False);
        }
      }
      index++;
    }

    ref<Path> path = parse(node.second.path, module, DBHashMap);
    newState->node.path = *path;
    
    if (node.second.isolated == 1) {
      newState->isolated = true;
    }
    newState->node.isolated = newState->isolated;

    if (node.second.terminated == 0) {
      newState->node.terminated = false;
    } else {
      newState->node.terminated = true;
    }

    for (auto expr_instr : node.second.expr_instr) {
      auto expr = exprReverseDBMap[expr_instr.first];
      auto instr = parseInstruction(expr_instr.second, module, DBHashMap);
      // newState->addConstraint(expr, instr);
      newState->node.addConstraint(expr, instr);
    }
    reExecutionStates.insert(newState);
  }
}

std::set<ExecutionState *, ExecutionStateIDCompare> ObjectManager::getReExecutionStates() {
  return reExecutionStates;
}

void ObjectManager::loadPobs() {
  auto DBPob = db->pobs_retrieve();
  for (auto pob : DBPob) {
    ProofObligation *newPob = new ProofObligation(pob.first);
    newPob->setCounter(maxIdPob);
    if (pob.first == pob.second.root_id) {
      newPob->parent = nullptr;
      newPob->root = newPob;
    } else {
      // auto pob_root = std::make_pair(pob.first, pob.second.root_id);
      // pobsRoot.push_back(pob_root);
      pobsRoot[pob.second.root_id].insert(pob.first);
      newPob->parent = nullptr;
      newPob->root  = nullptr;
    }
    newPob->location = parseLocation(pob.second.location, module, DBHashMap);
    ref<Path> path = parse(pob.second.path, module, DBHashMap);
    newPob->path = *path;
    for (auto instr : pob.second.stack) {
      auto instraction = parseInstruction(instr.second, module, DBHashMap);
      newPob->stack.push_back(instraction);
    }
    for (auto expr_instr : pob.second.expr_instr) {
      auto expr = exprReverseDBMap[expr_instr.first];
      auto instr = parseInstruction(expr_instr.second, module, DBHashMap);
      // newPob->condition.insert(expr, instr);
      newPob->addCondition(expr, instr);
    }
    addedPobs.push_back(newPob);
  }
  for (auto pob : addedPobs) {
    auto pob_id = pob->id;
    auto children = pobsChildren[pob_id];
    for (auto child_id : children) {
      for (auto child_pob : addedPobs) {
        if (child_pob->id == child_id) {
          pob->children.insert(child_pob);
          child_pob->parent = pob;
        }
      }
    }
  }
  for (auto pob : addedPobs) {
    auto pob_id = pob->id;
    auto descendants = pobsRoot[pob_id];
    for (auto child_id : descendants) {
      for (auto desc_pob : addedPobs) {
        if (desc_pob->id == child_id) {
          desc_pob->root = pob;
        }
      }
    }
  }
}

void ObjectManager::storeStates() {
  for (auto node : statesDB) {
    std::string values;
    values += std::to_string(node.state_id) + ", "; // state_id
    
    std::string initLoc = "'"; // initLoc
    initLoc += node.initPC->parent->parent->function->getName().str();
    initLoc += " ";
    initLoc += std::to_string(node.initPC->dest) + "', ";
    values += initLoc;
    
    std::string currLoc = "'"; // currLoc
    currLoc += node.currPC->parent->parent->function->getName().str();
    currLoc += " ";
    currLoc += std::to_string(node.currPC->dest) + "', ";
    values += currLoc;

    values += "'" + node.executionPath + "', "; // choiceBranch
    
    std::string solverResult = "'"; // solverResult
    for (auto res : node.solverResult) {
      solverResult += std::to_string(res);
    }
    solverResult += "', ";
    values += solverResult;

    values += "'" + node.path.toString() + "', "; // path
    std::set<KFunction *> functions = node.path.getFunctionsInPath();
    for (auto f : functions) {
      db->functionhash_write(std::string(f->function->getName()), module->functionHash(f));
    }

    values += std::to_string(node.countInstrs) + ", "; // countInstr
    
    if (node.isolated == true) { // isolated
      values += std::to_string(1) + ", ";
    } else {
      values += std::to_string(0) + ", ";
    }

    if (node.terminated == true) { // terminated
      values += std::to_string(1);
    } else {
      values += std::to_string(0);
    }

    db->state_write(values);

    for (auto e : node.constraints) { // constraints
      if (!exprDBMap.count(e))
        exprDBMap[e] = db->expr_write(e);
      assert(exprDBMap.count(e) && "No expr in DB");
      auto instr = node.constraints.getLocation(e);
      std::string str_instr = instr->parent->parent->function->getName().str();
      str_instr += " ";
      str_instr +=  std::to_string(instr->dest);
      db->statesConstr_write(node.state_id, exprDBMap[e], str_instr);
      storeArray(e);
    }
  }
}

void ObjectManager::storePropagations() {
  for (auto prop : propagations) {
    db->prop_write(prop.state->getID(), prop.pob->id);
  }
}

void ObjectManager::storeAllToDB() {
  // write all objects to DB
  db->maxId_write(maxIdState, maxIdPob);

  storeStates();
  storePropagations();
  storeLemmas();
}

void ObjectManager::loadAllFromDB(ExecutionState *startState) {
  builder = createDefaultExprBuilder();
  parser = expr::Parser::Create("DBParser", 
                                llvm::MemoryBuffer::getMemBuffer("").get(),
                                builder, arrayCache, false);

  DBHashMap = db->functionhash_retrieve();
  auto maxId = db->maxId_retrieve();
  maxIdState = maxId.first;
  maxIdPob = maxId.second;

  auto arrays = db->arrays_retrieve();
  auto parents = db->parents_retrieve();
  for (const auto &parent : parents) {
    arrayParentMap[parent.first].insert(parent.second);
  }
  for (const auto &array : arrays) {
    makeArray(arrays, array.first);
  }

  auto exprs = db->exprs_retrieve();
  makeExprs(exprs);

  // load all objects from DB
  loadLemmas();

  loadStates(startState);

  auto childrens = db->pobsChildren_retrieve();
  for (const auto &pob_child : childrens) {
    pobsChildren[pob_child.first].insert(pob_child.second);
  }
  loadPobs();

  delete parser;
  delete builder;
}



ObjectManager::~ObjectManager() {
  pobs.clear();
  propagations.clear();
  for (auto lemma : lemmas) {
    delete lemma;
  }
  delete db;
}