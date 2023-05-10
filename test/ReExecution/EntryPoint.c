// RUN: %clang -emit-llvm -g -c %s -o %t.bc
// RUN: rm -rf %t.klee-out1
// RUN: rm -rf %t.klee-out2
// RUN: %klee --output-dir=%t.klee-out1 --execution-mode=forward --entry-point=other_main --write-to-db %t.bc > %t.log1
// RUN: %klee --output-dir=%t.klee-out2 --execution-mode=forward --entry-point=other_main --write-to-db --summary-db=%t.klee-out1/summary.sqlite3 %t.bc > %t.log2
// RUN: grep "Hello World" %t.log2

#include <stdio.h>

int other_main() {
  printf("Hello World\n");
  return 0;
}
