// RUN: %clang -emit-llvm -c -o %t1.bc %s
// RUN: rm -rf %t.klee-out1
// RUN: rm -rf %t.klee-out2
// RUN: %klee --output-dir=%t.klee-out1 --do-backward-first --exit-on-error --write-to-db %t1.bc
// RUN: %klee --output-dir=%t.klee-out2 --do-backward-first --exit-on-error --write-to-db --summary-db=%t.klee-out1/summary.sqlite3 %t1.bc

#include <stdio.h>

#define ADDRESS ((int*) 0x0080)

int main() {
  klee_define_fixed_object(ADDRESS, 4);
  
  int *p = ADDRESS;

  *p = 10;
  printf("*p: %d\n", *p);

  return 0;
}
