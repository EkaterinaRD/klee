// RUN: %clang %s -emit-llvm %O0opt -c -o %t1.bc
// RUN: rm -rf %t.klee-out1
// RUN: rm -rf %t.klee-out2
// RUN: %klee --optimize=false --equality-substitution=true --output-dir=%t.klee-out1 --write-to-db %t1.bc
// RUN: %klee --optimize=false --equality-substitution=true --output-dir=%t.klee-out2 --write-to-db --summary-db=%t.klee-out1/summary.sqlite3 %t1.bc
// RUN: grep "total queries = 2" %t.klee-out2/info

#include <assert.h>

int main() {
  unsigned char x, y;

  klee_make_symbolic(&x, sizeof x, "x");
  
  y = x;

  // should be exactly two queries (prove x is/is not 10)
  // eventually should be 0 when we have fast solver
  if (x==10) {
    assert(y==10);
  }

  klee_silent_exit(0);
  return 0;
}
