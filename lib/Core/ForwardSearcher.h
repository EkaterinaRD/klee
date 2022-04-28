// -*- C++ -*-
#pragma once

#include "ExecutionState.h"
#include "PForest.h"
#include "klee/ADT/RNG.h"
#include "klee/Module/KModule.h"
#include "klee/System/Time.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <memory>
#include <queue>
#include <set>
#include <unordered_set>
#include <vector>

namespace llvm {
  class BasicBlock;
  class Function;
  class Instruction;
  class raw_ostream;
}

namespace klee {
  template<class T, class Comparator> class DiscretePDF;
  class ExecutionState;

  /// A Searcher implements an exploration strategy for the Executor by selecting
  /// states for further exploration using different strategies or heuristics.
  class ForwardSearcher {
  public:
    virtual ~ForwardSearcher() = default;

    /// Selects a state for further exploration.
    /// \return The selected state.
    virtual ExecutionState &selectState() = 0;

    /// Notifies searcher about new or deleted states.
    /// \param current The currently selected state for exploration.
    /// \param addedStates The newly branched states with `current` as common ancestor.
    /// \param removedStates The states that will be terminated.
    virtual void update(ExecutionState *current,
                        const std::vector<ExecutionState *> &addedStates,
                        const std::vector<ExecutionState *> &removedStates) = 0;

    /// \return True if no state left for exploration, False otherwise
    virtual bool empty() = 0;

    /// Prints name of searcher as a `klee_message()`.
    // TODO: could probably made prettier or more flexible
    virtual void printName(llvm::raw_ostream &os) = 0;

    enum CoreSearchType : std::uint8_t {
      DFS,
      BFS,
      RandomState,
      RandomPath,
      NURS_CovNew,
      NURS_MD2U,
      NURS_Depth,
      NURS_RP,
      NURS_ICnt,
      NURS_CPICnt,
      NURS_QC
    };
  };

  /// DFSSearcher implements depth-first exploration. All states are kept in
  /// insertion order. The last state is selected for further exploration.
  class DFSSearcher final : public ForwardSearcher {
    std::vector<ExecutionState*> states;

