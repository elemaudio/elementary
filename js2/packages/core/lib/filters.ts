import {
  createNode,
  isNode,
  resolve,
} from '../nodeUtils';

import type {NodeRepr_t} from '../src/Reconciler.gen';

import * as co from './core';
import * as ma from './math';
import * as si from './signals';


type OptionalKeyProps = {
  key?: string,
}

const el = {
  ...co,
  ...ma,
  ...si,
};

/**
 * Unity gain one-pole smoothing. Commonly used for addressing discontinuities in control signals.
 *
 * Expects two children, the first is the pole position, the second is the signal to filter.
 *
 * @param {Object} [props]
 * @param {core.Node|number} p - Pole position in [0, 1)
 * @param {core.Node|number} x - Signal to be smoothed
 * @returns {core.Node}
 */
export function smooth(p: NodeRepr_t | number, x: NodeRepr_t | number): NodeRepr_t;
export function smooth(props: OptionalKeyProps, p: NodeRepr_t | number, x: NodeRepr_t | number): NodeRepr_t;
export function smooth(a, b, c?) {
  if (typeof a === "number" || isNode(a)) {
    // The `pole` itself is just a leaky integrator, no gain correction, hence the multiply here.
    return el.pole(a, el.mul(el.sub(1, a), b));
  }

  return el.pole(a, b, el.mul(el.sub(1, b), c));
}

/**
 * A pre-defined smoothing function with a 20ms decay time.
 *
 * Equivalent to `el.smooth(el.tau2pole(0.02), x)`.
 *
 * @param {Object} [props]
 * @param {core.Node|number} x - Signal to be smoothed
 * @returns {core.Node}
 */
export function sm(x: NodeRepr_t | number): NodeRepr_t;
export function sm(props: OptionalKeyProps, x: NodeRepr_t | number): NodeRepr_t;
export function sm(a, b?) {
  if (typeof a === "number" || isNode(a)) {
    return smooth(el.tau2pole(0.02), a);
  }

  return smooth(a, el.tau2pole(0.02), b);
}

/**
 * Implements a simple one-zero filter.
 *
 * Expects the b0 coefficient as the first argument, the zero position b1 as
 * the second argument, and the input to the filter as the third.
 *
 * Difference equation: `y[n] = b0 * x[n] + b1 * x[n - 1]`
 *
 * Reference: https://ccrma.stanford.edu/~jos/filters/One_Zero.html
 *
 * @param {Object} [props]
 * @param {core.Node|number} b0
 * @param {core.Node|number} b1
 * @param {core.Node|number} x
 * @returns {core.Node}
 */
export function zero(b0: NodeRepr_t | number, b1: NodeRepr_t | number, x: NodeRepr_t | number): NodeRepr_t;
export function zero(props: OptionalKeyProps, b0: NodeRepr_t | number, b1: NodeRepr_t | number, x: NodeRepr_t | number): NodeRepr_t;
export function zero(a, b, c, d?) {
  let [b0, b1, x] = (typeof a === "number" || isNode(a))
    ? [a, b, c]
    : [b, c, d];

  return el.sub(el.mul(b0, x), el.mul(b1, el.z(x)));
}

/**
 * Implements a default DC blocking filter with a pole at 0.995 and a zero at 1.
 *
 * This filter has a -3dB point near 35Hz at 44.1kHz.
 *
 * @param {Object} [props]
 * @param {core.Node|number} x
 * @returns {core.Node}
 */
export function dcblock(x: NodeRepr_t | number): NodeRepr_t;
export function dcblock(props: OptionalKeyProps, x: NodeRepr_t | number): NodeRepr_t;
export function dcblock(a, b?) {
  let x = (typeof a === "number" || isNode(a)) ? a : b;
  return el.pole(0.995, zero(1, 1, x));
}

/**
 * A simple first order pole-zero filter, Direct Form 1.
 *
 * @param {Object} [props]
 * @param {core.Node|number} b0
 * @param {core.Node|number} b1
 * @param {core.Node|number} a1
 * @param {core.Node|number} x - Signal to filter
 * @returns {core.Node}
 */
