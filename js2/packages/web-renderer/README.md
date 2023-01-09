# @elemaudio/web-renderer

The official package for rendering Elementary applications in web browsers using Web Audio.

This package will be used alongside `@elemaudio/core`. Please see the full
documentation at [https://www.elementary.audio/docs](https://www.elementary.audio/docs)

## Installation

```js
npm install --save @elemaudio/web-renderer
```

## Example

```js
import {el} from '@elemaudio/core';
import WebRenderer from '@elemaudio/web-renderer';

const ctx = new AudioContext();
const core = new WebRenderer();

core.on('load', function() {
  core.render(el.cycle(440), el.cycle(441));
});

(async function main() {
  let node = await core.initialize(ctx, {
    numberOfInputs: 0,
    numberOfOutputs: 1,
    outputChannelCount: [2],
  });

  node.connect(ctx.destination);
})();
```

## Usage

```js
import WebRenderer from '@elemaudio/web-renderer';
```

### Constructor

```js
let core = new WebRenderer();
```

No arguments provided; you can construct multiple WebRenderer's and run them through your
Web Audio application as you like. See `initialize()` for connecting to WebAudio.

### initialize

```js
core.initialize(ctx: AudioContext, options: AudioWorkletNodeOptions) : Promise<WebAudioNode>`
```

Initializes the Elementary runtime within the provided AudioContext. Here, Elementary will construct
an AudioWorkletNode in which the Elementary runtime operates, and all subsequent operations will forward
to the AudioWorkletNode.

The second argument here is for configuring the AudioWorkletNode, see the available options [here on MDN](https://developer.mozilla.org/en-US/docs/Web/API/AudioWorkletNode/AudioWorkletNode). Note that this method supports the optional `processorOptions` object for initializing the virtual file system. See Virtual File System below for more details.

This method returns a promise which resolves to the underlying AudioWorkletNode itself, which you may use
to connect into your Web Audio context destination directly, as in the example above.

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

Each `WebRenderer` instance is itself an event emitter with an API matching that of the [Node.js Event Emitter](https://nodejs.org/api/events.html#class-eventemitter)
class.

The renderer will emit events from underlying audio processing graph for nodes such as `el.meter`, `el.snapshot`, etc. See
the reference documentation for each such node for details.

## Virtual File System

When running in a web browser, the Elementary runtime has no access to your file system or network itself.
Therefore, when writing graphs which rely on sample data (such as with `el.sample`, `el.table`, or `el.convolve`),
you must first load the sample data into the runtime using the virtual file system.

If you know your sample data ahead of time, you can load the virtual file system at initialization time using the
`processorOptions` property as follows.

```js
let node = await core.initialize(ctx, {
  numberOfInputs: 0,
  numberOfOutputs: 1,
  outputChannelCount: [2],
  processorOptions: {
    // Maps from String -> Array|Float32Array
    virtualFileSystem: {
      '/your/virtual/file.wav': (new Float32Array(512)).map(() => Math.random()),
    }
  }
});
```

After configuring the core processor this way, you may use `el.sample` or any other node which
reads from file by referencing the corresponding virtual file path that you provided:

```js
core.render(el.sample({path: '/your/virtual/path.wav'}, el.train(1)))
```

If you need to dynamically update the virtual file system after initialization, you may do so
using the `updateVirtualFileSystem` method. As an example, we'll fetch a file here, use the Web Audio API
to decode the file data, and update our renderer when the data is ready.

```js
let res = await fetch('https://ia800407.us.archive.org/9/items/999WavFiles/10.mp3');
let sampleBuffer = await ctx.decodeAudioData(await res.arrayBuffer());

core.updateVirtualFileSystem({
  '/some/new/arbitrary/fileName.wav': sampleBuffer.getChannelData(0),
});

// In this example, after performing the update, we can now `render()` a new graph which references
// our new file data.
core.render(el.sample({path: '/some/new/arbitrary/fileName.wav'}, el.train(1)))
```

Note: Each virtual file system entry maps to a single channel of audio data. To load multi-channel sample
data into the virtual file system, you should enumerate each channel as a differently named virtual file path.

## MIDI

The WebRenderer does not include MIDI support itself. We recommend pairing Elementary with the
[Web MIDI API](https://developer.mozilla.org/en-US/docs/Web/API/Web_MIDI_API), or a third
party library such as [WebMIDI.js](https://webmidijs.org/) to enable MIDI functionality.
