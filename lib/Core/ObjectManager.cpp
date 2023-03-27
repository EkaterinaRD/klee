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

ConvertState::ConvertState(const ExecutionState *state, bool isTerminated) {
  // state_id = std::to_string(state->getID());
  state_id = state->getID();
  // il = "'" + state->initPC->parent->getLabel() + "'";
  initPC = state->initPC;
  // cl = "'" + state->pc->parent->getLabel() + "'";
  currPC = state->pc;
  // path = "'" + state->path.toString() + "'";
  path = state->path;
  // pc = "'" + state->printConstraints() + "'";
  constraintInfos = state->constraintInfos;
  choiceBranch = state->executionPath;
  // ci = std::to_string(state->steppedInstructions);
  countInstr = state->steppedInstructions;
  isIsolated = state->isIsolated();
  terminated = isTerminated;
}

std::string ConvertState::getValues() const {
  std::string strConstr = "";
  for (auto i : constraintInfos) {
    strConstr += i->toString();
    strConstr += " ";
  }


  std::string strIsIsolated;
  if (isIsolated)
    strIsIsolated = "1";
  else 
    strIsIsolated = "0";

  std::string strTerminated;
  if (terminated)
    strTerminated = "1";
  else 
    strTerminated = "0";
  
  std::string values;
  // values = "(" + state_id + ", " + il + ", " + cl + ", " + path + ", " + pc + ", " + cb + ", " + ci + ", " + isIsolated + ", " + terminated + ")";
  values = "(" + std::to_string(state_id) + ", " +
           "'" + initPC->parent->getLabel() + "', " +
           "'" + currPC->parent->getLabel() + "', " +
           "'" + path.toString() + "', " +
           "'" + strConstr + "', " +
           "'" + choiceBranch + "', " +
           std::to_string(countInstr) + ", " + 
           strIsIsolated + ", " + strTerminated + ")";

  return values;
}

void ConvertState::setValues(Database::DBState state, KModule *module, std::map<std::string, size_t> DBHashMap) {
  state_id = state.state_id;

  ref<Path> _path = parse(state.path, module, DBHashMap);
  path = *_path;
  initLocation = parseBlock(state.initLocation, module, DBHashMap);

  choiceBranch = state.choiceBranch;
  countInstr = state.countInstructions;

  if (state.isIsolated == 1) {
    isIsolated = true;
  } else {
    isIsolated = false;
  }

  if (state.terminated == 1) {
    terminated = true;
  } else {
    terminated = false;
  }
}
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
  initialState = state->copy(); //print initialState
  // llvm::errs() << "InitialState: " << initialState->getID() << "\n";

  emptyState = state->copy();
  emptyState->stack.clear();
  emptyState->isolated = true; //print emptyState
  // llvm::errs() << "EmptyState: " << emptyState->getID() << "\n";
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
  std::set<KFunction *> functions = newPob->path.getFunctionsInPath();
  for (auto f : functions) {
    db->functionhash_write(std::string(f->function->getName()),
                                module->functionHash(f));
  }
  db->pob_write(newPob);
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
    state = initialState->copy(); //print initBranch from initialStae
    // llvm::errs() << "InitBranch from initialState: " << state->parent_id << "->" << state->getID() << "\n";
    state->isolated = true;
  } else {
    state = emptyState->withKInstruction(loc); //print initBranch from emptyState
    // llvm::errs() << "InitBranch from emptyState: " << state->parent_id << "->" << state->getID() << "\n";
  }
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

void ObjectManager::addState(ExecutionState *state) {
  addedStates.push_back(state);
}

ExecutionState *ObjectManager::branchState(ExecutionState *state) {
  ExecutionState *newState = state->branch();
  
  // llvm::errs() << "Branch from state: " << newState->parent_id << " to: " << newState->getID() << "\n";

  addedStates.push_back(newState);

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
      // llvm::errs() << "Add prop: state: " << state->getID() << " pob: " << pob->id <<"\n";
      addedPropagations.push_back(prop);
      if (!state->isIsolated()) {
        ++state->backwardStepsLeftCounter;
      }
      db->propagation_write(state, pob);
    }
  }
}

void ObjectManager::addPobToState(ProofObligation *pob) {
  for (auto state : targetedStates) {
    if (state->pc->parent == pob->location && checkStack(state, pob)) {
      assert(state->path.getFinalBlock() == pob->path.getInitialBlock() &&
               "Paths are not compatible.");
      Propagation prop(state, pob);
      // llvm::errs() << "Add prop: state: " << state->getID() << " pob: " << pob->id <<"\n";
      addedPropagations.push_back(prop);
      db->propagation_write(state, pob);
    }
  }
}