export function df11(b0: NodeRepr_t | number, b1: NodeRepr_t | number, a1: NodeRepr_t | number, x: NodeRepr_t | number): NodeRepr_t;
export function df11(props: OptionalKeyProps, b0: NodeRepr_t | number, b1: NodeRepr_t | number, a1: NodeRepr_t | number, x: NodeRepr_t | number): NodeRepr_t;
export function df11(a, b, c, d, e?) {
  let [b0, b1, a1, x] = (typeof a === "number" || isNode(a))
    ? [a, b, c, d]
    : [b, c, d, e];

  return el.pole(a1, zero(b0, b1, x));
}

// Helper for the below el.lowpass
function LowpassComposite({context, children}) {
  const [fc, q, x] = children;
  const w0 = el.div(el.mul(2 * Math.PI, fc), context.sampleRate);
  const cosw0 = el.cos(w0);
  const alpha = el.div(el.sin(w0), el.mul(2, q));

  const b0 = el.mul(0.5, el.sub(1, cosw0));
  const b1 = el.sub(1, cosw0);
  const b2 = el.mul(0.5, el.sub(1, cosw0));
  const a0 = el.add(1, alpha);
  const a1 = el.mul(-2, cosw0);
  const a2 = el.sub(1, alpha);

  const a0inv = el.div(1, a0);

  // TODO: After we implement the task for eagerly computing el.mul/div/add/sub
  // over primitive number types, we'll arrive at the follow expression being
  // a series of primitive numbers to el.biquad if the provided fc, q are also primitive
  // numbers. In that case, we want `resolveNode` from the reconciler to optionally take
  // some props to stick onto that resulting `const` node. That way, a user can pass a key
  // prop to the lowpass node along with two numbers for fc and q, and we can do all of the
  // mapping onto biquad coeffs right here even with changing fc/q as long as the key holds.
  return el.biquad(
    el.mul(b0, a0inv),
    el.mul(b1, a0inv),
    el.mul(b2, a0inv),
    el.mul(a1, a0inv),
    el.mul(a2, a0inv),
    x
  );
}

/**
 * A second order lowpass filter, derived from the classic RBJ Audio EQ Cookbook.
 *
 * The underlying filter is the transposed Direct Form II `el.biquad`.
 * Coefficients are computed ahead of the realtime thread when possible,
 *
 * Reference: https://www.musicdsp.org/en/latest/Filters/197-rbj-audio-eq-cookbook.html
 *
 * @param {Object} [props]
 * @param {core.Node|number} fc - Cutoff frequency
 * @param {core.Node|number} q - Resonance
 * @param {core.Node|number} x - Signal to filter
 * @returns {core.Node}
 */
export function lowpass(fc: NodeRepr_t | number, q: NodeRepr_t | number, x: NodeRepr_t | number): NodeRepr_t;
export function lowpass(props: OptionalKeyProps, fc: NodeRepr_t | number, q: NodeRepr_t | number, x: NodeRepr_t | number): NodeRepr_t;
export function lowpass(a, b, c, d?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode(LowpassComposite, {}, [a, b, c]);
  }

  return createNode(LowpassComposite, a, [b, c, d]);
}

// Helper for the below el.highpass
function HighpassComposite({context, children}) {
  const [fc, q, x] = children;
  const w0 = el.div(el.mul(2 * Math.PI, fc), context.sampleRate);
  const cosw0 = el.cos(w0);
  const alpha = el.div(el.sin(w0), el.mul(2, q));

  const b0 = el.mul(0.5, el.add(1, cosw0));
  const b1 = el.mul(-1.0, el.add(1, cosw0));
  const b2 = el.mul(0.5, el.add(1, cosw0));
  const a0 = el.add(1, alpha);
  const a1 = el.mul(-2, cosw0);
  const a2 = el.sub(1, alpha);

  const a0inv = el.div(1, a0);

  return el.biquad(
    el.mul(b0, a0inv),
    el.mul(b1, a0inv),
    el.mul(b2, a0inv),
    el.mul(a1, a0inv),
    el.mul(a2, a0inv),
    x
  );
}

