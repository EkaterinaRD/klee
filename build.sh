#!/bin/bash
#
# This source file has been modified by Huawei. Copyright (c) 2021
#

set -e
set -o pipefail
mkdir -p build
cd build
$UTBOT_CMAKE_BINARY -G Ninja \
  -DCMAKE_PREFIX_PATH=$UTBOT_INSTALL_DIR/lib/cmake/z3 \
  -DCMAKE_LIBRARY_PATH=$UTBOT_INSTALL_DIR/lib \
  -DCMAKE_INCLUDE_PATH=$UTBOT_INSTALL_DIR/include \
  -DENABLE_SOLVER_Z3=ON \
  -DENABLE_POSIX_RUNTIME=ON \
  -DENABLE_FLOATING_POINT=ON \
  -DENABLE_FP_RUNTIME=ON \
  -DENABLE_KLEE_UCLIBC=ON \
  -DLLVM_CONFIG_BINARY=$UTBOT_INSTALL_DIR/bin/llvm-config \
  -DKLEE_UCLIBC_PATH=$UTBOT_ALL/uclibc/ \
  -DENABLE_UNIT_TESTS=ON \
  -DGTEST_SRC_DIR=$UTBOT_ALL/gtest \
  -DGTEST_INCLUDE_DIR=$UTBOT_ALL/gtest/googletest/include \
  -DCMAKE_INSTALL_PREFIX=$UTBOT_ALL/klee/ \
    ..
$UTBOT_CMAKE_BINARY --build .
ninja check
$UTBOT_CMAKE_BINARY --install .