import { create, isNode as NodeRepr_isNode } from './src/NodeRepr.gen';
import type { t as NodeRepr_t } from './src/NodeRepr.gen';

import invariant from 'invariant';


export type ElemNode = NodeRepr_t | number;
export type { NodeRepr_t };

export function resolve(n : ElemNode): NodeRepr_t {
  if (typeof n === 'number')
    return create("const", {value: n}, []);

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
  kind: string,
  props,
  children: Array<ElemNode>
): NodeRepr_t {
  return create(kind, props, children.map(resolve));
}
