//===-- ExecutionState.h ----------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_EXECUTIONSTATE_H
#define KLEE_EXECUTIONSTATE_H

#include "Path.h"
#include "ProofObligation.h"

#include "AddressSpace.h"
#include "MergeHandler.h"
//twice?
#include "Path.h"

#include "klee/ADT/TreeStream.h"
#include "klee/Expr/Constraints.h"
#include "klee/Expr/Expr.h"
#include "klee/Module/KInstIterator.h"
#include "klee/Solver/Solver.h"
#include "TimingSolver.h"
#include "klee/System/Time.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"

#include <map>
#include <memory>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace klee {
class Array;
class CallPathNode;
struct Cell;
struct KFunction;
struct KBlock;
struct KInstruction;
class MemoryObject;
class PTreeNode;
struct InstructionInfo;

llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const MemoryMap &mm);

struct StackFrame {
  KInstIterator caller;
  KFunction *kf;
  CallPathNode *callPathNode;

  std::vector<const MemoryObject *> allocas;
  Cell *locals;

  /// Minimum distance to an uncovered instruction once the function
  /// returns. This is not a good place for this but is used to
  /// quickly compute the context sensitive minimum distance to an
  /// uncovered instruction. This value is updated by the StatsTracker
  /// periodically.
  unsigned minDistToUncoveredOnReturn;

  // For vararg functions: arguments not passed via parameter are
  // stored (packed tightly) in a local (alloca) memory object. This
  // is set up to match the way the front-end generates vaarg code (it
  // does not pass vaarg through as expected). VACopy is lowered inside
  // of intrinsic lowering.
  MemoryObject *varargs;

  StackFrame(KInstIterator caller, KFunction *kf);
  StackFrame(const StackFrame &s);
  ~StackFrame();
  void print() const;
};

/// Contains information related to unwinding (Itanium ABI/2-Phase unwinding)
class UnwindingInformation {
public:
  enum class Kind {
    SearchPhase, // first phase
    CleanupPhase // second phase
  };

private:
  const Kind kind;

public:
  // _Unwind_Exception* of the thrown exception, used in both phases
  ref<ConstantExpr> exceptionObject;

  Kind getKind() const { return kind; }

  explicit UnwindingInformation(ref<ConstantExpr> exceptionObject, Kind k)
      : kind(k), exceptionObject(exceptionObject) {}
  virtual ~UnwindingInformation() = default;

  virtual std::unique_ptr<UnwindingInformation> clone() const = 0;
};

struct SearchPhaseUnwindingInformation : public UnwindingInformation {
  // Keeps track of the stack index we have so far unwound to.
  std::size_t unwindingProgress;

  // MemoryObject that contains a serialized version of the last executed
  // landingpad, so we can clean it up after the personality fn returns.
  MemoryObject *serializedLandingpad = nullptr;

  SearchPhaseUnwindingInformation(ref<ConstantExpr> exceptionObject,
                                  std::size_t const unwindingProgress)
      : UnwindingInformation(exceptionObject,
                             UnwindingInformation::Kind::SearchPhase),
        unwindingProgress(unwindingProgress) {}

  std::unique_ptr<UnwindingInformation> clone() const {
    return std::make_unique<SearchPhaseUnwindingInformation>(*this);
  }

  static bool classof(const UnwindingInformation *u) {
    return u->getKind() == UnwindingInformation::Kind::SearchPhase;
  }
};

struct CleanupPhaseUnwindingInformation : public UnwindingInformation {
  // Phase 1 will try to find a catching landingpad.
  // Phase 2 will unwind up to this landingpad or return from
  // _Unwind_RaiseException if none was found.

  // The selector value of the catching landingpad that was found
  // during the search phase.
  ref<ConstantExpr> selectorValue;

  // Used to know when we have to stop unwinding and to
  // ensure that unwinding stops at the frame for which
  // we first found a handler in the search phase.
  const std::size_t catchingStackIndex;

  CleanupPhaseUnwindingInformation(ref<ConstantExpr> exceptionObject,
                                   ref<ConstantExpr> selectorValue,
                                   const std::size_t catchingStackIndex)
      : UnwindingInformation(exceptionObject,
                             UnwindingInformation::Kind::CleanupPhase),
        selectorValue(selectorValue),
        catchingStackIndex(catchingStackIndex) {}

