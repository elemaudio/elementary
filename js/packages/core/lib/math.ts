import {
  createNode,
  isNode,
  resolve,
  ElemNode,
  NodeRepr_t,
} from "../nodeUtils";

// Unary nodes
export function identity(
  props: {
    key?: string;
    channel?: number;
  },
  x?: ElemNode,
): NodeRepr_t {
  if (isNode(x)) {
    return createNode("in", props, [x]);
  }

  return createNode("in", props, []);
}

export function sin(x: ElemNode): NodeRepr_t {
  return createNode("sin", {}, [resolve(x)]);
}

export function cos(x: ElemNode): NodeRepr_t {
  return createNode("cos", {}, [resolve(x)]);
}

export function tan(x: ElemNode): NodeRepr_t {
  return createNode("tan", {}, [resolve(x)]);
}

export function tanh(x: ElemNode): NodeRepr_t {
  return createNode("tanh", {}, [resolve(x)]);
}

export function asinh(x: ElemNode): NodeRepr_t {
  return createNode("asinh", {}, [resolve(x)]);
}

export function ln(x: ElemNode): NodeRepr_t {
  return createNode("ln", {}, [resolve(x)]);
}

export function log(x: ElemNode): NodeRepr_t {
  return createNode("log", {}, [resolve(x)]);
}

export function log2(x: ElemNode): NodeRepr_t {
  return createNode("log2", {}, [resolve(x)]);
}

export function ceil(x: ElemNode): NodeRepr_t {
  return createNode("ceil", {}, [resolve(x)]);
}

export function floor(x: ElemNode): NodeRepr_t {
  return createNode("floor", {}, [resolve(x)]);
}

export function round(x: ElemNode): NodeRepr_t {
  return createNode("round", {}, [resolve(x)]);
}

export function sqrt(x: ElemNode): NodeRepr_t {
  return createNode("sqrt", {}, [resolve(x)]);
}

export function exp(x: ElemNode): NodeRepr_t {
  return createNode("exp", {}, [resolve(x)]);
}

export function abs(x: ElemNode): NodeRepr_t {
  return createNode("abs", {}, [resolve(x)]);
}

// Binary nodes
export function le(a: ElemNode, b: ElemNode): NodeRepr_t {
  return createNode("le", {}, [resolve(a), resolve(b)]);
}

export function leq(a: ElemNode, b: ElemNode): NodeRepr_t {
  return createNode("leq", {}, [resolve(a), resolve(b)]);
}

export function ge(a: ElemNode, b: ElemNode): NodeRepr_t {
  return createNode("ge", {}, [resolve(a), resolve(b)]);
}

export function geq(a: ElemNode, b: ElemNode): NodeRepr_t {
  return createNode("geq", {}, [resolve(a), resolve(b)]);
}

export function pow(a: ElemNode, b: ElemNode): NodeRepr_t {
  return createNode("pow", {}, [resolve(a), resolve(b)]);
}

export function eq(a: ElemNode, b: ElemNode): NodeRepr_t {
  return createNode("eq", {}, [resolve(a), resolve(b)]);
}

export function and(a: ElemNode, b: ElemNode): NodeRepr_t {
  return createNode("and", {}, [resolve(a), resolve(b)]);
}

export function or(a: ElemNode, b: ElemNode): NodeRepr_t {
  return createNode("or", {}, [resolve(a), resolve(b)]);
}

// Binary reducing nodes
export function add(...args: Array<ElemNode>): NodeRepr_t {
  return createNode("add", {}, args.map(resolve));
}

export function sub(...args: Array<ElemNode>): NodeRepr_t {
  return createNode("sub", {}, args.map(resolve));
}

export function mul(...args: Array<ElemNode>): NodeRepr_t {
  return createNode("mul", {}, args.map(resolve));
}

export function div(...args: Array<ElemNode>): NodeRepr_t {
  return createNode("div", {}, args.map(resolve));
}

export function mod(...args: Array<ElemNode>): NodeRepr_t {
  return createNode("mod", {}, args.map(resolve));
}

export function min(...args: Array<ElemNode>): NodeRepr_t {
  return createNode("min", {}, args.map(resolve));
}

export function max(...args: Array<ElemNode>): NodeRepr_t {
  return createNode("max", {}, args.map(resolve));
}
