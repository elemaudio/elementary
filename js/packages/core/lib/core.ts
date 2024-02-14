import {
  createNode,
  isNode,
  resolve,
} from '../nodeUtils';

import type {ElemNode, NodeRepr_t} from '../nodeUtils';


// Generic
type OptionalKeyProps = {
  key?: string,
};

// Const node and constant value nodes
type ConstNodeProps = {
  key?: string,
  value: number,
};

export function constant(props: ConstNodeProps): NodeRepr_t {
  return createNode("const", props, []);
}

export function sr(): NodeRepr_t {
  return createNode("sr", {}, []);
}

export function time(): NodeRepr_t {
  return createNode("time", {}, []);
}

// Counter node
export function counter(gate: ElemNode): NodeRepr_t;
export function counter(props: OptionalKeyProps, gate: ElemNode): NodeRepr_t;
export function counter(a, b?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode("counter", {}, [resolve(a)]);
  }

  return createNode("counter", a, [resolve(b)]);
}

// Accum node
export function accum(xn: ElemNode, reset: ElemNode): NodeRepr_t;
export function accum(props: OptionalKeyProps, xn: ElemNode, reset: ElemNode): NodeRepr_t;
export function accum(a, b, c?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode("accum", {}, [resolve(a), resolve(b)]);
  }

  return createNode("accum", a, [resolve(b), resolve(c)]);
}

// Phasor node
export function phasor(rate: ElemNode): NodeRepr_t;
export function phasor(props: OptionalKeyProps, rate: ElemNode): NodeRepr_t;
export function phasor(a, b?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode("phasor", {}, [resolve(a)]);
  }

  return createNode("phasor", a, [resolve(b)]);
}

export function syncphasor(rate: ElemNode, reset: ElemNode): NodeRepr_t;
export function syncphasor(props: OptionalKeyProps, rate: ElemNode, reset: ElemNode): NodeRepr_t;
export function syncphasor(a, b, c?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode("sphasor", {}, [resolve(a), resolve(b)]);
  }

  return createNode("sphasor", a, [resolve(b), resolve(c)]);
}

// Latch node
export function latch(t: ElemNode, x: ElemNode): NodeRepr_t;
export function latch(props: OptionalKeyProps, t: ElemNode, x: ElemNode): NodeRepr_t;
export function latch(a, b, c?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode("latch", {}, [resolve(a), resolve(b)]);
  }

  return createNode("latch", a, [resolve(b), resolve(c)]);
}

// MaxHold node
type MaxHoldNodeProps = {
  key?: string,
  hold?: number,
};

export function maxhold(x: ElemNode, reset: ElemNode): NodeRepr_t;
export function maxhold(props: MaxHoldNodeProps, x: ElemNode, reset: ElemNode): NodeRepr_t;
export function maxhold(a, b, c?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode("maxhold", {}, [resolve(a), resolve(b)]);
  }

  return createNode("maxhold", a, [resolve(b), resolve(c)]);
}

// Once node
type OnceNodeProps = {
  key?: string,
  arm?: boolean,
};

export function once(x: ElemNode): NodeRepr_t;
export function once(props: OnceNodeProps, x: ElemNode): NodeRepr_t;
export function once(a, b?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode("once", {}, [resolve(a)]);
  }

  return createNode("once", a, [resolve(b)]);
}

// Rand node
type RandNodeProps = {
  key?: string,
  seed?: number,
};

export function rand(): NodeRepr_t;
export function rand(props: RandNodeProps): NodeRepr_t;
export function rand(a?) {
  if (typeof a !== "undefined") {
    return createNode("rand", a, []);
  }

  return createNode("rand", {}, []);
}

// Metro node
type MetroNodeProps = {
  key?: string,
  interval?: number,
};

export function metro(): NodeRepr_t;
export function metro(props: MetroNodeProps): NodeRepr_t;
export function metro(a?) {
  if (typeof a !== "undefined") {
    return createNode("metro", a, []);
  }

  return createNode("metro", {}, []);
}

// Sample node
type SampleNodeProps = {
  key?: string,
  path?: string,
  mode?: string,
  startOffset?: number,
  stopOffset?: number,
};

export function sample(props: SampleNodeProps, trigger: ElemNode, rate: ElemNode): NodeRepr_t {
  return createNode("sample", props, [resolve(trigger), resolve(rate)]);
}

// Table node
type TableNodeProps = {
  key?: string,
  path?: string,
};

export function table(props: TableNodeProps, t: ElemNode): NodeRepr_t {
  return createNode("table", props, [resolve(t)]);
}

// Convolve node
type ConvolveNodeProps = {
  key?: string,
  path?: string,
};

export function convolve(props: ConvolveNodeProps, x: ElemNode): NodeRepr_t {
  return createNode("convolve", props, [resolve(x)]);
}

