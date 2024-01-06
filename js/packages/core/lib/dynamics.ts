import {
  createNode,
  isNode,
} from '../nodeUtils';

import type { ElemNode, NodeRepr_t } from '../nodeUtils';

import * as co from './core';
import * as ma from './math';
import * as si from './signals';


type OptionalKeyProps = {
  key?: string,
}

// NEED Signals library stuffs for tau2pole and db2gain yayay
const el = {
  ...co,
  ...ma,
  ...si,
};

/* A simple compressor with parameterized attack and release times,
 * threshold, compression ratio and (soft) knee width.
 *
 * Users may drive the compressor with an optional sidechain signal, or send the
 * same input both as the input to be compressed and as the sidechain signal itself
 * for standard compressor behavior.
 *
 * @param {Node | number} atkMs – attack time in milliseconds
 * @param {Node | number} relMs – release time in millseconds
 * @param {Node | number} threshold – decibel value above which the comp kicks in
 * @param {Node | number} ratio – ratio by which we squash signal above the threshold
 * @param {Node | number} kneeWidth – width of the knee in decibels, 0 for hard knee
 * @param {Node} sidechain – sidechain signal to drive the compressor
 * @param {Node} xn – input signal to filter
 */
export function compress(
  attackMs: ElemNode,
  releaseMs: ElemNode,
  threshold: ElemNode,
  ratio: ElemNode,
  kneeWidth: ElemNode,
  sidechain: ElemNode,
  xn: ElemNode,
): NodeRepr_t;

export function compress(
  props: OptionalKeyProps,
  attackMs: ElemNode,
  releaseMs: ElemNode,
  threshold: ElemNode,
  ratio: ElemNode,
  kneeWidth: ElemNode,
  sidechain: ElemNode,
  xn: ElemNode,
): NodeRepr_t;

export function compress(a_, b_, c_, d_, e_, f_, g_, h_?) {
  let children = (typeof a_ === "number" || isNode(a_))
    ? [a_, b_, c_, d_, e_, f_, h_]
    : [b_, c_, d_, e_, f_, g_, h_];

  const [atkMs, relMs, threshold, ratio, kneeWidth, sidechain, xn] = children;
  const env = el.env(
    el.tau2pole(el.mul(0.001, atkMs)),
    el.tau2pole(el.mul(0.001, relMs)),
    sidechain,
  );

  const envDecibels = el.gain2db(env);

  // Calculate the soft knee bounds around the threshold
  const lowerKneeBound = el.sub(threshold, el.div(kneeWidth, 2)); // threshold - kneeWidth/2
  const upperKneeBound = el.add(threshold, el.div(kneeWidth, 2)); // threshold + kneeWidth/2

  // Check if the envelope is in the soft knee range
  const isInSoftKneeRange = el.and(
    el.geq(envDecibels, lowerKneeBound), // envDecibels >= lowerKneeBound
    el.leq(envDecibels, upperKneeBound), // envDecibels <= upperKneeBound
  );

  // Calculate gain multiplier for the ratio (1 - 1/ratio)
  const adjustedRatio = el.sub(1, el.div(1, ratio));

  /* Gain calculation
  * When in soft knee range, do : 
  * 0.5 * adjustedRatio * ((envDecibels - lowerKneeBound) / kneeWidth) * (lowerKneeBound - envDecibels)
  * Else do :
  * adjustedRatio * (threshold - envDecibels)
  */
  const gain = el.select(
    isInSoftKneeRange,
    el.mul(
      el.mul(adjustedRatio, 0.5),
      el.mul(
        el.div(el.sub(envDecibels, lowerKneeBound), kneeWidth),
        el.sub(lowerKneeBound, envDecibels)
      )
    ),
    el.mul(
      adjustedRatio,
      el.sub(threshold, envDecibels)
    )
  );

  // Ensuring gain is not positive
  const cleanGain = el.min(0, gain);

  // Convert the gain reduction in dB to a gain factor
  const compressedGain = el.db2gain(cleanGain);

  return el.mul(xn, compressedGain);
}
