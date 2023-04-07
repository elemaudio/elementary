import {
  createNode,
  isNode,
  resolve,
} from '../nodeUtils';

import type {NodeRepr_t} from '../src/Reconciler.gen';


type OptionalKeyProps = {
  key?: string,
}

// Unary nodes
type IdentityNodeProps = {
  key?: string,
  channel?: number,
}

export function identity(x: NodeRepr_t | number): NodeRepr_t;
export function identity(props: IdentityNodeProps): NodeRepr_t;
export function identity(props: IdentityNodeProps, x: NodeRepr_t | number): NodeRepr_t;
export function identity(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("in", {}, [resolve(a)])
    : (typeof b === "number" || isNode(b))
      ? createNode("in", a, [resolve(b)])
      : createNode("in", a, []);
}

export function sin(x: NodeRepr_t | number): NodeRepr_t | number;
export function sin(props: OptionalKeyProps, x: NodeRepr_t | number): NodeRepr_t | number;
export function sin(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("sin", {}, [resolve(a)])
    : createNode("sin", a, [resolve(b)]);
}

export function cos(x: NodeRepr_t | number): NodeRepr_t | number;
export function cos(props: OptionalKeyProps, x: NodeRepr_t | number): NodeRepr_t | number;
export function cos(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("cos", {}, [resolve(a)])
    : createNode("cos", a, [resolve(b)]);
}

export function tan(x: NodeRepr_t | number): NodeRepr_t | number;
export function tan(props: OptionalKeyProps, x: NodeRepr_t | number): NodeRepr_t | number;
export function tan(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("tan", {}, [resolve(a)])
    : createNode("tan", a, [resolve(b)]);
}

export function tanh(x: NodeRepr_t | number): NodeRepr_t | number;
export function tanh(props: OptionalKeyProps, x: NodeRepr_t | number): NodeRepr_t | number;
export function tanh(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("tanh", {}, [resolve(a)])
    : createNode("tanh", a, [resolve(b)]);
}

export function asinh(x: NodeRepr_t | number): NodeRepr_t | number;
export function asinh(props: OptionalKeyProps, x: NodeRepr_t | number): NodeRepr_t | number;
export function asinh(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("asinh", {}, [resolve(a)])
    : createNode("asinh", a, [resolve(b)]);
}

export function ln(x: NodeRepr_t | number): NodeRepr_t | number;
export function ln(props: OptionalKeyProps, x: NodeRepr_t | number): NodeRepr_t | number;
export function ln(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("ln", {}, [resolve(a)])
    : createNode("ln", a, [resolve(b)]);
}

export function log(x: NodeRepr_t | number): NodeRepr_t | number;
export function log(props: OptionalKeyProps, x: NodeRepr_t | number): NodeRepr_t | number;
export function log(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("log", {}, [resolve(a)])
    : createNode("log", a, [resolve(b)]);
}

export function log2(x: NodeRepr_t | number): NodeRepr_t | number;
export function log2(props: OptionalKeyProps, x: NodeRepr_t | number): NodeRepr_t | number;
export function log2(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("log2", {}, [resolve(a)])
    : createNode("log2", a, [resolve(b)]);
}

export function ceil(x: NodeRepr_t | number): NodeRepr_t | number;
export function ceil(props: OptionalKeyProps, x: NodeRepr_t | number): NodeRepr_t | number;
export function ceil(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("ceil", {}, [resolve(a)])
    : createNode("ceil", a, [resolve(b)]);
}

export function floor(x: NodeRepr_t | number): NodeRepr_t | number;
export function floor(props: OptionalKeyProps, x: NodeRepr_t | number): NodeRepr_t | number;
export function floor(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("floor", {}, [resolve(a)])
    : createNode("floor", a, [resolve(b)]);
}

export function sqrt(x: NodeRepr_t | number): NodeRepr_t | number;
export function sqrt(props: OptionalKeyProps, x: NodeRepr_t | number): NodeRepr_t | number;
export function sqrt(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("sqrt", {}, [resolve(a)])
    : createNode("sqrt", a, [resolve(b)]);
}

export function exp(x: NodeRepr_t | number): NodeRepr_t | number;
export function exp(props: OptionalKeyProps, x: NodeRepr_t | number): NodeRepr_t | number;
export function exp(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("exp", {}, [resolve(a)])
    : createNode("exp", a, [resolve(b)]);
}

export function abs(x: NodeRepr_t | number): NodeRepr_t | number;
export function abs(props: OptionalKeyProps, x: NodeRepr_t | number): NodeRepr_t | number;
export function abs(a, b?) {
  return (typeof a === "number" || isNode(a))
    ? createNode("abs", {}, [resolve(a)])
    : createNode("abs", a, [resolve(b)]);
}

// Binary nodes
export function le(a: NodeRepr_t | number, b: NodeRepr_t | number): NodeRepr_t | number;
export function le(props: OptionalKeyProps, a: NodeRepr_t | number, b: NodeRepr_t | number): NodeRepr_t | number;
export function le(a, b, c?) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("le", {}, [resolve(a), resolve(b)]);
  }

  return createNode("le", a, [resolve(b), resolve(c)]);
}

