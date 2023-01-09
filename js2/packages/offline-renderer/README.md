# @elemaudio/offline-renderer

The official package for rendering Elementary applications offline, whether in Node.js or in the browser.
Often this is used for file-based processing (reading files, processing them, and writing them to disk), though
the actual encoding/decoding to and from file is not handled here, just the actual audio processing.

Note that, while the command line [Node renderer](https://www.elementary.audio/docs/packages/node-renderer) currently requires the separate
Elementary command line binary, this renderer actually runs properly in Node.js itself.

This package will be used alongside `@elemaudio/core`. Please see the full
documentation at [https://www.elementary.audio/docs](https://www.elementary.audio/docs)

## Installation

```js
npm install --save @elemaudio/offline-renderer
```

**Note**: As of `v1.0.11`, the offline renderer requires Node v18, or Node v16+ with the `--experimental-wasm-eh` flag set.

## Example

```js
import {el} from '@elemaudio/core';
import OfflineRenderer from '@elemaudio/offline-renderer';

(async function main() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 0,
    numOutputChannels: 1,
    sampleRate: 44100,
  });

  // Our sample data for processing.
  //
  // We are not expecting any inputs, hence the empty array, but we are expecting
  // an output buffer to write into.
  let inps = [];
  let outs = [new Float32Array(44100 * 10)]; // 10 seconds of output data

  // Render our processing graph
  core.render(el.cycle(440));

  // Pushing samples through the graph. After this call, the buffer in `outs` will
  // have the desired output data which can then be saved to file if you like.
  core.process(inps, outs);
})();
```

## Usage

```js
import Offline from '@elemaudio/offline-renderer';
```

### Constructor

```js
let core = new OfflineRenderer();
```

No arguments provided; you can construct multiple OfflineRenderers and use them however you like,
even to generate buffers that will be loaded into other OfflineRenderers.

### initialize

```js
core.initialize(options: Object): Promise<void>
```

Initializes the Elementary offline runtime. In most other Elementary applications you'll rely on listening
for the "load" event to fire after initialization. With the OfflineRenderer you can be sure that after `initialize`
resolves, the runtime is ready to render.

The options object expects the following properties:

* `numInputChannels: number` – default 0
* `numOutputChannels: number` – default 2
* `sampleRate: number` – default 44100
* `blockSize: number` – default 512
* `virtualFileSystem: Object<string, Array<number>|Float32Array>` – default `{}`

### render

```js
core.render(...args: Array<NodeRepr_t | number>) : RenderStats;
```

Performs the reconciliation process for rendering your desired audio graph. This method expects one argument
for each available output channel. That is, if you want to render a stereo graph, you will invoke this method
with two arguments: `core.render(leftOut, rightOut)`.

The `RenderStats` object returned by this call provides some insight into what happened during the reconciliation
process: how many new nodes and edges were added to the graph, how long it took, etc.

### updateVirtualFileSystem

```js
core.updateVirtualFileSystem(Object<string, Array | Float32Array>);
```

Use this method to dynamically update the buffers available in the virtual file system after initialization. See the
Virtual File System section below for more details.

### reset

```js
core.reset();
```

Resets internal nodes and buffers back to their initial state.

## Events

Each `OfflineRenderer` instance is itself an event emitter with an API matching that of the [Node.js Event Emitter](https://nodejs.org/api/events.html#class-eventemitter)
class.

The renderer will emit events from underlying audio processing graph for nodes such as `el.meter`, `el.snapshot`, etc. See
the reference documentation for each such node for details.

## Virtual File System

When running offline, either in a web browser or in Node.js, the Elementary runtime has no access to your file system or network itself.
Therefore, when writing graphs which rely on sample data (such as with `el.sample`, `el.table`, or `el.convolve`),
you must first load the sample data into the runtime using the virtual file system.

If you know your sample data ahead of time, you can load the virtual file system at initialization time using the
`virtualFileSystem` property as follows.

```js
await core.initialize(ctx, {
  numInputChannels: 0,
  numOutputChannels: 2,
  virtualFileSystem: {
    '/your/virtual/file.wav': (new Float32Array(512)).map(() => Math.random()),
  }
});
```

After configuring the core processor this way, you may use `el.sample` or any other node which
reads from file by referencing the corresponding virtual file path that you provided:

```js
core.render(el.sample({path: '/your/virtual/path.wav'}, el.train(1)))
```

If you need to dynamically update the virtual file system after initialization, you may do so
using the `updateVirtualFileSystem` method.

```js
await core.initialize({
  numInputChannels: 0,
  numOutputChannels: 1,
});

let inps = [];
let outs = [new Float32Array(44100 * 10)]; // 10 seconds of output data

// Render our processing graph
core.render(el.cycle(440));

// Pushing samples through the graph. After this call, the buffer in `outs` will
// have the desired output data which can then be saved to file if you like.
core.process(inps, outs);

// Perhaps now we have some new sample data that needs to be loaded, maybe in response
// to an HTTP request (though admittedly it might be both easier and better to just make
// a new OfflineRenderer if you're doing something like HTTP request handling!)
core.updateVirtualFileSystem({
  '/some/new/arbitrary/fileName.wav': myNewSampleData,
});

// In this example, after performing the update, we can now `render()` a new graph which references
// our new file data.
core.render(el.sample({path: '/some/new/arbitrary/fileName.wav'}, el.train(1)))

// Process again; this will overwrite what's already in `out`
core.process(inps, outs);
```

Note: Each virtual file system entry maps to a single channel of audio data. To load multi-channel sample
data into the virtual file system, you should enumerate each channel as a differently named virtual file path.

## MIDI

The OfflineRenderer does not include MIDI support itself.
