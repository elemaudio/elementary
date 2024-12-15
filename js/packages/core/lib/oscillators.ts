import {
  createNode,
  isNode,
  resolve,
  ElemNode,
  NodeRepr_t,
} from "../nodeUtils";

import * as co from "./core";
import * as ma from "./math";
import * as fi from "./filters";

const el = {
  ...co,
  ...ma,
  ...fi,
};

/**
 * Outputs a pulse train alternating between 0 and 1 at the given rate.
 *
 * Expects exactly one child, providing the train rate in Hz.
 *
 * @param {ElemNode} rate - Frequency
 * @returns {NodeRepr_t}
 */
export function train(rate: ElemNode): NodeRepr_t {
  return el.le(el.phasor(rate), 0.5);
}

/**
 * Outputs a periodic sine tone at the given frequency.
 *
 * Expects exactly one child specifying the cycle frequency in Hz.
 *
 * @param {ElemNode} rate - Cycle frequency
 * @returns {NodeRepr_t}
 */
export function cycle(rate: ElemNode): NodeRepr_t {
  return el.sin(el.mul(2.0 * Math.PI, el.phasor(rate)));
}

/**
 * Outputs a naive sawtooth oscillator at the given frequency.
 *
 * Expects exactly one child specifying the saw frequency in Hz.
 *
 * Typically, due to the aliasing of the naive sawtooth at audio rates, this oscillator
 * is used for low frequency modulation.
 *
 * @param {ElemNode} rate - Saw frequency
 * @returns {NodeRepr_t}
 */
export function saw(rate: ElemNode): NodeRepr_t {
  return el.sub(el.mul(2, el.phasor(rate)), 1);
}

/**
 * Outputs a naive square oscillator at the given frequency.
 *
 * Expects exactly one child specifying the square frequency in Hz.
 *
 * Typically, due to the aliasing of the naive square at audio rates, this oscillator
 * is used for low frequency modulation.
 *
 * @param {ElemNode} rate - Square frequency
 * @returns {NodeRepr_t}
 */
export function square(rate: ElemNode): NodeRepr_t {
  return el.sub(el.mul(2, train(rate)), 1);
}

/**
 * Outputs a naive triangle oscillator at the given frequency.
 *
 * Expects exactly one child specifying the triangle frequency in Hz.
 *
 * Typically, due to the aliasing of the naive triangle at audio rates, this oscillator
 * is used for low frequency modulation.
 *
 * @param {ElemNode} rate - Triangle frequency
 * @returns {NodeRepr_t}
 */
export function triangle(rate: ElemNode): NodeRepr_t {
  return el.mul(2, el.sub(0.5, el.abs(saw(rate))));
}

/**
 * Outputs a band-limited polyblep sawtooth waveform at the given frequency.
 *
 * Expects exactly one child specifying the saw frequency in Hz.
 *
 * @param {ElemNode} rate - Saw frequency
 * @returns {NodeRepr_t}
 */
export function blepsaw(rate: ElemNode): NodeRepr_t {
  return createNode("blepsaw", {}, [resolve(rate)]);
}

/**
 * Outputs a band-limited polyblep square waveform at the given frequency.
 *
 * Expects exactly one child specifying the square frequency in Hz.
 *
 * @param {ElemNode} rate - Square frequency
 * @returns {NodeRepr_t}
 */
export function blepsquare(rate: ElemNode): NodeRepr_t {
  return createNode("blepsquare", {}, [resolve(rate)]);
}

/**
 * Outputs a band-limited polyblep triangle waveform at the given frequency.
 *
 * Due to the integrator in the signal path, the polyblep triangle oscillator
 * may perform poorly (in terms of anti-aliasing) when the oscillator frequency
 * changes over time.
 *
 * Further, integrating a square waveform as we do here will produce a triangle
 * waveform with a DC offset. Therefore we use a leaky integrator (coefficient at 0.999)
 * which filters out the DC component over time. Note that before the DC component
 * is filtered out, the triangle waveform may exceed +/- 1.0, so take appropriate
 * care to apply gains where necessary.
 *
 * Expects exactly one child specifying the triangle frequency in Hz.
 *
 * @param {ElemNode} rate - Triangle frequency
 * @returns {NodeRepr_t}
 */
export function bleptriangle(rate: ElemNode): NodeRepr_t {
  return createNode("bleptriangle", {}, [resolve(rate)]);
}

/**
 * A simple white noise generator.
 *
 * Generates values uniformly distributed on the range [-1, 1]
 *
 * The seed property may be used to seed the underying random number generator.
 *
 * @param {Object} props
 * @returns {NodeRepr_t}
 */
export function noise(props?: { key?: string; seed?: number }): NodeRepr_t {
  return el.sub(el.mul(2, el.rand(props)), 1);
}

/**
 * A simple pink noise generator.
 *
 * Generates noise with a -3dB/octave rolloff in the frequency response.
 *
 * The seed property may be used to seed the underying random number generator.
 *
 * @param {Object} [props]
 * @returns {core.Node}
 */
export function pinknoise(props?: { key?: string; seed?: number }): NodeRepr_t {
  return el.pink(noise(props));
}
