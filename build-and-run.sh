#!/bin/bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

rm -rf "$SCRIPT_DIR/build"
mkdir "$SCRIPT_DIR/build"
cd "$SCRIPT_DIR/build"
conan install -s build_type=Release ..
cmake -DCMAKE_BUILD_TYPE=Release -G Ninja ..
cmake --build .

.//memory-test memcpy
.//memory-test dumb
.//memory-test reference