// Seq node
type SeqNodeProps = {
  key?: string,
  seq?: Array<number>,
  offset?: number,
  hold?: boolean,
  loop?: boolean,
};

export function seq(props: SeqNodeProps, trigger: ElemNode, reset: ElemNode): NodeRepr_t {
  return createNode("seq", props, [resolve(trigger), resolve(reset)]);
}

// Seq2 node
type Seq2NodeProps = {
  key?: string,
  seq?: Array<number>,
  offset?: number,
  hold?: boolean,
  loop?: boolean,
};

export function seq2(props: Seq2NodeProps, trigger: ElemNode, reset: ElemNode): NodeRepr_t {
  return createNode("seq2", props, [resolve(trigger), resolve(reset)]);
}

// SparSeq node
type SparSeqNodeProps = {
  key?: string,
  seq?: Array<{value: number, tickTime: number}>,
  offset?: number,
  loop?: boolean | Array<number>,
  resetOnLoop?: boolean,
  interpolate?: number,
  tickInterval?: number,
};

export function sparseq(props: SparSeqNodeProps, trigger: ElemNode, reset: ElemNode): NodeRepr_t {
  return createNode("sparseq", props, [resolve(trigger), resolve(reset)]);
}

// SparSeq2 node
type SparSeq2NodeProps = {
  key?: string,
  seq?: Array<{value: number, time: number}>,
};

export function sparseq2(props: SparSeq2NodeProps, time: ElemNode): NodeRepr_t {
  return createNode("sparseq2", props, [resolve(time)]);
}

// Sampleseq node
type SampleSeqNodeProps = {
  key?: string,
  seq?: Array<{value: number, time: number}>,
  duration: number,
  path: string,
};

export function sampleseq(props: SampleSeqNodeProps, time: ElemNode): NodeRepr_t {
  return createNode("sampleseq", props, [resolve(time)]);
}

// Sampleseq2 node
type SampleSeq2NodeProps = {
  key?: string,
  seq?: Array<{value: number, time: number}>,
  duration: number,
  path: string,
  stretch?: number,
  shift?: number,
};

export function sampleseq2(props: SampleSeq2NodeProps, time: ElemNode): NodeRepr_t {
  return createNode("sampleseq2", props, [resolve(time)]);
}

// Pole node
export function pole(p: ElemNode, x: ElemNode): NodeRepr_t;
export function pole(props: OptionalKeyProps, p: ElemNode, x: ElemNode): NodeRepr_t;
export function pole(a, b, c?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode("pole", {}, [resolve(a), resolve(b)]);
  }

  return createNode("pole", a, [resolve(b), resolve(c)]);
}

// Env node
export function env(atkPole: ElemNode, relPole: ElemNode, x: ElemNode): NodeRepr_t;
export function env(props: OptionalKeyProps, atkPole: ElemNode, relPole: ElemNode, x: ElemNode): NodeRepr_t;
export function env(a, b, c, d?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode("env", {}, [resolve(a), resolve(b), resolve(c)]);
  }

  return createNode("env", a, [resolve(b), resolve(c), resolve(d)]);
}

// Single sample delay node
export function z(x: ElemNode): NodeRepr_t;
export function z(props: OptionalKeyProps, x: ElemNode): NodeRepr_t;
export function z(a, b?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode("z", {}, [resolve(a)]);
  }

  return createNode("z", a, [resolve(b)]);
}

// Variable delay node
type DelayNodeProps = {
  key?: string,
  size: number,
};

export function delay(
  props: DelayNodeProps,
  len: ElemNode,
  fb: ElemNode,
  x : ElemNode,
): NodeRepr_t;

export function delay(a, b, c, d) {
  if (typeof a === "number" || isNode(a)) {
    return createNode("delay", {}, [resolve(a), resolve(b), resolve(c)]);
  }

  return createNode("delay", a, [resolve(b), resolve(c), resolve(d)]);
}

export function sdelay(props: DelayNodeProps, x: ElemNode): NodeRepr_t {
  return createNode("sdelay", props, [resolve(x)]);
}

// Multimode1p
export function prewarp(fc: ElemNode): NodeRepr_t {
  return createNode("prewarp", {}, [fc]);
}

export function mm1p(
  fc: ElemNode,
  x: ElemNode,
): NodeRepr_t;

export function mm1p(
  props: {
    key?: string,
    mode?: string,
  },
  fc: ElemNode,
  x: ElemNode,
): NodeRepr_t;

export function mm1p(a, b, c?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode("mm1p", {}, [
      resolve(a),
      resolve(b),
    ]);
  }

  return createNode("mm1p", a, [
    resolve(b),
    resolve(c),
  ]);
}

