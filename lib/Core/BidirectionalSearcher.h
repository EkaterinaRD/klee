//===-- BidirectionalSearcher.h ---------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#pragma once
#include "BackwardSearcher.h"
#include "Executor.h"
#include "ForwardSearcher.h"
#include "Initializer.h"
#include "ProofObligation.h"
#include "SearcherUtil.h"

#include "Subscriber.h"
#include "ObjectManager.h"

#include "klee/Module/KModule.h"
#include <memory>
#include <unordered_set>
#include <vector>

namespace klee {

class IBidirectionalSearcher : public Subscriber {
public:
  virtual ref<BidirectionalAction> selectAction() = 0;
  virtual void update(ref<ActionResult>) = 0;
  virtual void closeProofObligation(ProofObligation *) = 0;
  virtual ~IBidirectionalSearcher() {}
};

class BidirectionalSearcher : public IBidirectionalSearcher {
public:
  ref<BidirectionalAction> selectAction() override;
  void update(ref<ActionResult>) override;
  void closeProofObligation(ProofObligation *) override;
  explicit BidirectionalSearcher(const SearcherConfig &);
  ~BidirectionalSearcher() override;

private:
  enum class StepKind { Initialize, Forward, Branch, Backward, Terminate, ReachedStates};

  Executor *ex; // hack
  ObjectManager *objMng;
  ExecutionState *initialState;
  
  std::vector<ExecutionState *> pausedStates;
  void pauseState(ExecutionState *state, BidirectionalSearcher::StepKind stepKind);
  
  std::vector<ExecutionState *> reached;
  bool reachedStatesFlag;

  GuidedSearcher *forward;
  GuidedSearcher *branch;
  RecencyRankedSearcher *backward;
  ConflictCoreInitializer *initializer;

  std::vector<ProofObligation *> pobs;
  Ticker ticker;

  // Temporary _-_
  std::unordered_set<llvm::BasicBlock *> rootBlocks;
  std::unordered_set<llvm::BasicBlock *> reachableBlocks;

  StepKind selectStep();
  void updateForward(ExecutionState *current,
                     const std::vector<ExecutionState *> &addedStates,
                     const std::vector<ExecutionState *> &removedStates,
                     ref<TargetedConflict> targetedConflict);
  void updateBranch(ExecutionState *current,
                    const std::vector<ExecutionState *> &addedStates,
                    const std::vector<ExecutionState *> &removedStates);
  void updateBackward(std::vector<ProofObligation *> newPobs,
                      ProofObligation *oldPob);
  void updateInitialize(KInstruction *location, ExecutionState &state);
  void updatePropagations(std::vector<Propagation> &addedPropagations,
                          std::vector<Propagation> &removedPropagations);

  void addPob(ProofObligation *);
  void removePob(ProofObligation *);
  void answerPob(ProofObligation *);
};

class ForwardOnlySearcher : public IBidirectionalSearcher {
public:
  ref<BidirectionalAction> selectAction() override;
  void update(ref<ActionResult>) override;
  void closeProofObligation(ProofObligation *) override;
  explicit ForwardOnlySearcher(const SearcherConfig &);
  ~ForwardOnlySearcher() override;

private:
  std::unique_ptr<ForwardSearcher> searcher;
};

class GuidedOnlySearcher : public IBidirectionalSearcher {
public:
  ref<BidirectionalAction> selectAction() override;
  void update(ref<ActionResult>) override;
  void closeProofObligation(ProofObligation *) override;
  explicit GuidedOnlySearcher(const SearcherConfig &);
  ~GuidedOnlySearcher() override;

private:
  Executor *ex; // hack
  std::vector<ExecutionState *> pausedStates;
  void pauseState(ExecutionState *state);
  std::unique_ptr<GuidedSearcher> searcher;
};

bool isStuck(ExecutionState &state);

} // namespace klee
