#!/usr/bin/env bash

set -x
set -e

SCRIPT_DIR="$PWD/$(dirname "$0")"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"


el__build() {
    # We build outside the source dir here because Docker inside of github actions with
    # file system mounting gets all weird. No problems locally either way, so, we do it like this
    mkdir -p "/elembuild/wasm/"
    pushd "/elembuild/wasm/"

    ELEM_BUILD_ASYNC="${ELEM_BUILD_ASYNC:-0}" emcmake cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DONLY_BUILD_WASM=ON \
        -DCMAKE_CXX_FLAGS="-O3" \
        /src

    emmake make

    # Because we build out of the source dir, copy the resulting file back to the
    # source dir so that it exists outside the container
    mkdir -p /src/build/out/
    cp /elembuild/wasm/wasm/elementary-wasm.js /src/build/out/elementary-wasm.js

    popd
}

el__main() {
    # If the first positional argument matches one of the namespaced
    # subcommands above, pop the first positional argumen off the list
    # and invoke the subcommand with the remainder.
    if declare -f "el__${1//-/_}" > /dev/null; then
        fn="el__${1//-/_}"
        shift;
        "$fn" "$@"
    else
        # Else we're running our top-level main, for which we, by default, invoke
        # the build command from within an emscripten/emsdk docker container.
        local OUTPUT_FILENAME=""
        local ELEM_BUILD_ASYNC=0

        while getopts ao: opt; do
            case $opt in
                o)  OUTPUT_FILENAME="$OPTARG";;
                a)  ELEM_BUILD_ASYNC=1;;
            esac
        done

        shift "$((OPTIND - 1))"

        if [ -z "$OUTPUT_FILENAME" ]; then
            echo "Error: where are we outputting to?"
            exit 1
        fi

        docker run \
          -v $(pwd):/src \
          --env ELEM_BUILD_ASYNC="$ELEM_BUILD_ASYNC" \
          emscripten/emsdk:3.1.52 \
          ./scripts/build-wasm.sh build

        # Then we copy the resulting file over to the website directory where
        # we need it
        cp $ROOT_DIR/build/out/elementary-wasm.js $OUTPUT_FILENAME
    fi
}

el__main "$@"
