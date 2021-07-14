#include "klee/klee.h"

long lroundf(float x) {
    return klee_rintf(x);
}

long lround(double x) {
    return klee_rint(x);
}

long lroundl(long double x) {
    return klee_rintl(x);
}

long long llroundf(float x) {
    return klee_rintf(x);
}

long long llround(double x) {
    return klee_rint(x);
}

long long llroundl(long double x) {
    return klee_rintl(x);
}