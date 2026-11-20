#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=$(mktemp -d)
trap 'rm -rf "$BUILD_DIR"' EXIT HUP INT TERM

# SANITIZE=1 enables ASan and UBSan, independently of the full GTK build.
set -- -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Werror
if [ "${SANITIZE:-0}" = "1" ]; then
  set -- "$@" -g -fsanitize=address,undefined -fno-omit-frame-pointer
fi

"${CXX:-c++}" "$@" \
  "$ROOT/cpp/object_model.cpp" "$ROOT/cpp/object_model_tests.cpp" \
  -o "$BUILD_DIR/inde_object_model_tests"
"$BUILD_DIR/inde_object_model_tests"
