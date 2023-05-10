// Darwin does not have strong aliases.
// REQUIRES: not-darwin
// RUN: %clang %s -emit-llvm -g -c -o %t1.bc
// RUN: rm -rf %t.klee-out1
// RUN: rm -rf %t.klee-out2
// RUN: %klee --output-dir=%t.klee-out1 --do-backward-first --exit-on-error --write-to-db %t1.bc
// RUN: %klee --output-dir=%t.klee-out2 --do-backward-first --exit-on-error --write-to-db --summary-db=%t.klee-out1/summary.sqlite3 %t1.bc

#include <assert.h>

// alias with bitcast
// NOTE: this does not have to be before b is known
extern short d __attribute__((alias("b")));

// alias for global
int b = 52;
extern int a __attribute__((alias("b")));

// alias for alias
// NOTE: this does not have to be before foo is known
extern int foo2() __attribute__((alias("foo")));

// alias for function
int __foo() { return 52; }
extern int foo() __attribute__((alias("__foo")));

// alias without bitcast
extern int foo3(void) __attribute__((alias("__foo")));

int *c = &a;

int main() {
  assert(a == 52);
  assert(*c == 52);
  assert((int)d == 52);

  assert(c == &b);
  assert((int*)&d != &b);

  assert(foo() == 52);
  assert(foo2() == 52);
  assert(foo3() == 52);

  assert(foo != __foo);
  assert(foo2 != __foo);
  assert(foo3 == __foo);

  return 0;
}
