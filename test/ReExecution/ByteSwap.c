// RUN: %clang %s -emit-llvm %O0opt -c -o %t1.bc
// RUN: rm -rf %t.klee-out1
// RUN: rm -rf %t.klee-out2
// RUN: %klee --output-dir=%t.klee-out1 --execution-mode=forward --libc=klee --exit-on-error --write-to-db %t1.bc
// RUN: %klee --output-dir=%t.klee-out2 --execution-mode=forward --libc=klee --exit-on-error --write-to-db --summary-db=%t.klee-out1/summary.sqlite3 %t1.bc

#include <arpa/inet.h>
#include <assert.h>

int main() {
  
  uint32_t n = 0;
  klee_make_symbolic(&n, sizeof(n), "n");
  
  uint32_t h = ntohl(n);
  assert(htonl(h) == n);
  
  return 0;
}
