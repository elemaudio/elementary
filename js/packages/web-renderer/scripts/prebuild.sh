#!/usr/bin/env bash

set -x
set -e

ROOT_DIR="$(git rev-parse --show-toplevel)"
CURRENT_DIR="$(pwd)"


pushd "$ROOT_DIR"
./scripts/build-wasm.sh -o "$CURRENT_DIR/raw/elementary-wasm.js"
popd