/**
 * A second order highpass filter, derived from the classic RBJ Audio EQ Cookbook.
 *
 * The underlying filter is the transposed Direct Form II `el.biquad`.
 * Coefficients are computed ahead of the realtime thread when possible,
 *
 * Reference: https://www.musicdsp.org/en/latest/Filters/197-rbj-audio-eq-cookbook.html
 *
 * @param {Object} [props]
 * @param {core.Node|number} fc - Cutoff frequency
 * @param {core.Node|number} q - Resonance
 * @param {core.Node|number} x - Signal to filter
 * @returns {core.Node}
 */
export function highpass(fc: NodeRepr_t | number, q: NodeRepr_t | number, x: NodeRepr_t | number): NodeRepr_t;
export function highpass(props: OptionalKeyProps, fc: NodeRepr_t | number, q: NodeRepr_t | number, x: NodeRepr_t | number): NodeRepr_t;
export function highpass(a, b, c, d?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode(HighpassComposite, {}, [a, b, c]);
  }

  return createNode(HighpassComposite, a, [b, c, d]);
}

// Helper for the below el.bandpass
function BandpassComposite({context, children}) {
  const [fc, q, x] = children;
  const w0 = el.div(el.mul(2 * Math.PI, fc), context.sampleRate);
  const cosw0 = el.cos(w0);
  const alpha = el.div(el.sin(w0), el.mul(2, q));

  const b0 = alpha;
  const b1 = 0;
  const b2 = el.mul(-1, alpha);
  const a0 = el.add(1, alpha);
  const a1 = el.mul(-2, cosw0);
  const a2 = el.sub(1, alpha);

  const a0inv = el.div(1, a0);

  return el.biquad(
    el.mul(b0, a0inv),
    el.mul(b1, a0inv),
    el.mul(b2, a0inv),
    el.mul(a1, a0inv),
    el.mul(a2, a0inv),
    x
  );
}

/**
 * A second order bandpass filter, derived from the classic RBJ Audio EQ Cookbook.
 *
 * The underlying filter is the transposed Direct Form II `el.biquad`.
 * Coefficients are computed ahead of the realtime thread when possible,
 *
 * Reference: https://www.musicdsp.org/en/latest/Filters/197-rbj-audio-eq-cookbook.html
 *
 * @param {Object} [props]
 * @param {core.Node|number} fc - Cutoff frequency
 * @param {core.Node|number} q - Resonance
 * @param {core.Node|number} x - Signal to filter
 * @returns {core.Node}
 */
export function bandpass(fc: NodeRepr_t | number, q: NodeRepr_t | number, x: NodeRepr_t | number): NodeRepr_t;
export function bandpass(props: OptionalKeyProps, fc: NodeRepr_t | number, q: NodeRepr_t | number, x: NodeRepr_t | number): NodeRepr_t;
export function bandpass(a, b, c, d?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode(BandpassComposite, {}, [a, b, c]);
  }

  return createNode(BandpassComposite, a, [b, c, d]);
}

// Helper for the below el.notch
function NotchComposite({context, children}) {
  const [fc, q, x] = children;
  const w0 = el.div(el.mul(2 * Math.PI, fc), context.sampleRate);
  const cosw0 = el.cos(w0);
  const alpha = el.div(el.sin(w0), el.mul(2, q));

  const b0 = 1;
  const b1 = el.mul(-2, cosw0);
  const b2 = 1;
  const a0 = el.add(1, alpha);
  const a1 = el.mul(-2, cosw0);
  const a2 = el.sub(1, alpha);

  const a0inv = el.div(1, a0);

  return el.biquad(
    el.mul(b0, a0inv),
    el.mul(b1, a0inv),
    el.mul(b2, a0inv),
    el.mul(a1, a0inv),
    el.mul(a2, a0inv),
    x
  );
}

