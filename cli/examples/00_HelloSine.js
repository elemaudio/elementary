import {Renderer, el} from '@elemaudio/core';


// This example is the "Hello, world!" of writing audio processes in Elementary, and is
// intended to be run by the simple cli tool provided in the repository.
//
// Because we know that our cli will open the audio device with a sample rate of 44.1kHz,
// we can simply create a generic Renderer straight away and ask it to render our basic
// example.
//
// The signal we're generating here is a simple sine tone via `el.cycle` at 440Hz in the left
// channel and 441Hz in the right, creating some interesting binaural beating. Each sine tone is
// then multiplied by 0.3 to apply some simple gain before going to the output. That's it!
let core = new Renderer((batch) => {
  __postNativeMessage__(JSON.stringify(batch));
});

let stats = core.render(
  el.mul(0.3, el.cycle(440)),
  el.mul(0.3, el.cycle(441)),
);

console.log(stats);