// SVF
export function svf(
  fc: ElemNode,
  q: ElemNode,
  x: ElemNode,
): NodeRepr_t;

export function svf(
  props: {
    key?: string,
    mode?: string,
  },
  fc: ElemNode,
  q: ElemNode,
  x: ElemNode,
): NodeRepr_t;

export function svf(a, b, c, d?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode("svf", {}, [
      resolve(a),
      resolve(b),
      resolve(c),
    ]);
  }

  return createNode("svf", a, [
    resolve(b),
    resolve(c),
    resolve(d),
  ]);
}

export function svfshelf(
  fc: ElemNode,
  q: ElemNode,
  gainDecibels: ElemNode,
  x: ElemNode,
): NodeRepr_t;

export function svfshelf(
  props: {
    key?: string,
    mode?: string,
  },
  fc: ElemNode,
  q: ElemNode,
  gainDecibels: ElemNode,
  x: ElemNode,
): NodeRepr_t;

export function svfshelf(a, b, c, d, e?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode("svfshelf", {}, [
      resolve(a),
      resolve(b),
      resolve(c),
      resolve(d),
    ]);
  }

  return createNode("svfshelf", a, [
    resolve(b),
    resolve(c),
    resolve(d),
    resolve(e),
  ]);
}

// Biquad node
export function biquad(
  b0: ElemNode,
  b1: ElemNode,
  b2: ElemNode,
  a1: ElemNode,
  a2: ElemNode,
  x: ElemNode,
): NodeRepr_t;

export function biquad(
  props: OptionalKeyProps,
  b0: ElemNode,
  b1: ElemNode,
  b2: ElemNode,
  a1: ElemNode,
  a2: ElemNode,
  x: ElemNode,
): NodeRepr_t;

export function biquad(a, b, c, d, e, f, g?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode("biquad", {}, [
      resolve(a),
      resolve(b),
      resolve(c),
      resolve(d),
      resolve(e),
      resolve(f),
    ]);
  }

  return createNode("biquad", a, [
    resolve(b),
    resolve(c),
    resolve(d),
    resolve(e),
    resolve(f),
    resolve(g),
  ]);
}

// Feedback nodes
type TapInNodeProps = {
  name: string,
};

type TapOutNodeProps = {
  name: string,
};

export function tapIn(props: TapInNodeProps): NodeRepr_t {
  return createNode("tapIn", props, []);
}

export function tapOut(props: TapOutNodeProps, x: ElemNode): NodeRepr_t {
  return createNode("tapOut", props, [resolve(x)]);
}

// Meter node
type MeterNodeProps = {
  key?: string,
  name?: string,
};

export function meter(x: ElemNode): NodeRepr_t;
export function meter(props: MeterNodeProps, x: ElemNode): NodeRepr_t;
export function meter(a, b?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode("meter", {}, [resolve(a)]);
  }

  return createNode("meter", a, [resolve(b)]);
}

// Snapshot node
type SnapshotNodeProps = {
  key?: string,
  name?: string,
};

export function snapshot(trigger: ElemNode, x: ElemNode): NodeRepr_t;
export function snapshot(props: SnapshotNodeProps, trigger: ElemNode, x: ElemNode): NodeRepr_t;
export function snapshot(a, b, c?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode("snapshot", {}, [resolve(a), resolve(b)]);
  }

  return createNode("snapshot", a, [resolve(b), resolve(c)]);
}

// Scope node
type ScopeNodeProps = {
  key?: string,
  name?: string,
  size?: number,
  channels?: number,
};

export function scope(...args : Array<ElemNode>): NodeRepr_t;
export function scope(props: ScopeNodeProps, ...args : Array<ElemNode>): NodeRepr_t;
export function scope(a, ...bs) {
  if (typeof a === "number" || isNode(a)) {
    return createNode("scope", {}, [a, ...bs].map(resolve));
  }

  return createNode("scope", a, bs.map(resolve));
}

// FFT node
type FFTNodeProps = {
  key?: string,
  name?: string,
  size?: number,
};

export function fft(x: ElemNode): NodeRepr_t;
export function fft(props: FFTNodeProps, x: ElemNode): NodeRepr_t;
export function fft(a, b?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode("fft", {}, [resolve(a)]);
  }

  return createNode("fft", a, [resolve(b)]);
}

// Capture node
type CaptureNodeProps = {
  key?: string,
  size?: number,
};

export function capture(g: ElemNode, x: ElemNode): NodeRepr_t;
export function capture(props: CaptureNodeProps, g: ElemNode, x: ElemNode): NodeRepr_t;
export function capture(a, b, c?) {
  if (typeof a === "number" || isNode(a)) {
    return createNode("capture", {}, [resolve(a), resolve(b)]);
  }

  return createNode("capture", a, [resolve(b), resolve(c)]);
}
