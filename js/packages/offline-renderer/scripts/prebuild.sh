#!/usr/bin/env bash

set -x
set -e

ROOT_DIR="$(git rev-parse --show-toplevel)"
CURRENT_DIR="$(pwd)"


pushd "$ROOT_DIR"
./scripts/build-wasm.sh -a -o "$CURRENT_DIR/elementary-wasm.js"
popd
