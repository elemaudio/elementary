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

/* A simple hard-knee compressor with parameterized attack and release times,
 * threshold, and compression ratio.
 *
 * Users may drive the compressor with an optional sidechain signal, or send the
 * same input both as the input to be compressed and as the sidechain signal itself
 * for standard compressor behavior.
 *
 * @param {Node | number} atkMs – attack time in milliseconds
 * @param {Node | number} relMs – release time in millseconds
 * @param {Node | number} threshold – decibel value above which the comp kicks in
 * @param {Node | number} ratio – ratio by which we squash signal above the threshold
 * @param {Node} sidechain – sidechain signal to drive the compressor
 * @param {Node} xn – input signal to filter
 */
export function compress(
  attackMs: ElemNode,
  releaseMs: ElemNode,
  threshold: ElemNode,
  ratio: ElemNode,
  sidechain: ElemNode,
  xn: ElemNode,
): NodeRepr_t;

export function compress(
  props: OptionalKeyProps,
  attackMs: ElemNode,
  releaseMs: ElemNode,
  threshold: ElemNode,
  ratio: ElemNode,
  sidechain: ElemNode,
  xn: ElemNode,
): NodeRepr_t;

export function compress(a_, b_, c_, d_, e_, f_, g_?) {
  let children = (typeof a_ === "number" || isNode(a_))
    ? [a_, b_, c_, d_, e_, f_]
    : [b_, c_, d_, e_, f_, g_];

  const [atkMs, relMs, threshold, ratio, sidechain, xn] = children;
  const env = el.env(
    el.tau2pole(el.mul(0.001, atkMs)),
    el.tau2pole(el.mul(0.001, relMs)),
    sidechain,
  );

  const envDecibels = el.gain2db(env);

  // Calculate the gain reduction for dBs over the threshold
  // This is the core part of the compressor's ratio logic
  const gainReduction = el.select(
    el.leq(envDecibels, threshold),
    0, // No reduction if below threshold
    el.mul(
      el.sub(envDecibels, threshold),
      el.sub(1, el.div(1, ratio)) // Reduction amount per dB over threshold
    )
  );

  // Convert the gain reduction in dB to a gain factor
  const compressedGain = el.db2gain(
    el.mul(-1, gainReduction)
  );

  return el.mul(xn, compressedGain);
}
