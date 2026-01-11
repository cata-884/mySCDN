#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

mkdir -p $PROJECT_ROOT/build && cd $PROJECT_ROOT/build
cmake -DCMAKE_CXX_FLAGS="-fsyntax-only" ..
make -j$(nproc)