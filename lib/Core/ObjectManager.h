#ifndef KLEE_OBJECTMANAGER_H
#define KLEE_OBJECTMANAGER_H

#include "ExecutionState.h"
#include "SearcherUtil.h"
#include "ProofObligation.h"
#include "Subscriber.h"
#include "Database.h"
#include "klee/Expr/Expr.h"
#include "klee/Expr/ExprHashMap.h"
#include "klee/Expr/Parser/Parser.h"
#include "klee/Module/KModule.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

#include <set>
#include <vector>

namespace klee {

struct Lemma {
  Path path;
  ExprHashSet constraints;

  bool operator==(const Lemma &other) {
    return this->path == other.path && this->constraints == other.constraints;
  }

  Lemma(const Path &path) : path(path) {}
};

struct ConvertState {

  std::string getValues() const;
  ConvertState(const ExecutionState *state, bool isTerminated);

private: 
  std::string state_id; 
  std::string il;
  std::string cl;
  std::string path;
  std::string pc;
  std::string cb;
  std::string ci; 
  std::string isIsolated;
  std::string terminated;
};

class ObjectManager {
private:
  // 1. Subscribers
  std::vector<Subscriber *> subscribers;
  std::vector<Subscriber *> subscribersAfterAll;

  // 2. BidirectionalAction and ActionResult
  ref<BidirectionalAction> _action;
  ref<ActionResult> result;
  ExecutionState *initialState;
  ExecutionState *emptyState;

  // 3. States
  std::set<ExecutionState *, ExecutionStateIDCompare> states;
  std::set<ExecutionState *, ExecutionStateIDCompare> isolatedStates;
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
  ref<TargetedConflict> targetedConflict;
  std::set<ExecutionState *, ExecutionStateIDCompare> targetedStates;

  // 4. Pobs
  std::vector<ProofObligation *> pobs;
  std::vector<ProofObligation *> addedPobs;
  std::vector<ProofObligation *> removedPobs;

  // 5. Propagations
  std::vector<Propagation> propagations;
  std::vector<Propagation> addedPropagations;
  std::vector<Propagation> removedProgations;

  // 6. Lemmas
  std::map<KBlock *, std::set<Lemma *>> locationMap;
  std::map<Path, std::set<Lemma *>> pathMap;
  std::set<Lemma *> lemmas;

  // 7. Database;
  bool DBReady = false;
  Database *db;
  expr::Parser *parser;
  ArrayCache *arrayCache;
  KModule *module;
  llvm::raw_fd_ostream *summaryFile;
  // 7.1 Lemmas
  std::map<const Lemma *, int64_t> lemmaDBMap;
  // 7.2 Expressions
  std::map<ref<Expr>, int64_t> exprDBMap;
  std::map<int64_t, ref<Expr>> exprReverseDBMap;
  // 7.3 Symbolic objects (maybe...)
  std::map<const Array *, int64_t> arrayDBMap;
  std::map<uint64_t, std::set<uint64_t>> arrayParentMap;
  std::map<int64_t, const Array *> arrayReverseDBMap;
  // 7.4 States
  std::vector<ConvertState> statesDB;

public:
  ObjectManager();

  void setDatabase(llvm::raw_fd_ostream *_summaryFile, std::string _DBFile) { 
    summaryFile = _summaryFile;
    db = new Database(_DBFile); }
  void setModule(KModule *_module) { module = _module; }
  void setArrayCache(ArrayCache *cache) { arrayCache = cache; }

  void subscribe(Subscriber *s);
  void subscribeAfterAll(Subscriber *s);
  void unsubscribe(Subscriber *s);

  void setInitialAndEmtySt(ExecutionState *state);
  void deleteInitialAndEmptySt();
  ExecutionState *getInitialState();
  ExecutionState *getEmptyState();

  void setAction(ref<BidirectionalAction> action);

  void addPob(ProofObligation *newPob);
  void removePob(ProofObligation *pob);
  std::vector<ProofObligation *> getPobs();
  ExecutionState *initBranch(ref<InitializeAction> action);

  void setTargetedConflict(ref<TargetedConflict> tc);

  void setReachedStates();
  
  void addState(ExecutionState *state);
  ExecutionState *branchState(ExecutionState *state);
  void removeState(ExecutionState *state);

  bool emptyStates();
  bool emptyIsolatedStates();
  std::size_t sizeStates();
  void copyStatesTo(std::vector<ExecutionState *> &statesList);
  const std::set<ExecutionState *, ExecutionStateIDCompare> &getStates();
  const std::set<ExecutionState *, ExecutionStateIDCompare> &getIsolatedStates();

  void updateResult();
  std::vector<ExecutionState *> closeProofObligation(bool replayStateFromProofObligation);

  void summarize(const ProofObligation *pob,
                 const Conflict &conflict,
                 const ExprHashMap<ref<Expr>> &rebuildMap);
  void saveState(const ExecutionState *state, bool isTerminated);                 
  void storeAllToDB();               
  void loadAllFromDB();
  void makeArray(const std::map<uint64_t, std::string> &arrays, uint64_t id);
  void makeExprs(const std::map<uint64_t, std::string> &exprs);
  
  ~ObjectManager();

private:
  void setResult();

  void addStateToPob(ExecutionState *state);
  void addPobToState(ProofObligation *pob);
  void createPropagations();

  bool checkStack(ExecutionState * state, ProofObligation *pob);


  ExecutionState *replayStateFromPob(ProofObligation *pob);

  // void storePobs();
  void storeStates();
  // void storePropagations();
  void storeLemmas();
  // void loadPobs();
  // void loadStates();
  // void loadPropagations();
  void loadLemmas();
};
} // namespace klee


#endif /*KLEE_OBJECTMANAGER_H*/