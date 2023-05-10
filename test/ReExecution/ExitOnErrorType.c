// RUN: %clang %s -g -emit-llvm %O0opt -c -o %t1.bc
// RUN: rm -rf %t.klee-out1
// RUN: rm -rf %t.klee-out2
// RUN: %klee --output-dir=%t.klee-out1 --do-backward-first --write-to-db -exit-on-error-type  Assert %t1.bc 2>&1
// RUN: %klee --output-dir=%t.klee-out2 --do-backward-first --write-to-db --summary-db=%t.klee-out1/summary.sqlite3 -exit-on-error-type Assert %t1.bc 2>&1

#include "klee/klee.h"

#include <assert.h>

int main() {
  assert(klee_int("assert"));

  while (1) {
  }

  return 0;
}
