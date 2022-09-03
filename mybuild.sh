#!/bin/bash

cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_SOLVER_Z3=ON \
  -DKLEE_RUNTIME_BUILD_TYPE=Debug \
  -DLLVM_CONFIG_BINARY=/usr/bin/llvm-config-11 \
  -DENABLE_SYSTEM_TESTS=ON \
  -DENABLE_UNIT_TESTS=ON \
  -DGTEST_SRC_DIR=/home/burda/ss22/src/googletest-release-1.11.0/ \
  -DGTEST_INCLUDE_DIR=/home/burda/ss22/src/googletest-release-1.11.0/googletest/include \
  -DENABLE_POSIX_RUNTIME=ON \
  -DENABLE_KLEE_UCLIBC=ON \
  -DKLEE_UCLIBC_PATH=/home/burda/ss22/src/klee-uclibc \
  -DENABLE_KLEE_LIBCXX=ON \
  -DKLEE_LIBCXX_DIR=/home/burda/ss22/src/libcxx/libc++-install-110/ \
  -DKLEE_LIBCXX_INCLUDE_DIR=/home/burda/ss22/src/libcxx/libc++-install-110/include/c++/v1/ \
  -DENABLE_KLEE_EH_CXX=ON \
  -DKLEE_LIBCXXABI_SRC_DIR=/home/burda/ss22/src/libcxx/llvm-110/libcxxabi/ \
  -DLLVMCC=/usr/bin/clang-11 \
  -DLLVMCXX=/usr/bin/clang++-11 \
  ..
  
  
  
