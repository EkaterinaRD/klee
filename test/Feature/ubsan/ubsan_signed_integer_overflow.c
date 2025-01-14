// RUN: %clang %s -fsanitize=signed-integer-overflow -emit-llvm -g %O0opt -c -o %t.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee --output-dir=%t.klee-out %t.bc 2>&1 | FileCheck %s

#include "klee/klee.h"

int main() {
  signed int x;
  signed int y;
  volatile signed int result;

  klee_make_symbolic(&x, sizeof(x), "x");
  klee_make_symbolic(&y, sizeof(y), "y");

  // CHECK: ubsan_signed_integer_overflow.c:[[@LINE+1]]: overflow on addition
  result = x + y;

  // CHECK: ubsan_signed_integer_overflow.c:[[@LINE+1]]: overflow on subtraction
  result = x - y;

  // CHECK: ubsan_signed_integer_overflow.c:[[@LINE+1]]: overflow on multiplication
  result = x * y;

  return 0;
}
