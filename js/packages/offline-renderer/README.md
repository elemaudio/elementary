# @elemaudio/offline-renderer

Elementary is a JavaScript/C++ library for building audio applications.

The `@elemaudio/offline-renderer` package provides a specific `Renderer` implementation
for running Elementary applications in Node.js and web environments using WASM. Offline
rendering is intended for processing data with no associated real-time audio driver, such
as file processing. You'll need to use this package alongside `@elemaudio/core` to build
your application.


Please see the full documentation at [https://www.elementary.audio/](https://www.elementary.audio/)

## Installation

```js
npm install --save @elemaudio/offline-renderer
```

## Usage

```js
import { el } from '@elemaudio/core';
import OfflineRenderer from '@elemaudio/offline-renderer';

(async function main() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 0,
    numOutputChannels: 1,
    sampleRate: 44100,
  });

  // Our sample data for processing: an empty input and a silent 10s of
  // output data to be written into.
  let inps = [];
  let outs = [new Float32Array(44100 * 10)]; // 10 seconds of output data

  // Render our processing graph
  core.render(el.cycle(440));

  // Pushing samples through the graph. After this call, the buffer in `outs` will
  // have the desired output data which can then be saved to file if you like.
  core.process(inps, outs);
})();
```

## License

MIT
