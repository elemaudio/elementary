# @elemaudio/web-renderer

Elementary is a JavaScript/C++ library for building audio applications.

The `@elemaudio/web-renderer` package provides a specific `Renderer` implementation
for running Elementary applications in web environments using WASM and Web Audio. You'll
need to use this package alongside `@elemaudio/core` to build your application.

Please see the full documentation at [https://www.elementary.audio/](https://www.elementary.audio/)

## Installation

```js
npm install --save @elemaudio/web-renderer
```

## Usage

```js
import { el } from '@elemaudio/core';
import WebRenderer from '@elemaudio/web-renderer';


// Note that many browsers won't let you start an AudioContext before
// some corresponding user gesture. We're ignoring that in this example for brevity,
// but typically you would add an event callback to make or resume your
// AudioContext instance in order to start making noise.
const ctx = new AudioContext();
const core = new WebRenderer();

(async function main() {
  // Here we initialize our WebRenderer instance, returning a promise which resolves
  // to the WebAudio node containing the runtime
  let node = await core.initialize(ctx, {
    numberOfInputs: 0,
    numberOfOutputs: 1,
    outputChannelCount: [2],
  });

  // And connect the resolved node to the AudioContext destination
  node.connect(ctx.destination);

  // Then finally we can render
  core.render(el.cycle(440), el.cycle(441));
})();
```

## License

MIT