  std::unique_ptr<UnwindingInformation> clone() const {
    return std::make_unique<CleanupPhaseUnwindingInformation>(*this);
  }

  static bool classof(const UnwindingInformation *u) {
    return u->getKind() == UnwindingInformation::Kind::CleanupPhase;
  }
};

typedef std::pair<llvm::BasicBlock *, llvm::BasicBlock *> Transition;

struct TransitionHash {
  std::size_t operator()(const Transition& p) const {
    return reinterpret_cast<size_t>(p.first) * 31 + reinterpret_cast<size_t>(p.second);
  }
};

typedef std::pair<ref<const MemoryObject>, const Array *> Symbolic;

struct Target {
  KBlock *block;

  explicit Target(KBlock *_block) :
    block(_block) {}

  bool operator<(const Target &other) const {
    return block < other.block;
  }

  bool operator==(const Target &other) const {
    return block == other.block;
  }

  bool atReturn() const { return isa<KReturnBlock>(block); } 

  std::string print() const {
    std::string repr = "Target: ";
    repr += block->getIRLocation();
    if (atReturn()) {
      repr += " (at the end)";
    }
    return repr;
  }
  
};

struct Node {
  std::uint32_t state_id;
  KInstIterator initPC;
  KInstIterator currPC;
  std::uint64_t countInstrs;
  std::string executionPath; //==choiceBranch
  std::vector<Solver::Validity> solverResult;
  Constraints constraints;
  Path path;
  bool isolated;
  bool terminated;
  std::set<Target> targets;
  bool reached = false;

  std::vector<std::string> choiceBranch;

  size_t indexSolverResult = 0;
  size_t indexConstraints = 0;
  size_t indexChoiceBranch = 0;

  // void appendSolverResult(Solver::Validity result);
  void addConstraint(ref<Expr> e, KInstruction *loc, bool *sat = 0);
  Solver::Validity getSolverResult();
  ref<Expr> getExpr();
  std::string getChoice();
  void clearNode();

  std::string getValues();

};


/// @brief ExecutionState representing a path under exploration
class ExecutionState {
#ifdef KLEE_UNITTEST
public:
#else
private:
#endif
  // copy ctor
  ExecutionState(const ExecutionState &state);

public:
  using stack_ty = std::vector<StackFrame>;

  std::map<ref<Expr>, std::pair<Symbolic, ref<Expr>>> pointers;

  // Execution - Control Flow specific

  /// @brief Pointer to initial instruction
  KInstIterator initPC;

  /// @brief Pointer to instruction to be executed after the current
  /// instruction
  KInstIterator pc;

  /// @brief Pointer to instruction which is currently executed
  KInstIterator prevPC;

  /// @brief Stack representing the current instruction stream
  stack_ty stack;

  int stackBalance;

  /// @brief Remember from which Basic Block control flow arrived
  /// (i.e. to select the right phi values)
  std::int32_t incomingBBIndex;

  // Overall state of the state - Data specific

  /// @brief Exploration depth, i.e., number of times KLEE branched for this state
  std::uint32_t depth;

  /// @brief Exploration level, i.e., number of times KLEE cycled for this state
  std::unordered_multiset<llvm::BasicBlock *> multilevel;
  std::unordered_set<llvm::BasicBlock *> level;
  std::unordered_set<Transition, TransitionHash> transitionLevel;

  /// @brief Address space used by this state (e.g. Global and Heap)
  AddressSpace addressSpace;

  /// @brief Constraints collected so far
  Constraints constraintInfos;
  const ConstraintSet &constraints;

  /// Statistics and information

  /// @brief Metadata utilized and collected by solvers for this state
  mutable SolverQueryMetaData queryMetaData;

  /// @brief History of complete path: represents branches taken to
  /// reach/create this state (both concrete and symbolic)
  TreeOStream pathOS;

  /// @brief History of symbolic path: represents symbolic branches
  /// taken to reach/create this state
  TreeOStream symPathOS;

  /// @brief History of state branching including switches
  std::string executionPath;

  std::vector<Solver::Validity> solverResult;

  /// @brief Set containing which lines in which files are covered by this state
  std::map<const std::string *, std::set<std::uint32_t>> coveredLines;

  /// @brief Pointer to the process tree of the current state
  /// Copies of ExecutionState should not copy ptreeNode
  PTreeNode *ptreeNode = nullptr;

  /// @brief Ordered list of symbolics: used to generate test cases.
  //
  // FIXME: Move to a shared list structure (not critical).
  std::vector<Symbolic> symbolics;

