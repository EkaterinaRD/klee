// -*- C++ -*-
#pragma once

#include "ProofObligation.h"
#include "klee/Module/KModule.h"
#include <unordered_set>
#include <variant>

namespace klee {

class ExecutionState;
class Executor;

struct Action {
  enum class Type { Init, Forward, Backward, Terminate };

  Type type;
  ExecutionState* state; // Forward, Backward
  KBlock *location;      // Init
  ProofObligation *pob;  // Backward
  std::unordered_set<KBlock*> targets; // Init

  Action(ExecutionState *es)
      : type(Action::Type::Forward), state(es), location(nullptr), pob(nullptr),
        targets({}) {}

  Action(Type t, ExecutionState *es, KBlock *loc, ProofObligation *pob,
         std::unordered_set<KBlock *> targets)
      : type(t), state(es), location(loc), pob(pob), targets(targets) {}
};

struct SearcherConfig {
  ExecutionState* initial_state;
  std::unordered_set<KBlock*> targets;
  // Hack
  Executor* executor;
};

struct ForwardResult {
  ExecutionState *current;
  // references to vectors?
  std::vector<ExecutionState *> addedStates;
  std::vector<ExecutionState *> removedStates;
  // _-_ In the future probably do not use references
  ForwardResult(ExecutionState *_s, std::vector<ExecutionState *> &a,
                std::vector<ExecutionState *> &r)
      : current(_s), addedStates(a), removedStates(r){};
  // Way too easy to mistake for ...
  ForwardResult(ExecutionState *_s)
      : current(_s), addedStates({}), removedStates({}){};
};

struct BackwardResult {
  ProofObligation *newPob;
  ProofObligation *oldPob;
  BackwardResult(ProofObligation *_newPob, ProofObligation *_oldPob)
      : newPob(_newPob), oldPob(_oldPob) {}
};

struct InitResult {
  KBlock* location;
  ExecutionState* state;
  InitResult(KBlock *_loc, ExecutionState *es) : location(_loc), state(es) {}
};

using ActionResult = std::variant<ForwardResult, BackwardResult, InitResult>;

}