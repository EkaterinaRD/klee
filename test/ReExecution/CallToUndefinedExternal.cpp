// RUN: %clangxx %s -emit-llvm -g -c -o %t1.bc
// RUN: rm -rf %t.klee-out1
// RUN: rm -rf %t.klee-out2
// RUN: %klee --output-dir=%t.klee-out1 --execution-mode=forward --write-to-db %t1.bc 2>&1
// RUN: %klee --output-dir=%t.klee-out2 --execution-mode=forward --write-to-db --summary-db=%t.klee-out1/summary.sqlite3 %t1.bc 2>&1 | FileCheck %s
// RUN: test -f %t.klee-out2/test000001.external.err

extern "C" void poof(void);

int main() {
  // CHECK: failed external call: poof
  poof();

  return 0;
}
