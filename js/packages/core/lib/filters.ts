import {
  createNode,
  isNode,
  resolve,
} from '../nodeUtils';

import type {ElemNode, NodeRepr_t} from '../nodeUtils';

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
export function smooth(p: ElemNode, x: ElemNode): NodeRepr_t;
export function smooth(props: OptionalKeyProps, p: ElemNode, x: ElemNode): NodeRepr_t;
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
export function sm(x: ElemNode): NodeRepr_t;
export function sm(props: OptionalKeyProps, x: ElemNode): NodeRepr_t;
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
export function zero(b0: ElemNode, b1: ElemNode, x: ElemNode): NodeRepr_t;
export function zero(props: OptionalKeyProps, b0: ElemNode, b1: ElemNode, x: ElemNode): NodeRepr_t;
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
export function dcblock(x: ElemNode): NodeRepr_t;
export function dcblock(props: OptionalKeyProps, x: ElemNode): NodeRepr_t;
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
export function df11(b0: ElemNode, b1: ElemNode, a1: ElemNode, x: ElemNode): NodeRepr_t;
export function df11(props: OptionalKeyProps, b0: ElemNode, b1: ElemNode, a1: ElemNode, x: ElemNode): NodeRepr_t;
export function df11(a, b, c, d, e?) {
  let [b0, b1, a1, x] = (typeof a === "number" || isNode(a))
    ? [a, b, c, d]
    : [b, c, d, e];

  return el.pole(a1, zero(b0, b1, x));
}

/**
 * A second order lowpass filter
 *
 * @param {Object} [props]
 * @param {core.Node|number} fc - Cutoff frequency
 * @param {core.Node|number} q - Resonance
 * @param {core.Node|number} x - Signal to filter
 * @returns {core.Node}
 */
export function lowpass(fc: ElemNode, q: ElemNode, x: ElemNode): NodeRepr_t;
export function lowpass(props: OptionalKeyProps, fc: ElemNode, q: ElemNode, x: ElemNode): NodeRepr_t;
export function lowpass(a, b, c, d?) {
  if (typeof a === "number" || isNode(a)) {
    return el.svf({mode: 'lowpass'}, a, b, c);
  }

  return el.svf(Object.assign({}, a, {mode: 'lowpass'}), b, c, d);
}

/**
 * A second order highpass filter
 *
 * @param {Object} [props]
 * @param {core.Node|number} fc - Cutoff frequency
 * @param {core.Node|number} q - Resonance
 * @param {core.Node|number} x - Signal to filter
 * @returns {core.Node}
 */
export function highpass(fc: ElemNode, q: ElemNode, x: ElemNode): NodeRepr_t;
export function highpass(props: OptionalKeyProps, fc: ElemNode, q: ElemNode, x: ElemNode): NodeRepr_t;
export function highpass(a, b, c, d?) {
  if (typeof a === "number" || isNode(a)) {
    return el.svf({mode: 'highpass'}, a, b, c);
  }

  return el.svf(Object.assign({}, a, {mode: 'highpass'}), b, c, d);
}

/**
 * A second order bandpass filter
 *
 * @param {Object} [props]
 * @param {core.Node|number} fc - Cutoff frequency
 * @param {core.Node|number} q - Resonance
 * @param {core.Node|number} x - Signal to filter
 * @returns {core.Node}
 */
export function bandpass(fc: ElemNode, q: ElemNode, x: ElemNode): NodeRepr_t;
export function bandpass(props: OptionalKeyProps, fc: ElemNode, q: ElemNode, x: ElemNode): NodeRepr_t;
export function bandpass(a, b, c, d?) {
  if (typeof a === "number" || isNode(a)) {
    return el.svf({mode: 'bandpass'}, a, b, c);
  }

  return el.svf(Object.assign({}, a, {mode: 'bandpass'}), b, c, d);
}

/**
 * A second order notch filter
 *
 * @param {Object} [props]
 * @param {core.Node|number} fc - Cutoff frequency
 * @param {core.Node|number} q - Resonance
 * @param {core.Node|number} x - Signal to filter
 * @returns {core.Node}
 */