void ObjectManager::setReachedStates() {
  ref<ReachedStatesAction> act = cast<ReachedStatesAction>(_action);
  std::vector<ExecutionState *> reachedStates = act->reached;

  for (auto state : reachedStates) {
    saveState(state, true);
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
      // llvm::errs() << "Delete state: " << state->getID() << "\n";
      delete state; //print delete forward state
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
      // llvm::errs() << "Delete isolated state: " << state->getID() << "\n";
      delete state; //print delete branch state
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
    // llvm::errs() << "Delete pob: " << pob->id << "\n";
    delete pob; //print delete pob
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

void ObjectManager::saveState(const ExecutionState *state, bool isTerminated) {
  std::set<KFunction *> functions = state->path.getFunctionsInPath();
  for (auto f : functions) {
    db->functionhash_write(std::string(f->function->getName()),
                                module->functionHash(f));
  }
  ConvertState cs(state, isTerminated);
  statesDB.push_back(cs);
}


ExecutionState *ObjectManager::replayStateFromPob(ProofObligation *pob) {
  assert(pob->location->instructions[0]->inst == emptyState->initPC->inst);

  ExecutionState *replayState = initialState->copy(); //print replayState
  // llvm::errs() << "Replay state: " << replayState->parent_id << "->" << replayState->getID() << "\n";
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
  // llvm::errs() << "Delete replayState: " << replayState->getID() << "\n"; 
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

        std::vector<const Array *> arrays;
        klee::findSymbolicObjects(constraint, arrays);
        for (auto array : arrays) {
          if (!arrayDBMap.count(array))
            arrayDBMap[array] = db->array_write(array);
          uint64_t idArray = arrayDBMap[array];
          assert(exprDBMap.count(constraint) && "No expr in DB");
          assert(arrayDBMap.count(array) && "No array in DB");
          db->arraymap_write(idArray, idConstr);
        }
        for (auto array : arrays) {
          assert(arrayDBMap.count(array) && "No child in DB");
          uint64_t idChild = arrayDBMap.count(array);
          for (auto parent : array->parents) {
            assert(arrayDBMap.count(parent) && "No parent in DB");
            uint64_t idParent = arrayDBMap.count(parent);
            db->parent_write(idChild, idParent);
          }
        }


      }
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
  // ExprBuilder *builder = createDefaultExprBuilder();
  // parser = expr::Parser::Create("DBParser", 
  //                               llvm::MemoryBuffer::getMemBuffer("").get(),
  //                               builder, arrayCache, false);


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

  auto DBLemmas = db->lemmas_retrieve();
  auto DBHashMap = db->functionhash_retrieve();
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

  // delete parser; // remove to end of storeAllToDB;
  // delete builder;
}

void ObjectManager::storeStates() {
  for (auto state : statesDB) {
    db->state_write(state);
  }
}

void ObjectManager::storeAllToDB() {
  // write all objects to DB
  storeStates();
  storeLemmas();
  DBReady = false;
}



void ObjectManager::loadStates(ExecutionState *startState) {
  auto DBStates = db->states_retrieve();
  auto DBHashMap = db->functionhash_retrieve();
  for (auto _state : DBStates) {
    // ConvertState state;
    // state.setValues(_state, module, DBHashMap);
    auto initLocation = parseBlock(_state.initLocation, module, DBHashMap);
    ExecutionState *newState = new ExecutionState(initLocation.first, initLocation.second);
    newState->path = *parse(_state.path, module, DBHashMap);
    newState->executionPath = _state.choiceBranch;
    newState->steppedInstructions = _state.countInstructions;
    if (_state.isIsolated == 1)
      newState->isolated = true;
    newState->id = _state.state_id;
    addedStates.push_back(newState);
    // statesDB.push_back(state);
  }
}

void ObjectManager::loadPobs() {
  auto DBPobs = db->pobs_retrieve();
  auto DBHashMap = db->functionhash_retrieve();

  std::vector<ProofObligation *> pobs;

  for (auto pob : DBPobs) {
    ProofObligation *newPob;
    int id = pob.pob_id;

    ref<Path> path = parse(pob.path, module, DBHashMap);
    newPob->path = *path;

    KBlock *location = parseBlock(pob.location, module, DBHashMap).second;
    newPob->location = location;
    pobs.push_back(newPob);
  }

  // for (auto pob : pobs) {
  //   setRoot;
  //   setparent
  //   setChildren
  // }
}

void ObjectManager::loadAllFromDB(ExecutionState *startState) {
  builder = createDefaultExprBuilder();
  parser = expr::Parser::Create("DBParser", 
                                llvm::MemoryBuffer::getMemBuffer("").get(),
                                builder, arrayCache, false);

  // load all objects from DB
  loadPobs();
  loadLemmas();
  // loadStates(startState);

  DBReady = true;
  delete parser;
  delete builder;
}




ObjectManager::~ObjectManager() {
  pobs.clear();
  propagations.clear();
  statesDB.clear();
  for (auto lemma : lemmas) {
    delete lemma;
  }
  delete db;
}