#ifndef KLEE_SUBSCRIBER_H
#define KLEE_SUBSCRIBER_H

#include "SearcherUtil.h"
#include "ProofObligation.h"

namespace klee {
class Subscriber {
public:
  virtual void update(ref<ActionResult>) = 0;
  //virtual void closeProofObligation(ProofObligation *) = 0;
  //virtual void addRoot(ExecutionState *) = 0;
};
} // namespace klee


#endif /*KLEE_SUBSCRIBER_H*/