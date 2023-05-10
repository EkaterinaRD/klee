// RUN: %clang %s -emit-llvm -g -c -o %t1.bc
// RUN: rm -rf %t.klee-out1
// RUN: rm -rf %t.klee-out2
// RUN: %klee --output-dir=%t.klee-out1 --execution-mode=forward --exit-on-error --write-to-db %t1.bc
// RUN: %klee --output-dir=%t.klee-out2 --execution-mode=forward --exit-on-error --write-to-db --summary-db=%t.klee-out1/summary.sqlite3 %t1.bc

#include <assert.h>

int main(int argc, char **argv, char **envp) {
  unsigned i;
  assert(argv[argc] == 0);
  printf("argc: %d, argv: %p, envp: %p\n", argc, argv, envp);
  printf("--environ--\n");
  int haspwd = 0;
  for (i=0; envp[i]; i++) {
    printf("%d: %s\n", i, envp[i]);
    haspwd |= strncmp(envp[i], "PWD=", 4)==0;
  }
  assert(haspwd);
  return 0;
}
