import {
  createNode,
  isNode,
  resolve,
} from '../nodeUtils';

import type {ElemNode, NodeRepr_t} from '../nodeUtils';

import * as co from './core';
import * as ma from './math';
import * as fi from './filters';
import * as si from './signals';


// Generic
type OptionalKeyProps = {
  key?: string,
}

const el = {
  ...co,
  ...ma,
  ...fi,
  ...si,
};

/**
 * An exponential ADSR envelope generator, triggered by the gate signal, g.
 *
 * When the gate is high (1), this generates the ADS phase. When the gate is
 * low (0), the R phase.
 *
 * @param {Object} [props]
 * @param {core.Node|number} a - Attack time in seconds
 * @param {core.Node|number} d - Decay time in seconds
 * @param {core.Node|number} s - Sustain level between 0, 1
 * @param {core.Node|number} r - Release time in seconds
 * @param {core.Node|number} g - Gate signal
 * @returns {core.Node}
 */
export function adsr(
  attackSec: ElemNode,
  decaySec: ElemNode,
  sustain: ElemNode,
  releaseSec: ElemNode,
  gate: ElemNode,
): NodeRepr_t;

export function adsr(
  props: OptionalKeyProps,
  attackSec: ElemNode,
  decaySec: ElemNode,
  sustain: ElemNode,
  releaseSec: ElemNode,
  gate: ElemNode,
): NodeRepr_t;

export function adsr(a_, b_, c_, d_, e_, f_?) {
  let children = (typeof a_ === "number" || isNode(a_))
    ? [a_, b_, c_, d_, e_]
    : [b_, c_, d_, e_, f_];

  let [a, d, s, r, g] = children;
  let atkSamps = el.mul(a, el.sr());
  let atkGate = el.le(el.counter(g), atkSamps);

  let target = el.select(g, el.select(atkGate, 1.0, s), 0);
  let t60 = el.select(g, el.select(atkGate, a, d), r);

  // Accelerate the phase time when calculating the pole position to ensure
  // we reach closer to the target value before moving to the next phase.
  let p = el.tau2pole(el.div(t60, 6.91));

  return el.smooth(p, target);
}
