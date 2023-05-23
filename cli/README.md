# cli

This directory holds a small command line project which runs Elementary direct to
the default audio device, and next to an embedded Quickjs JavaScript interpreter.

The cli tool here is not meant to be a fully featured, end-user product, but rather
a concise demonstration of integrating Elementary's native engine, and a simple
environment for testing, benchmarking, and profiling Elementary itself, or any
custom native nodes that you may be interested in building. Still, we have a few
examples here that you can compile and run to get a sense of the cli project.

## Building

### Native

To build the cli tool, you can use CMake from the top-level directory.

```bash
# Get Elementary
git clone https://github.com/elemaudio/elementary.git --recurse-submodules
cd elementary

# Initialize the CMake directory
mkdir build/
cd build/

# Choose your favorite CMake generator here
cmake -G Xcode -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15 ../

# Build the binaries
cmake --build .
```

### JavaScript

Next, in order to run the cli we need to direct it to a JavaScript file to run,
much like one would expect when using Node.js or similar. However, because our cli
runs the JavaScript inside a Quickjs environment, we need to resolve any module
imports into a single bundle before trying to run it. To do that, we have a simple
`esbuild` workflow inside the `examples/` directory.

```bash
cd cli/examples/
npm install
npm run build
```

After these few steps, you'll see bundled files in `examples/dist/`. These files can
then be run with the command line to hear them:

```bash
# Your path to the elemcli binary might be different depending on your build
# directory structure
./build/cli/Debug/elemcli examples/dist/00_HelloSine.js
```
