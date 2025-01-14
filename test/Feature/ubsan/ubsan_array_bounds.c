// RUN: %clang %s -fsanitize=array-bounds -emit-llvm -g %O0opt -c -o %t.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee --output-dir=%t.klee-out %t.bc 2>&1 | FileCheck %s

#include "klee/klee.h"

unsigned int array_index(unsigned int n) {
  unsigned int a[4] = {0};

  // CHECK: ubsan_array_bounds.c:[[@LINE+1]]: index out of bounds
  return a[n];
}

int main() {
  unsigned int x;
  volatile unsigned int result;

  klee_make_symbolic(&x, sizeof(x), "x");

  result = array_index(x);

  return 0;
}
