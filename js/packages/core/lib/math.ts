import {
  createNode,
  isNode,
  resolve,
} from '../nodeUtils';

import type {ElemNode, NodeRepr_t} from '../nodeUtils';


type OptionalKeyProps = {
  key?: string,
}

// Unary nodes
type IdentityNodeProps = {
  key?: string,
  channel?: number,
}

export function identity(x: ElemNode): NodeRepr_t;
export function identity(props: IdentityNodeProps): NodeRepr_t;
export function identity(props: IdentityNodeProps, x: ElemNode): NodeRepr_t;
export function identity(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("in", {}, [resolve(a)])
    : (typeof b === "number" || isNode(b))
      ? createNode("in", a, [resolve(b)])
      : createNode("in", a, []);
}

export function sin(x: ElemNode): NodeRepr_t;
export function sin(props: OptionalKeyProps, x: ElemNode): NodeRepr_t;
export function sin(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("sin", {}, [resolve(a)])
    : createNode("sin", a, [resolve(b)]);
}

export function cos(x: ElemNode): NodeRepr_t;
export function cos(props: OptionalKeyProps, x: ElemNode): NodeRepr_t;
export function cos(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("cos", {}, [resolve(a)])
    : createNode("cos", a, [resolve(b)]);
}

export function tan(x: ElemNode): NodeRepr_t;
export function tan(props: OptionalKeyProps, x: ElemNode): NodeRepr_t;
export function tan(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("tan", {}, [resolve(a)])
    : createNode("tan", a, [resolve(b)]);
}

export function tanh(x: ElemNode): NodeRepr_t;
export function tanh(props: OptionalKeyProps, x: ElemNode): NodeRepr_t;
export function tanh(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("tanh", {}, [resolve(a)])
    : createNode("tanh", a, [resolve(b)]);
}

export function asinh(x: ElemNode): NodeRepr_t;
export function asinh(props: OptionalKeyProps, x: ElemNode): NodeRepr_t;
export function asinh(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("asinh", {}, [resolve(a)])
    : createNode("asinh", a, [resolve(b)]);
}

export function ln(x: ElemNode): NodeRepr_t;
export function ln(props: OptionalKeyProps, x: ElemNode): NodeRepr_t;
export function ln(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("ln", {}, [resolve(a)])
    : createNode("ln", a, [resolve(b)]);
}

export function log(x: ElemNode): NodeRepr_t;
export function log(props: OptionalKeyProps, x: ElemNode): NodeRepr_t;
export function log(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("log", {}, [resolve(a)])
    : createNode("log", a, [resolve(b)]);
}

export function log2(x: ElemNode): NodeRepr_t;
export function log2(props: OptionalKeyProps, x: ElemNode): NodeRepr_t;
export function log2(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("log2", {}, [resolve(a)])
    : createNode("log2", a, [resolve(b)]);
}

export function ceil(x: ElemNode): NodeRepr_t;
export function ceil(props: OptionalKeyProps, x: ElemNode): NodeRepr_t;
export function ceil(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("ceil", {}, [resolve(a)])
    : createNode("ceil", a, [resolve(b)]);
}

export function floor(x: ElemNode): NodeRepr_t;
export function floor(props: OptionalKeyProps, x: ElemNode): NodeRepr_t;
export function floor(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("floor", {}, [resolve(a)])
    : createNode("floor", a, [resolve(b)]);
}

export function round(x: ElemNode): NodeRepr_t;
export function round(props: OptionalKeyProps, x: ElemNode): NodeRepr_t;
export function round(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("round", {}, [resolve(a)])
    : createNode("round", a, [resolve(b)]);
}

export function sqrt(x: ElemNode): NodeRepr_t;
export function sqrt(props: OptionalKeyProps, x: ElemNode): NodeRepr_t;
export function sqrt(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("sqrt", {}, [resolve(a)])
    : createNode("sqrt", a, [resolve(b)]);
}

export function exp(x: ElemNode): NodeRepr_t;
export function exp(props: OptionalKeyProps, x: ElemNode): NodeRepr_t;
export function exp(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("exp", {}, [resolve(a)])
    : createNode("exp", a, [resolve(b)]);
}

export function abs(x: ElemNode): NodeRepr_t;
export function abs(props: OptionalKeyProps, x: ElemNode): NodeRepr_t;
export function abs(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("abs", {}, [resolve(a)])
    : createNode("abs", a, [resolve(b)]);
}

// Binary nodes
export function le(a: ElemNode, b: ElemNode): NodeRepr_t;
export function le(props: OptionalKeyProps, a: ElemNode, b: ElemNode): NodeRepr_t;
export function le(a, b, c?) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("le", {}, [resolve(a), resolve(b)]);
  }

  return createNode("le", a, [resolve(b), resolve(c)]);
}