export function leq(a: NodeRepr_t | number, b: NodeRepr_t | number): NodeRepr_t | number;
export function leq(props: OptionalKeyProps, a: NodeRepr_t | number, b: NodeRepr_t | number): NodeRepr_t | number;
export function leq(a, b, c?) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("leq", {}, [resolve(a), resolve(b)]);
  }

  return createNode("leq", a, [resolve(b), resolve(c)]);
}

export function ge(a: NodeRepr_t | number, b: NodeRepr_t | number): NodeRepr_t | number;
export function ge(props: OptionalKeyProps, a: NodeRepr_t | number, b: NodeRepr_t | number): NodeRepr_t | number;
export function ge(a, b, c?) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("ge", {}, [resolve(a), resolve(b)]);
  }

  return createNode("ge", a, [resolve(b), resolve(c)]);
}

export function geq(a: NodeRepr_t | number, b: NodeRepr_t | number): NodeRepr_t | number;
export function geq(props: OptionalKeyProps, a: NodeRepr_t | number, b: NodeRepr_t | number): NodeRepr_t | number;
export function geq(a, b, c?) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("geq", {}, [resolve(a), resolve(b)]);
  }

  return createNode("geq", a, [resolve(b), resolve(c)]);
}

export function pow(a: NodeRepr_t | number, b: NodeRepr_t | number): NodeRepr_t | number;
export function pow(props: OptionalKeyProps, a: NodeRepr_t | number, b: NodeRepr_t | number): NodeRepr_t | number;
export function pow(a, b, c?) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("pow", {}, [resolve(a), resolve(b)]);
  }

  return createNode("pow", a, [resolve(b), resolve(c)]);
}

export function eq(a: NodeRepr_t | number, b: NodeRepr_t | number): NodeRepr_t | number;
export function eq(props: OptionalKeyProps, a: NodeRepr_t | number, b: NodeRepr_t | number): NodeRepr_t | number;
export function eq(a, b, c?) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("eq", {}, [resolve(a), resolve(b)]);
  }

  return createNode("eq", a, [resolve(b), resolve(c)]);
}

export function and(a: NodeRepr_t | number, b: NodeRepr_t | number): NodeRepr_t | number;
export function and(props: OptionalKeyProps, a: NodeRepr_t | number, b: NodeRepr_t | number): NodeRepr_t | number;
export function and(a, b, c?) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("and", {}, [resolve(a), resolve(b)]);
  }

  return createNode("and", a, [resolve(b), resolve(c)]);
}

export function or(a: NodeRepr_t | number, b: NodeRepr_t | number): NodeRepr_t | number;
export function or(props: OptionalKeyProps, a: NodeRepr_t | number, b: NodeRepr_t | number): NodeRepr_t | number;
export function or(a, b, c?) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("or", {}, [resolve(a), resolve(b)]);
  }

  return createNode("or", a, [resolve(b), resolve(c)]);
}

// Binary reducing nodes
export function add(...args : Array<NodeRepr_t | number>): NodeRepr_t | number;
export function add(props: OptionalKeyProps, ...args : Array<NodeRepr_t | number>): NodeRepr_t | number;
export function add(a, ...bs) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("add", {}, [a, ...bs].map(resolve));
  }

  return createNode("add", a, bs.map(resolve));
}

export function sub(...args : Array<NodeRepr_t | number>): NodeRepr_t | number;
export function sub(props: OptionalKeyProps, ...args : Array<NodeRepr_t | number>): NodeRepr_t | number;
export function sub(a, ...bs) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("sub", {}, [a, ...bs].map(resolve));
  }

  return createNode("sub", a, bs.map(resolve));
}

export function mul(...args : Array<NodeRepr_t | number>): NodeRepr_t | number;
export function mul(props: OptionalKeyProps, ...args : Array<NodeRepr_t | number>): NodeRepr_t | number;
export function mul(a, ...bs) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("mul", {}, [a, ...bs].map(resolve));
  }

  return createNode("mul", a, bs.map(resolve));
}

export function div(...args : Array<NodeRepr_t | number>): NodeRepr_t | number;
export function div(props: OptionalKeyProps, ...args : Array<NodeRepr_t | number>): NodeRepr_t | number;
export function div(a, ...bs) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("div", {}, [a, ...bs].map(resolve));
  }

  return createNode("div", a, bs.map(resolve));
}

export function mod(...args : Array<NodeRepr_t | number>): NodeRepr_t | number;
export function mod(props: OptionalKeyProps, ...args : Array<NodeRepr_t | number>): NodeRepr_t | number;
export function mod(a, ...bs) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("mod", {}, [a, ...bs].map(resolve));
  }

  return createNode("mod", a, bs.map(resolve));
}

export function min(...args : Array<NodeRepr_t | number>): NodeRepr_t | number;
export function min(props: OptionalKeyProps, ...args : Array<NodeRepr_t | number>): NodeRepr_t | number;
export function min(a, ...bs) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("min", {}, [a, ...bs].map(resolve));
  }

  return createNode("min", a, bs.map(resolve));
}

export function max(...args : Array<NodeRepr_t | number>): NodeRepr_t | number;
export function max(props: OptionalKeyProps, ...args : Array<NodeRepr_t | number>): NodeRepr_t | number;
export function max(a, ...bs) {
  // In a future update we'll collapse literal constants here; need to sort
  // out how keys work in that case.
  if (typeof a === "number" || isNode(a)) {
    return createNode("max", {}, [a, ...bs].map(resolve));
  }

  return createNode("max", a, bs.map(resolve));
}
