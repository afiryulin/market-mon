#!/bin/bash

cd "$(dirname "$0")/.." || exit 1

rm -rf build

cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" \
    -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=thread"

cmake --build build

echo "Running with TSan fix..."

export TSAN_OPTIONS="suppressions=scripts/tsan.suppress"
setarch $(uname -m) -R ./build/server/market_server
