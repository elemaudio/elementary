import {
  NodeRepr_createPrimitive,
  NodeRepr_createComposite,
  NodeRepr_isNode,
} from './src/Reconciler.gen';

import invariant from 'invariant';

import type {NodeRepr_t} from './src/Reconciler.gen';


export function resolve(n : NodeRepr_t | number): NodeRepr_t {
  return typeof n === 'number'
    ? NodeRepr_createPrimitive("const", {value: n}, [])
    : n;
}

export function isNode(n: any): boolean {
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
