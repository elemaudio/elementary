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
// but typically you would add a click event callback to make or resume your
// AudioContext instance to start making noise.
const ctx = new AudioContext();
const core = new WebRenderer();

core.on('load', function() {
  core.render(el.cycle(440), el.cycle(441));
});

(async function main() {
  // Here we initialize our WebRenderer instance and connect the resulting
  // Web Audio node to our AudioContext destination.
  let node = await core.initialize(ctx, {
    numberOfInputs: 0,
    numberOfOutputs: 1,
    outputChannelCount: [2],
  });

  node.connect(ctx.destination);
})();
```

## License

MIT
