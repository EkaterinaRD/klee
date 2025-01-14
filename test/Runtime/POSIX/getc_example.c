// RUN: %clang %s -emit-llvm -g %O0opt -c -o %t.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee --output-dir=%t.klee-out --posix-runtime %t.bc --sym-stdin 64
// RUN: test -f %t.klee-out/test000001.ktestjson
// RUN: test -f %t.klee-out/test000002.ktestjson
// RUN: test -f %t.klee-out/test000003.ktestjson
// RUN: test -f %t.klee-out/test000004.ktestjson
// RUN: test -f %t.klee-out/test000005.ktestjson
// RUN: test -f %t.klee-out/test000006.ktestjson
// RUN: test -f %t.klee-out/test000007.ktestjson
// RUN: test -f %t.klee-out/test000008.ktestjson
// RUN: test -f %t.klee-out/test000009.ktestjson
// RUN: test -f %t.klee-out/test000010.ktestjson
// RUN: test -f %t.klee-out/test000011.ktestjson
// RUN: not test -f %t.klee-out/test000012.ktestjson

#include <stdio.h>

int main() {
  unsigned char a = getc(stdin);
  if (a == '0') {
    return 0;
  } else if (a == '1') {
    return 1;
  } else if (a == '2') {
    return 2;
  } else if (a == '3') {
    return 3;
  } else if (a == '4') {
    return 4;
  } else if (a == '5') {
    return 5;
  } else if (a == '6') {
    return 6;
  } else if (a == '7') {
    return 7;
  } else if (a == '8') {
    return 8;
  } else if (a == '9') {
    return 9;
  }
  return -1;
}