/**
 * A second order notch filter, derived from the classic RBJ Audio EQ Cookbook.
 *
 * The underlying filter is the transposed Direct Form II `el.biquad`.
 * Coefficients are computed ahead of the realtime thread when possible,
 *
 * Reference: https://www.musicdsp.org/en/latest/Filters/197-rbj-audio-eq-cookbook.html
 *
 * @param {Object} [props]
 * @param {core.Node|number} fc - Cutoff frequency
 * @param {core.Node|number} q - Resonance
 * @param {core.Node|number} x - Signal to filter
 * @returns {core.Node}
 */
export function notch(fc: NodeRepr_t | number, q: NodeRepr_t | number, x: NodeRepr_t | number): NodeRepr_t;
export function notch(props: OptionalKeyProps, fc: NodeRepr_t | number, q: NodeRepr_t | number, x: NodeRepr_t | number): NodeRepr_t;
export function notch(a, b, c, d?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode(NotchComposite, {}, [a, b, c]);
  }

  return createNode(NotchComposite, a, [b, c, d]);
}

// Helper for the below el.allpass
function AllpassComposite({context, children}) {
  const [fc, q, x] = children;
  const w0 = el.div(el.mul(2 * Math.PI, fc), context.sampleRate);
  const cosw0 = el.cos(w0);
  const alpha = el.div(el.sin(w0), el.mul(2, q));

  const b0 = el.sub(1, alpha);
  const b1 = el.mul(-2, cosw0);
  const b2 = el.add(1, alpha);
  const a0 = el.add(1, alpha);
  const a1 = el.mul(-2, cosw0);
  const a2 = el.sub(1, alpha);

  const a0inv = el.div(1, a0);

  return el.biquad(
    el.mul(b0, a0inv),
    el.mul(b1, a0inv),
    el.mul(b2, a0inv),
    el.mul(a1, a0inv),
    el.mul(a2, a0inv),
    x
  );
}

/**
 * A second order allpass filter, derived from the classic RBJ Audio EQ Cookbook.
 *
 * The underlying filter is the transposed Direct Form II `el.biquad`.
 * Coefficients are computed ahead of the realtime thread when possible,
 *
 * Reference: https://www.musicdsp.org/en/latest/Filters/197-rbj-audio-eq-cookbook.html
 *
 * @param {Object} [props]
 * @param {core.Node|number} fc - Cutoff frequency
 * @param {core.Node|number} q - Resonance
 * @param {core.Node|number} x - Signal to filter
 * @returns {core.Node}
 */
export function allpass(fc: NodeRepr_t | number, q: NodeRepr_t | number, x: NodeRepr_t | number): NodeRepr_t;
export function allpass(props: OptionalKeyProps, fc: NodeRepr_t | number, q: NodeRepr_t | number, x: NodeRepr_t | number): NodeRepr_t;
export function allpass(a, b, c, d?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode(AllpassComposite, {}, [a, b, c]);
  }

  return createNode(AllpassComposite, a, [b, c, d]);
}

// Helper for the below el.peak
function PeakComposite({context, children}) {
  const [fc, q, gainDecibels, x] = children;
  const A = el.pow(10, el.div(gainDecibels, 40));
  const w0 = el.div(el.mul(2 * Math.PI, fc), context.sampleRate);
  const cosw0 = el.cos(w0);
  const alpha = el.div(el.sin(w0), el.mul(2, q));

  const b0 = el.add(1, el.mul(alpha, A));
  const b1 = el.mul(-2, cosw0);
  const b2 = el.sub(1, el.mul(alpha, A));
  const a0 = el.add(1, el.div(alpha, A));
  const a1 = el.mul(-2, cosw0);
  const a2 = el.sub(1, el.div(alpha, A));

  const a0inv = el.div(1, a0);

  return el.biquad(
    el.mul(b0, a0inv),
    el.mul(b1, a0inv),
    el.mul(b2, a0inv),
    el.mul(a1, a0inv),
    el.mul(a2, a0inv),
    x
  );
}

