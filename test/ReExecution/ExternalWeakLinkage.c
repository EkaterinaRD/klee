// RUN: %clang %s -emit-llvm -g -c -o %t1.bc
// RUN: rm -rf %t.klee-out1
// RUN: rm -rf %t.klee-out2
// RUN: %klee --output-dir=%t.klee-out1 --do-backward-first --write-to-db --exit-on-error %t1.bc
// RUN: %klee --output-dir=%t.klee-out2 --do-backward-first --write-to-db --summary-db=%t.klee-out1/summary.sqlite3 --exit-on-error %t1.bc

#include <assert.h>

void __attribute__((weak)) IAmSoWeak(int);

int main() {
  assert(IAmSoWeak==0);
  return 0;
}