  public:
    ExecutionState &selectState() override;
    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates) override;
    bool empty() override;
    void printName(llvm::raw_ostream &os) override;
  };

  /// BFSSearcher implements breadth-first exploration. When KLEE branches multiple
  /// times for a single instruction, all new states have the same depth. Keep in
  /// mind that the process tree (PTree) is a binary tree and hence the depth of
  /// a state in that tree and its branch depth during BFS are different.
  class BFSSearcher final : public ForwardSearcher {
    std::deque<ExecutionState*> states;

  public:
    ExecutionState &selectState() override;
    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates) override;
    bool empty() override;
    void printName(llvm::raw_ostream &os) override;
  };

  /// RandomSearcher picks a state randomly.
  class RandomSearcher final : public ForwardSearcher {
    std::vector<ExecutionState*> states;
    RNG &theRNG;

  public:
    explicit RandomSearcher(RNG &rng);
    ExecutionState &selectState() override;
    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates) override;
    bool empty() override;
    void printName(llvm::raw_ostream &os) override;
  };

  /// TargetedSearcher picks a state /*COMMENT*/.
  class TargetedSearcher final : public ForwardSearcher {
  public:
    enum WeightResult : std::uint8_t {
      Continue,
      Done,
      Miss,
    };

  private:
    std::unique_ptr<DiscretePDF<ExecutionState *, ExecutionStateIDCompare>>
        states;

    KBlock* target;
    bool at_end;
    std::map<KFunction *, unsigned int> &distanceToTargetFunction;
    std::vector<ExecutionState *> reachedOnLastUpdate;

    bool distanceInCallGraph(KFunction *kf, KBlock *kb, unsigned int &distance);
    WeightResult tryGetLocalWeight(ExecutionState *es, double &weight, const std::vector<KBlock*> &localTargets);
    WeightResult tryGetPreTargetWeight(ExecutionState *es, double &weight);
    WeightResult tryGetTargetWeight(ExecutionState *es, double &weight);
    WeightResult tryGetPostTargetWeight(ExecutionState *es, double &weight);
    WeightResult tryGetWeight(ExecutionState* es, double &weight);

  public:
    std::unordered_set<ExecutionState*> states_set;
    TargetedSearcher(KBlock* target, bool at_return = false);
    ~TargetedSearcher() override;

    ExecutionState &selectState() override;
    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates) override;
    bool empty() override;
    void printName(llvm::raw_ostream &os) override;
    std::vector<ExecutionState *> reached();
    void removeReached();
  };

  class GuidedSearcher final : public ForwardSearcher {

  private:
    std::unique_ptr<ForwardSearcher> baseSearcher;
    std::map<Target, std::unique_ptr<TargetedSearcher>> targetedSearchers;
    unsigned index {1};
    bool reachingEnough;
    void addTarget(Target target);

  public:
    GuidedSearcher(std::unique_ptr<ForwardSearcher> baseSearcher, bool _reachingEnough);
    ~GuidedSearcher() override = default;
    ExecutionState &selectState() override;
    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates) override;

    std::map<Target,std::unordered_set<ExecutionState *>> collectAndClearReached();

    bool empty() override;
    void printName(llvm::raw_ostream &os) override;
  };

  /// The base class for all weighted searchers. Uses DiscretePDF as underlying
  /// data structure.
  class WeightedRandomSearcher final : public ForwardSearcher {
  public:
    enum WeightType : std::uint8_t {
      Depth,
      RP,
      QueryCost,
      InstCount,
      CPInstCount,
      MinDistToUncovered,
      CoveringNew
    };

  private:
    std::unique_ptr<DiscretePDF<ExecutionState*, ExecutionStateIDCompare>> states;
    RNG &theRNG;
    WeightType type;
    bool updateWeights;
    
    double getWeight(ExecutionState*);

  public:
    /// \param type The WeightType that determines the underlying heuristic.
    /// \param RNG A random number generator.
    WeightedRandomSearcher(WeightType type, RNG &rng);
    ~WeightedRandomSearcher() override = default;

    ExecutionState &selectState() override;
    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates) override;
    bool empty() override;
    void printName(llvm::raw_ostream &os) override;
  };

  /// RandomPathSearcher performs a random walk of the PTree to select a state.
  /// PTree is a global data structure, however, a searcher can sometimes only
  /// select from a subset of all states (depending on the update calls).
  ///
  /// To support this, RandomPathSearcher has a subgraph view of PTree, in that it
  /// only walks the PTreeNodes that it "owns". Ownership is stored in the
  /// getInt method of the PTreeNodePtr class (which hides it in the pointer itself).
  ///
  /// The current implementation of PTreeNodePtr supports only 3 instances of the
  /// RandomPathSearcher. This is because the current PTreeNodePtr implementation
  /// conforms to C++ and only steals the last 3 alignment bits. This restriction
  /// could be relaxed slightly by an architecture-specific implementation of
  /// PTreeNodePtr that also steals the top bits of the pointer.
  ///
  /// The ownership bits are maintained in the update method.
  class RandomPathSearcher final : public ForwardSearcher {
    PForest &processForest;
    RNG &theRNG;

    // Unique bitmask of this searcher
    const uint8_t idBitMask;

  public:
    /// \param processTree The process tree.
    /// \param RNG A random number generator.
    RandomPathSearcher(PForest &processForest, RNG &rng);
    ~RandomPathSearcher() override = default;

    ExecutionState &selectState() override;
    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates) override;
    bool empty() override;
    void printName(llvm::raw_ostream &os) override;
  };


  extern llvm::cl::opt<bool> UseIncompleteMerge;
  class MergeHandler;
  class MergingSearcher final : public ForwardSearcher {
    friend class MergeHandler;

    private:

    std::unique_ptr<ForwardSearcher> baseSearcher;

    /// States that have been paused by the 'pauseState' function
    std::vector<ExecutionState*> pausedStates;

    public:
    /// \param baseSearcher The underlying searcher (takes ownership).
    explicit MergingSearcher(ForwardSearcher *baseSearcher);
    ~MergingSearcher() override = default;

    /// ExecutionStates currently paused from scheduling because they are
    /// waiting to be merged in a klee_close_merge instruction
    std::set<ExecutionState *> inCloseMerge;

    /// Keeps track of all currently ongoing merges.
    /// An ongoing merge is a set of states (stored in a MergeHandler object)
    /// which branched from a single state which ran into a klee_open_merge(),
    /// and not all states in the set have reached the corresponding
    /// klee_close_merge() yet.
    std::vector<MergeHandler *> mergeGroups;

    /// Remove state from the searcher chain, while keeping it in the executor.
    /// This is used here to 'freeze' a state while it is waiting for other
    /// states in its merge group to reach the same instruction.
    void pauseState(ExecutionState &state);

    /// Continue a paused state
    void continueState(ExecutionState &state);

    ExecutionState &selectState() override;
    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates) override;

    bool empty() override;
    void printName(llvm::raw_ostream &os) override;
  };

  /// BatchingSearcher selects a state from an underlying searcher and returns
  /// that state for further exploration for a given time or a given number
  /// of instructions.
  class BatchingSearcher final : public ForwardSearcher {
    std::unique_ptr<ForwardSearcher> baseSearcher;
    time::Span timeBudget;
    unsigned instructionBudget;

    ExecutionState *lastState {nullptr};
    time::Point lastStartTime;
    unsigned lastStartInstructions;

  public:
    /// \param baseSearcher The underlying searcher (takes ownership).
    /// \param timeBudget Time span a state gets selected before choosing a different one.
    /// \param instructionBudget Number of instructions to re-select a state for.
    BatchingSearcher(ForwardSearcher *baseSearcher, time::Span timeBudget, unsigned instructionBudget);
    ~BatchingSearcher() override = default;

    ExecutionState &selectState() override;
    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates) override;
    bool empty() override;
    void printName(llvm::raw_ostream &os) override;
  };

  /// IterativeDeepeningTimeSearcher implements time-based deepening. States
  /// are selected from an underlying searcher. When a state reaches its time
  /// limit it is paused (removed from underlying searcher). When the underlying
  /// searcher runs out of states, the time budget is increased and all paused
  /// states are revived (added to underlying searcher).
  class IterativeDeepeningTimeSearcher final : public ForwardSearcher {
    std::unique_ptr<ForwardSearcher> baseSearcher;
    time::Point startTime;
    time::Span time {time::seconds(1)};
    std::set<ExecutionState*> pausedStates;

  public:
    /// \param baseSearcher The underlying searcher (takes ownership).
    explicit IterativeDeepeningTimeSearcher(ForwardSearcher *baseSearcher);
    ~IterativeDeepeningTimeSearcher() override = default;

    ExecutionState &selectState() override;
    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates) override;
    bool empty() override;
    void printName(llvm::raw_ostream &os) override;
  };

  /// InterleavedSearcher selects states from a set of searchers in round-robin
  /// manner. It is used for KLEE's default strategy where it switches between
  /// RandomPathSearcher and WeightedRandomSearcher with CoveringNew metric.
  class InterleavedSearcher final : public ForwardSearcher {
    std::vector<std::unique_ptr<ForwardSearcher>> searchers;
    unsigned index {1};

  public:
    /// \param searchers The underlying searchers (takes ownership).
    explicit InterleavedSearcher(const std::vector<ForwardSearcher *> &searchers);
    ~InterleavedSearcher() override = default;

    ExecutionState &selectState() override;
    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates) override;
    bool empty() override;
    void printName(llvm::raw_ostream &os) override;
  };

  struct ExecutionStateBinaryRank {
    ExecutionStateBinaryRank () {}
    virtual bool getRank(ExecutionState const &state) = 0;
  };

  struct ExecutionStateIsolationRank : klee::ExecutionStateBinaryRank {
    ExecutionStateIsolationRank () {}
    bool getRank(ExecutionState const &state) override;
  };

  class BinaryRankedSearcher final : public ForwardSearcher {
    ExecutionStateBinaryRank &rank;
    std::unique_ptr<ForwardSearcher> firstRankSearcher;
    std::unique_ptr<ForwardSearcher> secondRankSearcher;

  public:
    explicit BinaryRankedSearcher(ExecutionStateBinaryRank &rank, std::unique_ptr<ForwardSearcher> first, std::unique_ptr<ForwardSearcher> second);
    ExecutionState &selectState() override;
    void update(ExecutionState *current,
                const std::vector<ExecutionState *> &addedStates,
                const std::vector<ExecutionState *> &removedStates) override;
    bool empty() override;
    void printName(llvm::raw_ostream &os) override;
  };

} // klee namespace