/**
 * A second order peaking (bell) filter, derived from the classic RBJ Audio EQ Cookbook.
 *
 * The underlying filter is the transposed Direct Form II `el.biquad`.
 * Coefficients are computed ahead of the realtime thread when possible,
 *
 * Reference: https://www.musicdsp.org/en/latest/Filters/197-rbj-audio-eq-cookbook.html
 *
 * @param {Object} [props]
 * @param {core.Node|number} fc - Cutoff frequency
 * @param {core.Node|number} q - Resonance
 * @param {core.Node|number} gainDecibels - Peak gain in decibels
 * @param {core.Node|number} x - Signal to filter
 * @returns {core.Node}
 */
export function peak(fc: NodeRepr_t | number, q: NodeRepr_t | number, gainDecibels: NodeRepr_t | number, x: NodeRepr_t | number): NodeRepr_t;
export function peak(props: OptionalKeyProps, fc: NodeRepr_t | number, q: NodeRepr_t | number, gainDecibels: NodeRepr_t | number, x: NodeRepr_t | number): NodeRepr_t;
export function peak(a, b, c, d, e?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode(PeakComposite, {}, [a, b, c, d]);
  }

  return createNode(PeakComposite, a, [b, c, d, e]);
}

// Helper for the below el.lowshelf
function LowshelfComposite({context, children}) {
  const [fc, q, gainDecibels, x] = children;
  const A = el.pow(10, el.div(gainDecibels, 40));
  const w0 = el.div(el.mul(2 * Math.PI, fc), context.sampleRate);
  const cosw0 = el.cos(w0);
  const alpha = el.div(el.sin(w0), el.mul(2, q));
  const alphaRootA2 = el.mul(2, alpha, el.sqrt(A));

  const ap1 = el.add(A, 1);
  const as1 = el.sub(A, 1);
  const ap1cosw0 = el.mul(ap1, cosw0);
  const as1cosw0 = el.mul(as1, cosw0);

  const b0 = el.mul(A, el.add(alphaRootA2, el.sub(ap1, as1cosw0)));
  const b1 = el.mul(2, A, el.sub(as1, ap1cosw0));
  const b2 = el.mul(A, el.sub(ap1, as1cosw0, alphaRootA2));
  const a0 = el.add(ap1, as1cosw0, alphaRootA2);
  const a1 = el.mul(-2, el.add(as1, ap1cosw0));
  const a2 = el.sub(el.add(ap1, as1cosw0), alphaRootA2);

  const a0inv = el.div(1, a0);

  return el.biquad(
    el.mul(b0, a0inv),
    el.mul(b1, a0inv),
    el.mul(b2, a0inv),
    el.mul(a1, a0inv),
    el.mul(a2, a0inv),
    x
  );
}

/**
 * A second order lowshelf filter, derived from the classic RBJ Audio EQ Cookbook.
 *
 * The underlying filter is the transposed Direct Form II `el.biquad`.
 * Coefficients are computed ahead of the realtime thread when possible,
 *
 * Reference: https://www.musicdsp.org/en/latest/Filters/197-rbj-audio-eq-cookbook.html
 *
 * @param {Object} [props]
 * @param {core.Node|number} fc - Cutoff frequency
 * @param {core.Node|number} q - Resonance
 * @param {core.Node|number} gainDecibels - Shelf gain in decibels
 * @param {core.Node|number} x - Signal to filter
 * @returns {core.Node}
 */
export function lowshelf(fc: NodeRepr_t | number, q: NodeRepr_t | number, gainDecibels: NodeRepr_t | number, x: NodeRepr_t | number): NodeRepr_t;
export function lowshelf(props: OptionalKeyProps, fc: NodeRepr_t | number, q: NodeRepr_t | number, gainDecibels: NodeRepr_t | number, x: NodeRepr_t | number): NodeRepr_t;
export function lowshelf(a, b, c, d, e?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode(LowshelfComposite, {}, [a, b, c, d]);
  }

  return createNode(LowshelfComposite, a, [b, c, d, e]);
}

