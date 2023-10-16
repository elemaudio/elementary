# @elemaudio/core

Elementary is a JavaScript/C++ library for building audio applications.

The `@elemaudio/core` package provides the standard library for composing
audio processing nodes, as well as the core graph reconciling and rendering utilities. Often this
will be used with one of the provided renderers, `@elemaudio/web-renderer` or `@elemaudio/offline-renderer`.

Please see the full documentation at [https://www.elementary.audio/](https://www.elementary.audio/)

## Installation

```js
npm install --save @elemaudio/core
```

## Usage

```js
import { el, Renderer } from '@elemaudio/core';


// Here we're using a default Renderer instance, so it's our responsibility to
// send the instruction batches to the underlying engine
let core = new Renderer((batch) => {
  // Send the instruction batch somewhere: you can set up whatever message
  // passing channel you want!
  console.log(batch);
});

// Now we can write and render audio. How about some binaural beating
// with two detuned sine tones in the left and right channel:
core.render(el.cycle(440), el.cycle(441));
```

## License

MIT
