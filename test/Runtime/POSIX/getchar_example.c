// RUN: %clang %s -emit-llvm -g %O0opt -c -o %t.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee --output-dir=%t.klee-out --posix-runtime %t.bc --sym-stdin 64
// RUN: test -f %t.klee-out/test000001.ktestjson
// RUN: test -f %t.klee-out/test000002.ktestjson
// RUN: test -f %t.klee-out/test000003.ktestjson
// RUN: not test -f %t.klee-out/test000004.ktestjson

#include <stdio.h>

int main() {
  unsigned char a = getchar();
  unsigned char b = getchar();

  if (a + b < 0) {
    return -1;
  }

  if (a + b < 100) {
    return 0;
  } else if (a + b < 200) {
    return 1;
  } else {
    return 2;
  }
}
