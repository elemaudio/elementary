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
import * as si from "./signals";

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
 * @param {ElemNode} a - Attack time in seconds
 * @param {ElemNode} d - Decay time in seconds
 * @param {ElemNode} s - Sustain level between 0, 1
 * @param {ElemNode} r - Release time in seconds
 * @param {ElemNode} g - Gate signal
 * @returns {NodeRepr_t}
 */
export function adsr(
  attackSec: ElemNode,
  decaySec: ElemNode,
  sustain: ElemNode,
  releaseSec: ElemNode,
  gate: ElemNode,
): NodeRepr_t {
  let [a, d, s, r, g] = [attackSec, decaySec, sustain, releaseSec, gate];
  let atkSamps = el.mul(a, el.sr());
  let atkGate = el.le(el.counter(g), atkSamps);

  let target = el.select(g, el.select(atkGate, 1.0, s), 0);
  let t60 = el.select(g, el.select(atkGate, a, d), r);

  // Accelerate the phase time when calculating the pole position to ensure
  // we reach closer to the target value before moving to the next phase.
  let p = el.tau2pole(el.div(t60, 6.91));

  return el.smooth(p, target);
}