// Helper for the below el.highshelf
function HighshelfComposite({context, children}) {
  const [fc, q, gainDecibels, x] = children;
  const A = el.pow(10, el.div(gainDecibels, 40));
  const w0 = el.div(el.mul(2 * Math.PI, fc), context.sampleRate);
  const cosw0 = el.cos(w0);
  const alpha = el.div(el.sin(w0), el.mul(2, q));
  const alphaRootA2 = el.mul(2, alpha, el.sqrt(A));

  const ap1 = el.add(A, 1);
  const as1 = el.sub(A, 1);
  const ap1cosw0 = el.mul(ap1, cosw0);
  const as1cosw0 = el.mul(as1, cosw0);

  const b0 = el.mul(A, el.add(alphaRootA2, ap1, as1cosw0));
  const b1 = el.mul(-2, A, el.add(as1, ap1cosw0));
  const b2 = el.mul(A, el.sub(el.add(ap1, as1cosw0), alphaRootA2));
  const a0 = el.add(el.sub(ap1, as1cosw0), alphaRootA2);
  const a1 = el.mul(2, el.sub(as1, ap1cosw0));
  const a2 = el.sub(ap1, as1cosw0, alphaRootA2);

  const a0inv = el.div(1, a0);

  return el.biquad(
    el.mul(b0, a0inv),
    el.mul(b1, a0inv),
    el.mul(b2, a0inv),
    el.mul(a1, a0inv),
    el.mul(a2, a0inv),
    x
  );
}

/**
 * A second order highshelf filter, derived from the classic RBJ Audio EQ Cookbook.
 *
 * The underlying filter is the transposed Direct Form II `el.biquad`.
 * Coefficients are computed ahead of the realtime thread when possible,
 *
 * Reference: https://www.musicdsp.org/en/latest/Filters/197-rbj-audio-eq-cookbook.html
 *
 * @param {Object} [props]
 * @param {core.Node|number} fc - Cutoff frequency
 * @param {core.Node|number} q - Resonance
 * @param {core.Node|number} gainDecibels - Shelf gain in decibels
 * @param {core.Node|number} x - Signal to filter
 * @returns {core.Node}
 */
export function highshelf(fc: NodeRepr_t | number, q: NodeRepr_t | number, gainDecibels: NodeRepr_t | number, x: NodeRepr_t | number): NodeRepr_t;
export function highshelf(props: OptionalKeyProps, fc: NodeRepr_t | number, q: NodeRepr_t | number, gainDecibels: NodeRepr_t | number, x: NodeRepr_t | number): NodeRepr_t;
export function highshelf(a, b, c, d, e?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode(HighshelfComposite, {}, [a, b, c, d]);
  }

  return createNode(HighshelfComposite, a, [b, c, d, e]);
}

/**
 * A pinking filter designed for producing pink noise by filtering white noise.
 *
 * This uses the "economy" method of Paul Kellet's approach described here,
 * https://www.firstpr.com.au/dsp/pink-noise/
 *
 * Three one-pole filters summed but weighted by applying gain to
 * the input signal of each filter differently.
 *
 * This produces large gain below Nyquist (unity at Nyquist), so we aim to find the right scale factor
 * to account for that.  We pick -30dB because of the unity at Nyquist and a ~-3dB/octave
 * slope. If I walk back from Nyquist, 10 octaves gets me to ~20Hz, so 10*-3 = -30dB which
 * in theory places ~20Hz at unity gain and at Nyquist we're -30dB. That lands us close enough,
 * so then we clip into [-1, 1] to catch any remaining loud components below ~20Hz.
 *
 * @param {Object} [props]
 * @param {core.Node|number} x - Signal to filter
 * @returns {core.Node}
 */
export function pink(x: NodeRepr_t | number): NodeRepr_t;
export function pink(props: OptionalKeyProps, x: NodeRepr_t | number): NodeRepr_t;
export function pink(a, b?) {
  let x = (typeof a === "number" || isNode(a)) ? a : b;
  let clip = (min, max, x) => el.min(max, el.max(min, x));

  return clip(-1, 1, el.mul(
    el.db2gain(-30),
    el.add(
      el.pole(0.99765, el.mul(x, 0.0990460)),
      el.pole(0.96300, el.mul(x, 0.2965164)),
      el.pole(0.57000, el.mul(x, 1.0526913)),
      el.mul(0.1848, x),
    ),
  ));
}
