// RUN: %clang -emit-llvm -g -c %s -o %t.bc
// RUN: rm -rf %t.klee-out1
// RUN: rm -rf %t.klee-out2
// RUN: %klee --output-dir=%t.klee-out1 --execution-mode=forward --write-to-db  %t.bc > %t.log1
// RUN: %klee --output-dir=%t.klee-out2 --execution-mode=forward --write-to-db --summary-db=%t.klee-out1/summary.sqlite3  %t.bc > %t.log2
// RUN: grep "3.30* -1.10* 2.420*" %t.log2

#include <stdio.h>

float fadd(float a, float b) {
  return a + b;
}

float fsub(float a, float b) {
  return a - b;
}

float fmul(float a, float b) {
  return a * b;
}

int main() {
  printf("%f %f %f\n", fadd(1.1, 2.2), fsub(1.1, 2.2), fmul(1.1, 2.2));
  return 0;
}
