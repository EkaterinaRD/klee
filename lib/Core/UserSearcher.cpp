//===-- UserSearcher.cpp --------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "UserSearcher.h"

#include "Executor.h"
#include "MergeHandler.h"
#include "ForwardSearcher.h"

#include "klee/Support/ErrorHandling.h"

#include "llvm/Support/CommandLine.h"

using namespace llvm;
using namespace klee;

namespace {
llvm::cl::OptionCategory
    SearchCat("Search options", "These options control the search heuristic.");

cl::list<ForwardSearcher::CoreSearchType> CoreSearch(
    "search",
    cl::desc("Specify the search heuristic (default=random-path interleaved "
             "with nurs:covnew)"),
    cl::values(
        clEnumValN(ForwardSearcher::DFS, "dfs", "use Depth First Search (DFS)"),
        clEnumValN(ForwardSearcher::BFS, "bfs",
                   "use Breadth First Search (BFS), where scheduling decisions "
                   "are taken at the level of (2-way) forks"),
        clEnumValN(ForwardSearcher::RandomState, "random-state",
                   "randomly select a state to explore"),
        clEnumValN(ForwardSearcher::RandomPath, "random-path",
                   "use Random Path Selection (see OSDI'08 paper)"),
        clEnumValN(ForwardSearcher::NURS_CovNew, "nurs:covnew",
                   "use Non Uniform Random Search (NURS) with Coverage-New"),
        clEnumValN(ForwardSearcher::NURS_MD2U, "nurs:md2u",
                   "use NURS with Min-Dist-to-Uncovered"),
        clEnumValN(ForwardSearcher::NURS_Depth, "nurs:depth", "use NURS with depth"),
        clEnumValN(ForwardSearcher::NURS_RP, "nurs:rp", "use NURS with 1/2^depth"),
        clEnumValN(ForwardSearcher::NURS_ICnt, "nurs:icnt",
                   "use NURS with Instr-Count"),
        clEnumValN(ForwardSearcher::NURS_CPICnt, "nurs:cpicnt",
                   "use NURS with CallPath-Instr-Count"),
        clEnumValN(ForwardSearcher::NURS_QC, "nurs:qc", "use NURS with Query-Cost")
            KLEE_LLVM_CL_VAL_END),
    cl::cat(SearchCat));

cl::opt<bool> UseIterativeDeepeningTimeSearch(
    "use-iterative-deepening-time-search",
    cl::desc(
        "Use iterative deepening time search (experimental) (default=false)"),
    cl::init(false),
    cl::cat(SearchCat));

cl::opt<bool> UseBatchingSearch(
    "use-batching-search",
    cl::desc("Use batching searcher (keep running selected state for N "
             "instructions/time, see --batch-instructions and --batch-time) "
             "(default=false)"),
    cl::init(false),
    cl::cat(SearchCat));

cl::opt<unsigned> BatchInstructions(
    "batch-instructions",
    cl::desc("Number of instructions to batch when using "
             "--use-batching-search.  Set to 0 to disable (default=10000)"),
    cl::init(10000),
    cl::cat(SearchCat));

cl::opt<std::string> BatchTime(
    "batch-time",
    cl::desc("Amount of time to batch when using "
             "--use-batching-search.  Set to 0s to disable (default=5s)"),
    cl::init("5s"),
    cl::cat(SearchCat));

} // namespace

void klee::initializeSearchOptions() {
  // default values
  if (CoreSearch.empty()) {
    if (UseMerge){
      CoreSearch.push_back(ForwardSearcher::NURS_CovNew);
      klee_warning("--use-merge enabled. Using NURS_CovNew as default searcher.");
    } else {
      CoreSearch.push_back(ForwardSearcher::RandomPath);
      CoreSearch.push_back(ForwardSearcher::NURS_CovNew);
    }
  }
}

bool klee::userSearcherRequiresMD2U() {
  return (std::find(CoreSearch.begin(), CoreSearch.end(), ForwardSearcher::NURS_MD2U) != CoreSearch.end() ||
          std::find(CoreSearch.begin(), CoreSearch.end(), ForwardSearcher::NURS_CovNew) != CoreSearch.end() ||
          std::find(CoreSearch.begin(), CoreSearch.end(), ForwardSearcher::NURS_ICnt) != CoreSearch.end() ||
          std::find(CoreSearch.begin(), CoreSearch.end(), ForwardSearcher::NURS_CPICnt) != CoreSearch.end() ||
          std::find(CoreSearch.begin(), CoreSearch.end(), ForwardSearcher::NURS_QC) != CoreSearch.end());
}


ForwardSearcher *getNewSearcher(ForwardSearcher::CoreSearchType type, RNG &rng, PForest &processForest) {
  ForwardSearcher *searcher = nullptr;
  switch (type) {
    case ForwardSearcher::DFS: searcher = new DFSSearcher(); break;
    case ForwardSearcher::BFS: searcher = new BFSSearcher(); break;
    case ForwardSearcher::RandomState: searcher = new RandomSearcher(rng); break;
    case ForwardSearcher::RandomPath: searcher = new RandomPathSearcher(processForest, rng); break;
    case ForwardSearcher::NURS_CovNew: searcher = new WeightedRandomSearcher(WeightedRandomSearcher::CoveringNew, rng); break;
    case ForwardSearcher::NURS_MD2U: searcher = new WeightedRandomSearcher(WeightedRandomSearcher::MinDistToUncovered, rng); break;
    case ForwardSearcher::NURS_Depth: searcher = new WeightedRandomSearcher(WeightedRandomSearcher::Depth, rng); break;
    case ForwardSearcher::NURS_RP: searcher = new WeightedRandomSearcher(WeightedRandomSearcher::RP, rng); break;
    case ForwardSearcher::NURS_ICnt: searcher = new WeightedRandomSearcher(WeightedRandomSearcher::InstCount, rng); break;
    case ForwardSearcher::NURS_CPICnt: searcher = new WeightedRandomSearcher(WeightedRandomSearcher::CPInstCount, rng); break;
    case ForwardSearcher::NURS_QC: searcher = new WeightedRandomSearcher(WeightedRandomSearcher::QueryCost, rng); break;
  }

  return searcher;
}

std::unique_ptr<ForwardSearcher> klee::constructUserSearcher(Executor &executor) {

  std::unique_ptr<ForwardSearcher> searcher(getNewSearcher(CoreSearch[0], executor.theRNG, *executor.processForest));

  if (CoreSearch.size() > 1) {
    std::vector<ForwardSearcher *> s;
    s.push_back(searcher.release());

    for (unsigned i = 1; i < CoreSearch.size(); i++)
      s.push_back(getNewSearcher(CoreSearch[i], executor.theRNG, *executor.processForest));

    searcher.reset(new InterleavedSearcher(s));
  }

  if (UseBatchingSearch) {
    searcher.reset(new BatchingSearcher(searcher.release(), time::Span(BatchTime),
                                    BatchInstructions));
  }

  if (UseIterativeDeepeningTimeSearch) {
    searcher.reset(new IterativeDeepeningTimeSearcher(searcher.release()));
  }

  if (UseMerge) {
    auto mt = std::make_unique<MergingSearcher>(searcher.release());
    executor.setMergingSearcher(mt.get());
    searcher = std::move(mt);
  }

  llvm::raw_ostream &os = executor.getHandler().getInfoStream();

  os << "BEGIN searcher description\n";
  searcher->printName(os);
  os << "END searcher description\n";

  return searcher;
}
