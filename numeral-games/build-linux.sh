#!/usr/bin/env bash
# build-linux.sh — сборка portable x86-64 бинарника для Linux
set -e
cd "$(dirname "$0")"
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build . --config Release -j
echo
echo "Готово: build/numeral_games  ($(file -b numeral_games 2>/dev/null || echo binary))"
