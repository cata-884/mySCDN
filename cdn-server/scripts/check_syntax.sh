#!/bin/bash

source "$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )/_common.sh"

mkdir -p "$BUILD_DIR" && cd "$BUILD_DIR"
cmake -DCMAKE_CXX_FLAGS="-fsyntax-only" ..
make -j$(nproc)