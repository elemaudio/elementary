import {
  createNode,
  isNode,
  resolve,
} from '../nodeUtils';

import type {ElemNode, NodeRepr_t} from '../nodeUtils';

import * as co from './core';
import * as ma from './math';


// Generic
type OptionalKeyProps = {
  key?: string,
}

const el = {
  ...co,
  ...ma,
};

/**
 * Equivalent to (x / 1000) * sampleRate, where x is the input time in milliseconds.
 */
export function ms2samps(t: ElemNode): NodeRepr_t {
  return el.mul(el.sr(), el.div(t, 1000.0));
}

/**
 * Computes a real pole position giving exponential decay over t, where t is the time (in seconds) to decay 60dB.
 */
export function tau2pole(t: ElemNode): NodeRepr_t {
  return el.exp(el.div(-1.0, el.mul(t, el.sr())));
}

/**
 * Maps a value in Decibels to its corresponding value in amplitude.
 */
export function db2gain(db: ElemNode): NodeRepr_t {
  return el.pow(10, el.mul(db, 1 / 20));
}

/**
 * Maps a value in amplitude to its corresponding value in Decibels.
 *
 * Implicitly inserts a gain floor at -120dB (i.e. if you give a gain value
 * smaller than -120dB this will just return -120dB).
 */
export function gain2db(gain: ElemNode): NodeRepr_t {
  return select(
    el.ge(gain, 0),
    el.max(-120, el.mul(20, el.log(gain))),
    -120,
  );
}

/**
 * A simple conditional operator.
 *
 * Given a gate signal, g, on the range [0, 1], returns the signal a when the gate is high,
 * and the signal b when the gate is low. For values of g between (0, 1), performs
 * a linear interpolation between a and b.
 */
export function select(g: ElemNode, a: ElemNode, b: ElemNode): NodeRepr_t {
  return el.add(
    el.mul(g, a),
    el.mul(el.sub(1, g), b),
  );
}

/**
 * A simple Hann window generator.
 *
 * The window is generated according to an incoming phasor signal, where a phase
 * value of 0 corresponds to the left hand side of the window, and a phase value
 * of 1 corresponds to the right hand side of the window.
 *
 * Expects exactly one child, the incoming phase.
 *
 * @param {Object} [props]
 * @param {core.Node|number} t - Phase signal
 * @returns {core.Node}
 */
export function hann(t: ElemNode): NodeRepr_t {
  return el.mul(0.5, el.sub(1, el.cos(el.mul(2.0 * Math.PI, t))));
}
