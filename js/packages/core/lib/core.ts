import {
  createNode,
  isNode,
  resolve,
  ElemNode,
  NodeRepr_t,
} from "../nodeUtils";

export function constant(props: { key?: string; value: number }): NodeRepr_t {
  return createNode("const", props, []);
}

export function sr(): NodeRepr_t {
  return createNode("sr", {}, []);
}

export function time(): NodeRepr_t {
  return createNode("time", {}, []);
}

export function counter(gate: ElemNode): NodeRepr_t {
  return createNode("counter", {}, [resolve(gate)]);
}

export function accum(xn: ElemNode, reset: ElemNode): NodeRepr_t {
  return createNode("accum", {}, [resolve(xn), resolve(reset)]);
}

// Phasor node
export function phasor(rate: ElemNode): NodeRepr_t {
  return createNode("phasor", {}, [resolve(rate)]);
}

export function syncphasor(rate: ElemNode, reset: ElemNode): NodeRepr_t {
  return createNode("sphasor", {}, [resolve(rate), resolve(reset)]);
}

export function latch(t: ElemNode, x: ElemNode): NodeRepr_t {
  return createNode("latch", {}, [resolve(t), resolve(x)]);
}

export function maxhold(
  props: { key?: string; hold?: number },
  x: ElemNode,
  reset: ElemNode,
): NodeRepr_t {
  return createNode("maxhold", props, [resolve(x), resolve(reset)]);
}

export function once(
  props: { key?: string; arm?: boolean },
  x: ElemNode,
): NodeRepr_t {
  return createNode("once", props, [resolve(x)]);
}

export function rand(props?: { key?: string, seed?: number }): NodeRepr_t {
  return createNode("rand", props || {}, []);
}

export function metro(props?: {
  key?: string;
  name?: string;
  interval?: number;
}): NodeRepr_t {
  return createNode("metro", props || {}, []);
}

export function sample(
  props: {
    key?: string;
    path: string;
    mode?: string;
    startOffset?: number;
    stopOffset?: number;
  },
  trigger: ElemNode,
  rate: ElemNode,
): NodeRepr_t {
  return createNode("sample", props, [resolve(trigger), resolve(rate)]);
}

export function table(
  props: {
    key?: string;
    path: string;
  },
  t: ElemNode,
): NodeRepr_t {
  return createNode("table", props, [resolve(t)]);
}

export function convolve(
  props: {
    key?: string;
    path: string;
  },
  x: ElemNode,
): NodeRepr_t {
  return createNode("convolve", props, [resolve(x)]);
}

export function seq(
  props: {
    key?: string;
    seq: Array<number>;
    offset?: number;
    hold?: boolean;
    loop?: boolean;
  },
  trigger: ElemNode,
  reset: ElemNode,
): NodeRepr_t {
  return createNode("seq", props, [resolve(trigger), resolve(reset)]);
}

export function seq2(
  props: {
    key?: string;
    seq: Array<number>;
    offset?: number;
    hold?: boolean;
    loop?: boolean;
  },
  trigger: ElemNode,
  reset: ElemNode,
): NodeRepr_t {
  return createNode("seq2", props, [resolve(trigger), resolve(reset)]);
}

export function sparseq(
  props: {
    key?: string;
    seq: Array<{ value: number; tickTime: number }>;
    offset?: number;
    loop?: boolean | Array<number>;
    interpolate?: number;
    tickInterval?: number;
  },
  trigger: ElemNode,
  reset: ElemNode,
): NodeRepr_t {
  return createNode("sparseq", props, [resolve(trigger), resolve(reset)]);
}

export function sparseq2(
  props: {
    key?: string;
    seq: Array<{ value: number; time: number }>;
  },
  time: ElemNode,
): NodeRepr_t {
  return createNode("sparseq2", props, [resolve(time)]);
}

export function sampleseq(
  props: {
    key?: string;
    path: string;
    seq: Array<{ value: number; time: number }>;
    duration: number;
  },
  time: ElemNode,
): NodeRepr_t {
  return createNode("sampleseq", props, [resolve(time)]);
}

