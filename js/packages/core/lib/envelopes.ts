import {
  createNode,
  isNode,
  resolve,
} from '../nodeUtils';

import type {NodeRepr_t} from '../src/Reconciler.gen';

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

function AdsrComposite({children}) {
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
  attackSec: NodeRepr_t | number,
  decaySec: NodeRepr_t | number,
  sustain: NodeRepr_t | number,
  releaseSec: NodeRepr_t | number,
  gate: NodeRepr_t | number,
): NodeRepr_t;

export function adsr(
  props: OptionalKeyProps,
  attackSec: NodeRepr_t | number,
  decaySec: NodeRepr_t | number,
  sustain: NodeRepr_t | number,
  releaseSec: NodeRepr_t | number,
  gate: NodeRepr_t | number,
): NodeRepr_t;

export function adsr(a, b, c, d, e, f?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode(AdsrComposite, {}, [a, b, c, d, e]);
  }

  return createNode(AdsrComposite, a, [b, c, d, e, f]);
}
