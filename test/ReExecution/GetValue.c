// RUN: %clang -emit-llvm -c -o %t1.bc %s
// RUN: rm -rf %t.klee-out1
// RUN: rm -rf %t.klee-out2
// RUN: %klee --output-dir=%t.klee-out1 --do-backward-first --write-to-db --exit-on-error %t1.bc
// RUN: %klee --output-dir=%t.klee-out2 --do-backward-first --write-to-db --summary-db=%t.klee-out1/summary.sqlite3 --exit-on-error %t1.bc

#include <stdio.h>
#include <assert.h>

int main() {
  int x = klee_int("x");
  klee_assume(x > 10);
  klee_assume(x < 20);

  assert(!klee_is_symbolic(klee_get_value_i32(x)));
  assert(klee_get_value_i32(x) > 10);
  assert(klee_get_value_i32(x) < 20);

  return 0;
}
