import {
  createNode,
  isNode,
  resolve,
} from '../nodeUtils';

import type {ElemNode, NodeRepr_t} from '../nodeUtils';

import * as co from './core';
import * as ma from './math';
import * as fi from './filters';


// Generic
type OptionalKeyProps = {
  key?: string,
};

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
 * @param {Object} [props]
 * @param {core.Node|number} rate - Frequency
 * @returns {core.Node}
 */
export function train(rate: ElemNode): NodeRepr_t;
export function train(props: OptionalKeyProps, rate: ElemNode): NodeRepr_t;
export function train(a, b?) {
  if (typeof a === "number" || isNode(a)) {
    return el.le(el.phasor(a, 0), 0.5);
  }

  return el.le(el.phasor(a, b, 0), 0.5);
}

/**
 * Outputs a periodic sine tone at the given frequency.
 *
 * Expects exactly one child specifying the cycle frequency in Hz.
 *
 * @param {Object} [props]
 * @param {core.Node|number} rate - Cycle frequency
 * @returns {core.Node}
 */
export function cycle(rate: ElemNode): NodeRepr_t;
export function cycle(props: OptionalKeyProps, rate: ElemNode): NodeRepr_t;
export function cycle(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? el.sin(el.mul(2.0 * Math.PI, el.phasor(a, 0)))
    : el.sin(el.mul(2.0 * Math.PI, el.phasor(a, b, 0)));
}

/**
 * Outputs a naive sawtooth oscillator at the given frequency.
 *
 * Expects exactly one child specifying the saw frequency in Hz.
 *
 * Typically, due to the aliasing of the naive sawtooth at audio rates, this oscillator
 * is used for low frequency modulation.
 *
 * @param {Object} [props]
 * @param {core.Node|number} rate - Saw frequency
 * @returns {core.Node}
 */
export function saw(rate: ElemNode): NodeRepr_t;
export function saw(props: OptionalKeyProps, rate: ElemNode): NodeRepr_t;
export function saw(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? el.sub(el.mul(2, el.phasor(a, 0)), 1)
    : el.sub(el.mul(2, el.phasor(a, b, 0)), 1);
}

/**
 * Outputs a naive square oscillator at the given frequency.
 *
 * Expects exactly one child specifying the square frequency in Hz.
 *
 * Typically, due to the aliasing of the naive square at audio rates, this oscillator
 * is used for low frequency modulation.
 *
 * @param {Object} [props]
 * @param {core.Node|number} rate - Square frequency
 * @returns {core.Node}
 */
export function square(rate: ElemNode): NodeRepr_t;
export function square(props: OptionalKeyProps, rate: ElemNode): NodeRepr_t;
export function square(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? el.sub(el.mul(2, train(a)), 1)
    : el.sub(el.mul(2, train(a, b)), 1);
}

/**
 * Outputs a naive triangle oscillator at the given frequency.
 *
 * Expects exactly one child specifying the triangle frequency in Hz.
 *
 * Typically, due to the aliasing of the naive triangle at audio rates, this oscillator
 * is used for low frequency modulation.
 *
 * @param {Object} [props]
 * @param {core.Node|number} rate - Triangle frequency
 * @returns {core.Node}
 */
export function triangle(rate: ElemNode): NodeRepr_t;
export function triangle(props: OptionalKeyProps, rate: ElemNode): NodeRepr_t;
export function triangle(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? el.mul(2, el.sub(0.5, el.abs(saw(a))))
    : el.mul(2, el.sub(0.5, el.abs(saw(a, b))));
}

/**
 * Outputs a polyblep signal to be summed into various naive oscillator types.
 *
 * @param {core.Node|number} step – phase step per sample, given by the frequency of the oscillator
 * @param {core.Node} phase – current phase of the oscillator
 * @returns {core.Node}
 * @ignore
 */
function polyblep(step, phase) {
  let leftgate = el.le(phase, step);
  let rightgate = el.ge(phase, el.sub(1, step));
  let lx = el.div(phase, step);
  let rx = el.div(el.sub(phase, 1), step);

  return el.add(
    el.mul(leftgate, el.sub(el.mul(2, lx), el.mul(lx, lx), 1)),
    el.mul(rightgate, el.add(el.mul(2, rx), el.mul(rx, rx), 1)),
  );
}

/**
 * Outputs a band-limited polyblep sawtooth waveform at the given frequency.
 *
 * Expects exactly one child specifying the saw frequency in Hz.
 *
 * @param {Object} [props]
 * @param {core.Node|number} rate - Saw frequency
 * @returns {core.Node}
 */
export function blepsaw(rate: ElemNode): NodeRepr_t;
export function blepsaw(props: OptionalKeyProps, rate: ElemNode): NodeRepr_t;
export function blepsaw(a, b?) {
  let hasProps = !(typeof a === "number" || isNode(a));
  let rate = hasProps ? b : a;

  return createNode("blepsaw", {}, [rate]);
}

/**
 * Outputs a band-limited polyblep square waveform at the given frequency.
 *
 * Expects exactly one child specifying the square frequency in Hz.
 *
 * @param {Object} [props]
 * @param {core.Node|number} rate - Square frequency
 * @returns {core.Node}
 */
export function blepsquare(rate: ElemNode): NodeRepr_t;
export function blepsquare(props: OptionalKeyProps, rate: ElemNode): NodeRepr_t;
export function blepsquare(a, b?) {
  let hasProps = !(typeof a === "number" || isNode(a));
  let rate = hasProps ? b : a;

  return createNode("blepsquare", {}, [rate]);
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
 * @param {Object} [props]
 * @param {core.Node|number} rate - Triangle frequency
 * @returns {core.Node}
 */
export function bleptriangle(rate: ElemNode): NodeRepr_t;
export function bleptriangle(props: OptionalKeyProps, rate: ElemNode): NodeRepr_t;
export function bleptriangle(a, b?) {
  let hasProps = !(typeof a === "number" || isNode(a));
  let rate = hasProps ? b : a;

  return createNode("bleptriangle", {}, [rate]);
}

// Noise
type NoiseNodeProps = {
  key?: string,
  seed?: number,
};

/**
 * A simple white noise generator.
 *
 * Generates values uniformly distributed on the range [-1, 1]
 *
 * The seed property may be used to seed the underying random number generator.
 *
 * @param {Object} [props]
 * @returns {core.Node}
 */
export function noise(): NodeRepr_t;
export function noise(props: NoiseNodeProps): NodeRepr_t;
export function noise(a?) {
  if (typeof a === "undefined") {
    return el.sub(el.mul(2, el.rand()), 1);
  }

  return el.sub(el.mul(2, el.rand(a)), 1);
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
export function pinknoise(): NodeRepr_t;
export function pinknoise(props: NoiseNodeProps): NodeRepr_t;
export function pinknoise(a?) {
  if (typeof a === "undefined") {
    return el.pink(noise());
  }

  return el.pink(noise(a));
}
