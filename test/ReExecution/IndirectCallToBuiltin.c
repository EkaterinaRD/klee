// RUN: %clang %s -emit-llvm -g -c -o %t1.bc
// RUN: rm -rf %t.klee-out1
// RUN: rm -rf %t.klee-out2
// RUN: %klee --output-dir=%t.klee-out1 --do-backward-first --write-to-db %t1.bc
// RUN: %klee --output-dir=%t.klee-out2 --do-backward-first --write-to-db --summary-db=%t.klee-out1/summary.sqlite3 %t1.bc

#include <stdlib.h>
#include <stdio.h>

int main() {
  void *(*allocator)(size_t) = malloc;
  int *mem = allocator(10);

  printf("mem: %p\n", mem);
  printf("mem[0]: %d\n", mem[0]);

  return 0;
}
