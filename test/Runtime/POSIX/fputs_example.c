// RUN: %clang %s -emit-llvm -g %O0opt -c -o %t.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee --output-dir=%t.klee-out --posix-runtime %t.bc --sym-stdin 64
// RUN: test -f %t.klee-out/test000001.ktestjson
// RUN: test -f %t.klee-out/test000002.ktestjson
// RUN: test -f %t.klee-out/test000003.ktestjson
// RUN: test -f %t.klee-out/test000004.ktestjson
// RUN: test -f %t.klee-out/test000005.ktestjson
// RUN: test -f %t.klee-out/test000006.ktestjson
// RUN: not test -f %t.klee-out/test000007.ktestjson

#include "klee/klee.h"
#include <stdio.h>

char simple_fputs(char c) {
  if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u') {
    char a[] = "Vowel";
    fputs("Vowel", stdout);
    return 'V';
  } else {
    char a[] = "Consonant";
    fputs("Consonant", stdout);
    return 'C';
  }
}

int main() {
  char c;
  klee_make_symbolic(&c, sizeof(char), "c");
  char d = simple_fputs(c);
  return 0;
}
