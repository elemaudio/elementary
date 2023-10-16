import {Renderer, el} from '@elemaudio/core';


// Much like the FM Arp example, this example demonstrates another case of building a simple
// synth arp to recreate the classic Stranger Things synth line.
let core = new Renderer((batch) => {
  __postNativeMessage__(JSON.stringify(batch));
});

let synthVoice = (hz) => el.mul(
  0.25,
  el.add(
    el.blepsaw(el.mul(hz, 1.001)),
    el.blepsquare(el.mul(hz, 0.994)),
    el.blepsquare(el.mul(hz, 0.501)),
    el.blepsaw(el.mul(hz, 0.496)),
  ),
);

let train = el.train(4.8);
let arp = [0, 4, 7, 11, 12, 11, 7, 4].map(x => 261.63 * 0.5 * Math.pow(2, x / 12));

let modulate = (x, rate, amt) => el.add(x, el.mul(amt, el.cycle(rate)));
let env = el.adsr(0.01, 0.5, 0, 0.4, train);
let filt = (x) => el.lowpass(
  el.add(40, el.mul(modulate(1840, 0.05, 1800), env)),
  1,
  x
);

let out = el.mul(0.25, filt(synthVoice(el.seq({seq: arp, hold: true}, train, 0))));
let stats = core.render(out, out);