export function notch(fc: ElemNode, q: ElemNode, x: ElemNode): NodeRepr_t;
export function notch(props: OptionalKeyProps, fc: ElemNode, q: ElemNode, x: ElemNode): NodeRepr_t;
export function notch(a, b, c, d?) {
  if (typeof a === "number" || isNode(a)) {
    return el.svf({mode: 'notch'}, a, b, c);
  }

  return el.svf(Object.assign({}, a, {mode: 'notch'}), b, c, d);
}

/**
 * A second order allpass filter
 *
 * @param {Object} [props]
 * @param {core.Node|number} fc - Cutoff frequency
 * @param {core.Node|number} q - Resonance
 * @param {core.Node|number} x - Signal to filter
 * @returns {core.Node}
 */
export function allpass(fc: ElemNode, q: ElemNode, x: ElemNode): NodeRepr_t;
export function allpass(props: OptionalKeyProps, fc: ElemNode, q: ElemNode, x: ElemNode): NodeRepr_t;
export function allpass(a, b, c, d?) {
  if (typeof a === "number" || isNode(a)) {
    return el.svf({mode: 'allpass'}, a, b, c);
  }

  return el.svf(Object.assign({}, a, {mode: 'allpass'}), b, c, d);
}

/**
 * A second order peak (bell) filter
 *
 * @param {Object} [props]
 * @param {core.Node|number} fc - Cutoff frequency
 * @param {core.Node|number} q - Resonance
 * @param {core.Node|number} gainDecibels - Peak gain in decibels
 * @param {core.Node|number} x - Signal to filter
 * @returns {core.Node}
 */
export function peak(fc: ElemNode, q: ElemNode, gainDecibels: ElemNode, x: ElemNode): NodeRepr_t;
export function peak(props: OptionalKeyProps, fc: ElemNode, q: ElemNode, gainDecibels: ElemNode, x: ElemNode): NodeRepr_t;
export function peak(a, b, c, d, e?) {
  if (typeof a === "number" || isNode(a)) {
    return el.svfshelf({mode: 'peak'}, a, b, c, d);
  }

  return el.svfshelf(Object.assign({}, a, {mode: 'peak'}), b, c, d, e);
}

/**
 * A second order lowshelf filter
 *
 * @param {Object} [props]
 * @param {core.Node|number} fc - Cutoff frequency
 * @param {core.Node|number} q - Resonance
 * @param {core.Node|number} gainDecibels - Shelf gain in decibels
 * @param {core.Node|number} x - Signal to filter
 * @returns {core.Node}
 */
export function lowshelf(fc: ElemNode, q: ElemNode, gainDecibels: ElemNode, x: ElemNode): NodeRepr_t;
export function lowshelf(props: OptionalKeyProps, fc: ElemNode, q: ElemNode, gainDecibels: ElemNode, x: ElemNode): NodeRepr_t;
export function lowshelf(a, b, c, d, e?) {
  if (typeof a === "number" || isNode(a)) {
    return el.svfshelf({mode: 'lowshelf'}, a, b, c, d);
  }

  return el.svfshelf(Object.assign({}, a, {mode: 'lowshelf'}), b, c, d, e);
}

/**
 * A second order highshelf filter
 *
 * @param {Object} [props]
 * @param {core.Node|number} fc - Cutoff frequency
 * @param {core.Node|number} q - Resonance
 * @param {core.Node|number} gainDecibels - Shelf gain in decibels
 * @param {core.Node|number} x - Signal to filter
 * @returns {core.Node}
 */
export function highshelf(fc: ElemNode, q: ElemNode, gainDecibels: ElemNode, x: ElemNode): NodeRepr_t;
export function highshelf(props: OptionalKeyProps, fc: ElemNode, q: ElemNode, gainDecibels: ElemNode, x: ElemNode): NodeRepr_t;
export function highshelf(a, b, c, d, e?) {
  if (typeof a === "number" || isNode(a)) {
    return el.svfshelf({mode: 'highshelf'}, a, b, c, d);
  }

  return el.svfshelf(Object.assign({}, a, {mode: 'highshelf'}), b, c, d, e);
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
export function pink(x: ElemNode): NodeRepr_t;
export function pink(props: OptionalKeyProps, x: ElemNode): NodeRepr_t;
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