export function sampleseq2(
  props: {
    key?: string;
    path: string;
    seq: Array<{ value: number; time: number }>;
    duration: number;
    stretch?: number;
    shift?: number;
  },
  time: ElemNode,
): NodeRepr_t {
  return createNode("sampleseq2", props, [resolve(time)]);
}

export function pole(p: ElemNode, x: ElemNode): NodeRepr_t {
  return createNode("pole", {}, [resolve(p), resolve(x)]);
}

export function env(
  atkPole: ElemNode,
  relPole: ElemNode,
  x: ElemNode,
): NodeRepr_t {
  return createNode("env", {}, [
    resolve(atkPole),
    resolve(relPole),
    resolve(x),
  ]);
}

// Single sample delay node
export function z(x: ElemNode): NodeRepr_t {
  return createNode("z", {}, [resolve(x)]);
}

// Variable delay node
export function delay(
  props: {
    key?: string;
    size: number;
  },
  len: ElemNode,
  fb: ElemNode,
  x: ElemNode,
): NodeRepr_t {
  return createNode("delay", props, [resolve(len), resolve(fb), resolve(x)]);
}

// Static delay node
export function sdelay(
  props: {
    key?: string;
    size: number;
  },
  x: ElemNode,
): NodeRepr_t {
  return createNode("sdelay", props, [resolve(x)]);
}

// Multimode1p
export function prewarp(fc: ElemNode): NodeRepr_t {
  return createNode("prewarp", {}, [resolve(fc)]);
}

export function mm1p(
  props: {
    key?: string;
    mode?: string;
  },
  fc: ElemNode,
  x: ElemNode,
): NodeRepr_t {
  return createNode("mm1p", props, [resolve(fc), resolve(x)]);
}

export function svf(
  props: {
    key?: string;
    mode?: string;
  },
  fc: ElemNode,
  q: ElemNode,
  x: ElemNode,
): NodeRepr_t {
  return createNode("svf", props, [resolve(fc), resolve(q), resolve(x)]);
}

export function svfshelf(
  props: {
    key?: string;
    mode?: string;
  },
  fc: ElemNode,
  q: ElemNode,
  gainDecibels: ElemNode,
  x: ElemNode,
): NodeRepr_t {
  return createNode("svfshelf", props, [
    resolve(fc),
    resolve(q),
    resolve(gainDecibels),
    resolve(x),
  ]);
}

export function biquad(
  b0: ElemNode,
  b1: ElemNode,
  b2: ElemNode,
  a1: ElemNode,
  a2: ElemNode,
  x: ElemNode,
): NodeRepr_t {
  return createNode("biquad", {}, [
    resolve(b0),
    resolve(b1),
    resolve(b2),
    resolve(a1),
    resolve(a2),
    resolve(x),
  ]);
}

// Feedback nodes
export function tapIn(props: { key?: string; name: string }): NodeRepr_t {
  return createNode("tapIn", props, []);
}

export function tapOut(
  props: { key?: string; name: string },
  x: ElemNode,
): NodeRepr_t {
  return createNode("tapOut", props, [resolve(x)]);
}

// Analysis
export function meter(
  props: {
    key?: string;
    name?: string;
  },
  x: ElemNode,
): NodeRepr_t {
  return createNode("meter", props, [resolve(x)]);
}

export function snapshot(
  props: {
    key?: string;
    name?: string;
  },
  trigger: ElemNode,
  x: ElemNode,
): NodeRepr_t {
  return createNode("snapshot", props, [resolve(trigger), resolve(x)]);
}

export function scope(
  props: {
    key?: string;
    name?: string;
    size?: number;
    channels?: number;
  },
  ...args: Array<ElemNode>
): NodeRepr_t {
  return createNode("scope", props, args.map(resolve));
}

export function fft(
  props: {
    key?: string;
    name?: string;
    size?: number;
  },
  x: ElemNode,
): NodeRepr_t {
  return createNode("fft", props, [resolve(x)]);
}

export function capture(
  props: {
    key?: string;
  },
  g: ElemNode,
  x: ElemNode,
): NodeRepr_t {
  return createNode("capture", props, [resolve(g), resolve(x)]);
}
