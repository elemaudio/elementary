import {Renderer, el} from '@elemaudio/core';


// This example builds upon the HelloSine example to implement some very basic FM synthesis,
// and builds in some simple pattern sequencing to create an interesting arp.
let core = new Renderer((batch) => {
  __postNativeMessage__(JSON.stringify(batch));
});

// We start here with a couple simple constants: these map a few common note names
// to their respective frequencies, from which we'll construct a simple arpeggio pattern.
const e4 = 329.63 * 0.5;
const b4 = 493.88 * 0.5;
const e5 = 659.26 * 0.5;
const g5 = 783.99 * 0.5;

// Here we put together the patterns: each just a simple array of the notes above. We
// have two patterns here each with a slightly different note order, and the latter of
// which is slightly detuned from the original frequencies.
const s1 = [e4, b4, e5, e4, g5];
const s2 = [e4, b4, g5, b4, e5].map(x => x * 0.999);

// Now, an FM Synthesis voice where the carrier is a sine wave at the given
// frequency, `fq`, and the modulator is another sine wave at the 3.17 times the given frequency.
//
// The FM amount ratio oscillates between [1, 3] at 0.1Hz.
function voice(fq) {
  return el.cycle(
    el.add(
      fq,
      el.mul(
        el.mul(el.add(2, el.cycle(0.1)), el.mul(fq, 3.17)),
        el.cycle(fq),
      ),
    ),
  );
}

// Here we'll construct our arp pattern and apply an ADSR envelope to the synth voice.
// To start, we have a pulse train (gate signal) running at 5Hz.
let gate = el.train(5);

// Our ADSR envelope, triggered by the gate signal.
let env = el.adsr(0.004, 0.01, 0.2, 0.5, gate);

// Now we construct the left and right channel signals: in each channel we run our synth
// voice over the two sequences of frequency values we constructed above, using the `hold` property
// on `el.seq` to hold its output value right up until the next rising edge of the gate.
let left = el.mul(env, voice(el.seq({seq: s1, hold: true}, gate, 0)));
let right = el.mul(env, voice(el.seq({seq: s2, hold: true}, gate, 0)));

// And then render it!
let stats = core.render(
  el.mul(0.3, left),
  el.mul(0.3, right),
);

console.log(stats);
