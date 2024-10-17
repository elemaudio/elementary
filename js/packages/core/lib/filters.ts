import {
  createNode,
  isNode,
  resolve,
  ElemNode,
  NodeRepr_t,
} from "../nodeUtils";

import * as co from "./core";
import * as ma from "./math";
import * as si from "./signals";

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
 * @param {ElemNode} p - Pole position in [0, 1)
 * @param {ElemNode} x - Signal to be smoothed
 * @returns {NodeRepr_t}
 */
export function smooth(p: ElemNode, x: ElemNode): NodeRepr_t {
  // The pole node itself is just a leaky integrator, no gain correction, hence the multiply here.
  return el.pole(p, el.mul(el.sub(1, p), x));
}

/**
 * A pre-defined smoothing function with a 20ms decay time.
 *
 * Equivalent to `el.smooth(el.tau2pole(0.02), x)`.
 *
 * @param {ElemNode} x - Signal to be smoothed
 * @returns {NodeRepr_t}
 */
export function sm(x: ElemNode): NodeRepr_t {
  return smooth(el.tau2pole(0.02), x);
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
 * @param {ElemNode} b0
 * @param {ElemNode} b1
 * @param {ElemNode} x
 * @returns {NodeRepr_t}
 */
export function zero(b0: ElemNode, b1: ElemNode, x: ElemNode): NodeRepr_t {
  return el.sub(el.mul(b0, x), el.mul(b1, el.z(x)));
}

/**
 * Implements a default DC blocking filter with a pole at 0.995 and a zero at 1.
 *
 * This filter has a -3dB gain near 35Hz at 44.1kHz.
 *
 * @param {ElemNode} x
 * @returns {NodeRepr_t}
 */
export function dcblock(x: ElemNode): NodeRepr_t {
  return el.pole(0.995, zero(1, 1, x));
}

/**
 * A simple first order pole-zero filter, Direct Form 1.
 *
 * @param {ElemNode} b0
 * @param {ElemNode} b1
 * @param {ElemNode} a1
 * @param {ElemNode} x - Signal to filter
 * @returns {NodeRepr_t}
 */
export function df11(
  b0: ElemNode,
  b1: ElemNode,
  a1: ElemNode,
  x: ElemNode,
): NodeRepr_t {
  return el.pole(a1, zero(b0, b1, x));
}

/**
 * A second order lowpass filter
 *
 * @param {ElemNode} fc - Cutoff frequency
 * @param {ElemNode} q - Resonance
 * @param {ElemNode} x - Signal to filter
 * @returns {NodeRepr_t}
 */
export function lowpass(fc: ElemNode, q: ElemNode, x: ElemNode): NodeRepr_t {
  return el.svf({ mode: "lowpass" }, fc, q, x);
}

/**
 * A second order highpass filter
 *
 * @param {ElemNode} fc - Cutoff frequency
 * @param {ElemNode} q - Resonance
 * @param {ElemNode} x - Signal to filter
 * @returns {NodeRepr_t}
 */
export function highpass(fc: ElemNode, q: ElemNode, x: ElemNode): NodeRepr_t {
  return el.svf({ mode: "highpass" }, fc, q, x);
}

/**
 * A second order bandpass filter
 *
 * @param {ElemNode} fc - Cutoff frequency
 * @param {ElemNode} q - Resonance
 * @param {ElemNode} x - Signal to filter
 * @returns {NodeRepr_t}
 */
export function bandpass(fc: ElemNode, q: ElemNode, x: ElemNode): NodeRepr_t {
  return el.svf({ mode: "bandpass" }, fc, q, x);
}

/**
 * A second order notch filter
 *
 * @param {ElemNode} fc - Cutoff frequency
 * @param {ElemNode} q - Resonance
 * @param {ElemNode} x - Signal to filter
 * @returns {NodeRepr_t}
 */
export function notch(fc: ElemNode, q: ElemNode, x: ElemNode): NodeRepr_t {
  return el.svf({ mode: "notch" }, fc, q, x);
}

/**
 * A second order allpass filter
 *
 * @param {ElemNode} fc - Cutoff frequency
 * @param {ElemNode} q - Resonance
 * @param {ElemNode} x - Signal to filter
 * @returns {NodeRepr_t}
 */
export function allpass(fc: ElemNode, q: ElemNode, x: ElemNode): NodeRepr_t {
  return el.svf({ mode: "allpass" }, fc, q, x);
}

/**
 * A second order peak (bell) filter
 *
 * @param {ElemNode} fc - Cutoff frequency
 * @param {ElemNode} q - Resonance
 * @param {ElemNode} gainDecibels - Peak gain in decibels
 * @param {ElemNode} x - Signal to filter
 * @returns {NodeRepr_t}
 */
export function peak(
  fc: ElemNode,
  q: ElemNode,
  gainDecibels: ElemNode,
  x: ElemNode,
): NodeRepr_t {
  return el.svfshelf({ mode: "peak" }, fc, q, gainDecibels, x);
}

/**
 * A second order lowshelf filter
 *
 * @param {ElemNode} fc - Cutoff frequency
 * @param {ElemNode} q - Resonance
 * @param {ElemNode} gainDecibels - Shelf gain in decibels
 * @param {ElemNode} x - Signal to filter
 * @returns {NodeRepr_t}
 */
export function lowshelf(
  fc: ElemNode,
  q: ElemNode,
  gainDecibels: ElemNode,
  x: ElemNode,
): NodeRepr_t {
  return el.svfshelf({ mode: "lowshelf" }, fc, q, gainDecibels, x);
}

/**
 * A second order highshelf filter
 *
 * @param {ElemNode} fc - Cutoff frequency
 * @param {ElemNode} q - Resonance
 * @param {ElemNode} gainDecibels - Shelf gain in decibels
 * @param {ElemNode} x - Signal to filter
 * @returns {NodeRepr_t}
 */
export function highshelf(
  fc: ElemNode,
  q: ElemNode,
  gainDecibels: ElemNode,
  x: ElemNode,
): NodeRepr_t {
  return el.svfshelf({ mode: "highshelf" }, fc, q, gainDecibels, x);
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
 * @param {ElemNode} x - Signal to filter
 * @returns {NodeRepr_t}
 */
export function pink(x: ElemNode): NodeRepr_t {
  let clip = (min, max, x) => el.min(max, el.max(min, x));

  return clip(
    -1,
    1,
    el.mul(
      el.db2gain(-30),
      el.add(
        el.pole(0.99765, el.mul(x, 0.099046)),
        el.pole(0.963, el.mul(x, 0.2965164)),
        el.pole(0.57, el.mul(x, 1.0526913)),
        el.mul(0.1848, x),
      ),
    ),
  );
}
