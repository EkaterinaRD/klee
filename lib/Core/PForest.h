//===-- PForest.h -------------------------------------------------*- C++
//-*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===------------------------------------------------------------------------===//

#ifndef KLEE_PFOREST_H
#define KLEE_PFOREST_H

#include "PTree.h"
#include "Subscriber.h"
#include "SearcherUtil.h"

namespace klee {
class ExecutionState;
class PTreeNode;
class PTree;

class PForest : public Subscriber {
  // Number of registered ID
  int registeredIds = 0;
  std::map<uint32_t, PTree *> trees;

public:
  PForest() = default;
  ~PForest();
  void update(ref<ActionResult> result) override;
  void closeProofObligation(ProofObligation *pob) override {}
  void addRoot(ExecutionState *initialState);
  void attach(PTreeNode *node, ExecutionState *leftState,
              ExecutionState *rightState);
  void remove(PTreeNode *node);
  const std::map<uint32_t, PTree *> &getPTrees() { return trees; }
  void dump(llvm::raw_ostream &os);
  std::uint8_t getNextId() {
    std::uint8_t id = 1 << registeredIds++;
    if (registeredIds > PtrBitCount) {
      klee_error("PForest cannot support more than %d RandomPathSearchers",
                 PtrBitCount);
    }
    return id;
  }
};
} // namespace klee

#endif /* KLEE_PFOREST_H */
