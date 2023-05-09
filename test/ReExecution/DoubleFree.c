// RUN: %clang %s -emit-llvm %O0opt -c -o %t1.bc
// RUN: rm -rf %t.klee-out1
// RUN: rm -rf %t.klee-out2
// RUN: %klee --output-dir=%t.klee-out1 --write-to-db %t1.bc 2>&1
// RUN: %klee --output-dir=%t.klee-out2 --write-to-db --summary-db=%t.klee-out1/summary.sqlite3 %t1.bc 2>&1 | FileCheck %s
// RUN: test -f %t.klee-out2/test000001.ptr.err

int main() {
  int *x = malloc(4);
  free(x);
  // CHECK: memory error: invalid pointer: free
  free(x);
  return 0;
}
