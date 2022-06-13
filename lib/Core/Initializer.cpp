#include "Initializer.h"
#include "ExecutionState.h"
#include "Path.h"
#include "ProofObligation.h"
#include "SearcherUtil.h"
#include "klee/Module/KInstruction.h"
#include "klee/Module/KModule.h"
#include "klee/Solver/Solver.h"
#include "klee/Support/ModuleUtil.h"
#include "klee/Support/OptionCategories.h"

#include <algorithm>
#include <iostream>
#include <set>
#include <stack>
#include <utility>

namespace {
llvm::cl::opt<bool> DebugInitializer(
    "debug-initializer",
    llvm::cl::desc(""),
    llvm::cl::init(false),
    llvm::cl::cat(klee::DebugCat));
}

namespace klee {

std::pair<KInstruction *, std::set<Target>>
ConflictCoreInitializer::selectAction() {
  auto v = conflictCoreInits.front();
  conflictCoreInits.pop();
  return std::make_pair(v.first, v.second);
}

bool ConflictCoreInitializer::empty() {
  return conflictCoreInits.empty();
}

void ConflictCoreInitializer::addPob(ProofObligation *pob) {}

void ConflictCoreInitializer::removePob(ProofObligation *pob) {}

void ConflictCoreInitializer::addConflictInit(const Conflict &conflict, KBlock *target) {
  const Conflict::core_ty &core = conflict.core;
  assert(!core.empty());
  const Path &path = conflict.path;
  std::set<std::pair<KInstruction*, Target>> inits;
  KFunction *mainKF = initInst->parent->parent;
  KInstruction* current = initInst;
  std::set<KFunction *> visited;
  for (size_t pathIndex = 1; pathIndex < path.size(); pathIndex++) {
    if (path.getBlock(pathIndex)->parent != path.getBlock(pathIndex - 1)->parent) {
      if (path.getBlock(pathIndex - 1)->parent == mainKF) {
        auto dismantled = dismantle(current->parent, {path.getBlock(pathIndex - 1)});
        for (auto &blockpair : dismantled) {
          inits.insert(std::make_pair(blockpair.first == current->parent ? current : blockpair.first->instructions[0],
                                      Target(blockpair.second, false)));
          if (blockpair.second->getKBlockType() == KBlockType::Call) {
            KFunction* f = dyn_cast<KCallBlock>(blockpair.second)->getKFunction();
            if (f) {
              inits.insert(std::make_pair(blockpair.second->instructions[0], 
                                          Target(f->entryKBlock, false)));
            }
          }
        }
        inits.insert(std::make_pair(path.getBlock(pathIndex-1)->instructions[0],
                                    Target(path.getBlock(pathIndex), false)));
        current = path.getBlock(pathIndex - 1)->instructions[1];
      }
      if (path.getBlock(pathIndex)->parent != mainKF &&
          !dismantledKFunctions.count(path.getBlock(pathIndex)->parent)) {
        visited.insert(path.getBlock(pathIndex)->parent);
      }
    }
    if (pathIndex == path.size() - 1) {
      if (path.getBlock(pathIndex)->parent == mainKF &&
          path.getBlock(pathIndex) != current->parent) {
        auto dismantled = dismantle(current->parent, {path.getBlock(pathIndex)});
        for (auto &blockpair : dismantled) {
          inits.insert(std::make_pair(
            blockpair.first == current->parent ? current :blockpair.first->instructions[0],
            Target(blockpair.second, false)));
          if (blockpair.second->getKBlockType() == KBlockType::Call) {
            KFunction* f = dyn_cast<KCallBlock>(blockpair.second)->getKFunction();
            if (f) {
              inits.insert(std::make_pair(blockpair.second->instructions[0],
                                          Target(f->entryKBlock,false)));
            }
          }
        }
      }
    }
  }
  for (auto kf : visited) {
    dismantledKFunctions.insert(kf);
    auto dismantled = dismantle(kf->entryKBlock, kf->returnKBlocks);
    for (auto blockpair : dismantled) {
      bool atReturn =
          std::find(kf->returnKBlocks.begin(), kf->returnKBlocks.end(),
                    blockpair.second) != kf->returnKBlocks.end();

      inits.insert(std::make_pair(blockpair.first->instructions[0],
                                  Target(blockpair.second, atReturn)));

      if (blockpair.second->getKBlockType() == KBlockType::Call) {
        KFunction *f = dyn_cast<KCallBlock>(blockpair.second)->getKFunction();
        if (f) {
          inits.insert(std::make_pair(blockpair.second->instructions[0],
                                      Target(f->entryKBlock, false)));
        }
      }
    }
  }
  inits.insert(std::make_pair(path.getFinalBlock()->instructions[0], Target(target, false)));

  std::map<KInstruction *, std::set<Target>> ret;
  if (DebugInitializer) {
    llvm::errs() << "Initialization of entry points and targets: \n";
  }
  for (auto &init : inits) {
    if (!initialized[init.first].count(init.second) &&
        !(!init.second.atReturn && init.first->parent == init.second.targetBlock)) {
      if (DebugInitializer) {
        llvm::errs() << init.first->getIRLocation() << "\n" << init.first->getSourceLocation() << "\n";
        llvm::errs() << init.second.print() << "\n" << init.second.targetBlock->instructions[0]->getSourceLocation();
        llvm::errs() << "\n";
      }
      ret[init.first].insert(init.second);
      initialized[init.first].insert(init.second);
    }
  }
  if (DebugInitializer) {
    llvm::errs() << "\n";
  }
  for (auto &init : ret) {
    conflictCoreInits.push(init);
  }
}

};
 