export function leq(a: ElemNode, b: ElemNode): NodeRepr_t;
export function leq(props: OptionalKeyProps, a: ElemNode, b: ElemNode): NodeRepr_t;
export function leq(a, b, c?) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("leq", {}, [resolve(a), resolve(b)]);
  }

  return createNode("leq", a, [resolve(b), resolve(c)]);
}

export function ge(a: ElemNode, b: ElemNode): NodeRepr_t;
export function ge(props: OptionalKeyProps, a: ElemNode, b: ElemNode): NodeRepr_t;
export function ge(a, b, c?) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("ge", {}, [resolve(a), resolve(b)]);
  }

  return createNode("ge", a, [resolve(b), resolve(c)]);
}

export function geq(a: ElemNode, b: ElemNode): NodeRepr_t;
export function geq(props: OptionalKeyProps, a: ElemNode, b: ElemNode): NodeRepr_t;
export function geq(a, b, c?) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("geq", {}, [resolve(a), resolve(b)]);
  }

  return createNode("geq", a, [resolve(b), resolve(c)]);
}

export function pow(a: ElemNode, b: ElemNode): NodeRepr_t;
export function pow(props: OptionalKeyProps, a: ElemNode, b: ElemNode): NodeRepr_t;
export function pow(a, b, c?) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("pow", {}, [resolve(a), resolve(b)]);
  }

  return createNode("pow", a, [resolve(b), resolve(c)]);
}

export function eq(a: ElemNode, b: ElemNode): NodeRepr_t;
export function eq(props: OptionalKeyProps, a: ElemNode, b: ElemNode): NodeRepr_t;
export function eq(a, b, c?) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("eq", {}, [resolve(a), resolve(b)]);
  }

  return createNode("eq", a, [resolve(b), resolve(c)]);
}

export function and(a: ElemNode, b: ElemNode): NodeRepr_t;
export function and(props: OptionalKeyProps, a: ElemNode, b: ElemNode): NodeRepr_t;
export function and(a, b, c?) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("and", {}, [resolve(a), resolve(b)]);
  }

  return createNode("and", a, [resolve(b), resolve(c)]);
}

export function or(a: ElemNode, b: ElemNode): NodeRepr_t;
export function or(props: OptionalKeyProps, a: ElemNode, b: ElemNode): NodeRepr_t;
export function or(a, b, c?) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("or", {}, [resolve(a), resolve(b)]);
  }

  return createNode("or", a, [resolve(b), resolve(c)]);
}

// Binary reducing nodes
export function add(...args : Array<ElemNode>): NodeRepr_t;
export function add(props: OptionalKeyProps, ...args : Array<ElemNode>): NodeRepr_t;
export function add(a, ...bs) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("add", {}, [a, ...bs].map(resolve));
  }

  return createNode("add", a, bs.map(resolve));
}

export function sub(...args : Array<ElemNode>): NodeRepr_t;
export function sub(props: OptionalKeyProps, ...args : Array<ElemNode>): NodeRepr_t;
export function sub(a, ...bs) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("sub", {}, [a, ...bs].map(resolve));
  }

  return createNode("sub", a, bs.map(resolve));
}

export function mul(...args : Array<ElemNode>): NodeRepr_t;
export function mul(props: OptionalKeyProps, ...args : Array<ElemNode>): NodeRepr_t;
export function mul(a, ...bs) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("mul", {}, [a, ...bs].map(resolve));
  }

  return createNode("mul", a, bs.map(resolve));
}

export function div(...args : Array<ElemNode>): NodeRepr_t;
export function div(props: OptionalKeyProps, ...args : Array<ElemNode>): NodeRepr_t;
export function div(a, ...bs) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("div", {}, [a, ...bs].map(resolve));
  }

  return createNode("div", a, bs.map(resolve));
}

export function mod(...args : Array<ElemNode>): NodeRepr_t;
export function mod(props: OptionalKeyProps, ...args : Array<ElemNode>): NodeRepr_t;
export function mod(a, ...bs) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("mod", {}, [a, ...bs].map(resolve));
  }

  return createNode("mod", a, bs.map(resolve));
}

export function min(...args : Array<ElemNode>): NodeRepr_t;
export function min(props: OptionalKeyProps, ...args : Array<ElemNode>): NodeRepr_t;
export function min(a, ...bs) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("min", {}, [a, ...bs].map(resolve));
  }

  return createNode("min", a, bs.map(resolve));
}

export function max(...args : Array<ElemNode>): NodeRepr_t;
export function max(props: OptionalKeyProps, ...args : Array<ElemNode>): NodeRepr_t;
export function max(a, ...bs) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("max", {}, [a, ...bs].map(resolve));
  }

  return createNode("max", a, bs.map(resolve));
}
