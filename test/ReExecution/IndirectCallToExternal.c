// RUN: %clang %s -emit-llvm -g -c -o %t1.bc
// RUN: rm -rf %t.klee-out1
// RUN: rm -rf %t.klee-out2
// RUN: %klee --output-dir=%t.klee-out1 --execution-mode=forward --write-to-db %t1.bc
// RUN: %klee --output-dir=%t.klee-out2 --execution-mode=forward --write-to-db --summary-db=%t.klee-out1/summary.sqlite3 %t1.bc

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main() {
  int (*scmp)(char*,char*) = strcmp;

  assert(scmp("hello","hi") < 0);

  return 0;
}
