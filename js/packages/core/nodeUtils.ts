import {
  NodeRepr_createPrimitive,
  NodeRepr_createComposite,
  NodeRepr_isNode,
} from './src/Reconciler.gen';

import invariant from 'invariant';

import type {NodeRepr_t} from './src/Reconciler.gen';


export function resolve(n : NodeRepr_t | number): NodeRepr_t {
  if (typeof n === 'number')
    return NodeRepr_createPrimitive("const", {value: n}, []);

  invariant(isNode(n), `Whoops, expecting a Node type here! Got: ${typeof n}`);
  return n;
}

export function isNode(n: unknown): n is NodeRepr_t {
  // We cannot pass `unknown` type to the underlying method generated from ReScript,
  // but we'd like to keep the signature of this method's API to be more semantically correct (use `unknown` instead of `any`).
  // That's why we're using "@ts-expect-error" here.
  // Once this resolved, the TS error pops up and we can remove it.
  // @ts-expect-error
  return NodeRepr_isNode(n);
}

export function createNode(
  kind: Parameters<typeof NodeRepr_createPrimitive>[0] | Parameters<typeof NodeRepr_createComposite>[0],
  props,
  children: Array<NodeRepr_t | number>
): NodeRepr_t {
  invariant(children.length <= 8, `Nodes can only have at most 8 children.`);

  if (typeof kind === 'string') {
    return NodeRepr_createPrimitive(kind, props, children.map(resolve));
  }

  return NodeRepr_createComposite(kind, props, children.map(resolve));
}