  /// @brief Set of used array names for this state.  Used to avoid collisions.
  std::set<std::string> arrayNames;

  /// @brief The objects handling the klee_open_merge calls this state ran through
  std::vector<ref<MergeHandler>> openMergeStack;

  /// @brief The numbers of times this state has run through Executor::stepInstruction
  std::uint64_t steppedInstructions;

  /// @brief The numbers of times this state has run through Executor::stepInstruction with executeMemoryOperation
  std::uint64_t steppedMemoryInstructions;

  /// @brief Counts how many instructions were executed since the last new
  /// instruction was covered.
  std::uint32_t instsSinceCovNew;

  std::uint64_t steppedInstrLastFork = 0;

  /// @brief Keep track of unwinding state while unwinding, otherwise empty
  std::unique_ptr<UnwindingInformation> unwindingInformation;

  /// @brief the global state counter
  static std::uint32_t nextID;

  /// @brief the state id
  std::uint32_t id {0};

  std::uint32_t parentID {0};

  /// @brief Whether a new instruction was covered in this state
  bool coveredNew;

  /// @brief Disables forking for this state. Set by user code
  bool forkDisabled;

  bool isolated;
  // bool reExecuted = false;
  bool transferToBB = false;
  Node node;

  /// @brief The target basic block that the state must achieve
  std::set<Target> targets;

  Path path;

  /// @brief Index of current symbolic in case of pre-loaded symbolics.
  size_t symbolicCounter;

  ref<Expr> returnValue;

  unsigned backwardStepsLeftCounter;
  unsigned failedBackwardStepsCounter;

public:
  #ifdef KLEE_UNITTEST
  // provide this function only in the context of unittests
  ExecutionState() : constraints(constraintInfos) {}
  #endif
  // only to create the initial state
  explicit ExecutionState(KFunction *kf);
  ExecutionState(KFunction *kf, KBlock *kb);
  // only to create reex state
  ExecutionState(uint32_t _id, uint32_t max_id);
  ExecutionState(ExecutionState &state, uint32_t _id, uint32_t max_id);
  // no copy assignment, use copy constructor
  ExecutionState &operator=(const ExecutionState &) = delete;
  // no move ctor
  ExecutionState(ExecutionState &&) noexcept = delete;
  // no move assignment
  ExecutionState& operator=(ExecutionState &&) noexcept = delete;
  // dtor
  ~ExecutionState();

  ExecutionState *branch();
  ExecutionState *withKFunction(KFunction *kf) const;
  ExecutionState *withKBlock(KBlock *kb) const;
  ExecutionState *withKInstruction(KInstruction* ki) const;
  ExecutionState *copy() const;

  void pushFrame(KInstIterator caller, KFunction *kf);
  void popFrame();

  void addSymbolic(const MemoryObject *mo, const Array *array);

  void addConstraint(ref<Expr> e, KInstruction *loc, bool *sat = 0);

  bool merge(const ExecutionState &b);
  void dumpStack(llvm::raw_ostream &out) const;

  std::uint32_t getID() const { return id; };
  void setID() { id = nextID++; };
  llvm::BasicBlock *getInitPCBlock() const;
  llvm::BasicBlock *getPrevPCBlock() const;
  llvm::BasicBlock *getPCBlock() const;

  void increaseLevel();
  bool isEmpty() const;
  bool isCriticalPC() const;
  bool isIsolated() const;

  void setNode(bool terminated);
  void setMaxID(uint32_t max_id);
  void setInitLocation(KInstruction *initLoc);
  void appendSolverResult(Solver::Validity result);
  // for debugging
  static void printCompareList(const ExecutionState &, const ExecutionState &, llvm::raw_ostream &);
  void print(llvm::raw_ostream & os) const;
};

struct ExecutionStateIDCompare {
  bool operator()(const ExecutionState *a, const ExecutionState *b) const {
    return a->getID() < b->getID();
  }
};

class ExecutionManager {
private:
  std::map<Target, std::unordered_set<ExecutionState *>> mapTargetToStates;
public:
  std::set<ExecutionState *, ExecutionStateIDCompare> states;

  ~ExecutionManager();

  void insert(Target target, ExecutionState &state);
  std::unordered_set<ExecutionState *> &at(Target target);
};

}

#endif /* KLEE_EXECUTIONSTATE_H */
