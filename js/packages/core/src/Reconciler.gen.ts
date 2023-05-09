/* TypeScript file generated from Reconciler.res by genType. */
/* eslint-disable import/first */


// @ts-ignore: Implicit any on import
import * as Curry__Es6Import from 'rescript/lib/es6/curry.js';
const Curry: any = Curry__Es6Import;

// @ts-ignore: Implicit any on import
import * as ReconcilerBS__Es6Import from './Reconciler.bs';
const ReconcilerBS: any = ReconcilerBS__Es6Import;

import type {NodeRepr_symbol as $$NodeRepr_symbol} from './Symbol';

import type {list} from '../src/shims/RescriptPervasives.shim';

// tslint:disable-next-line:max-classes-per-file 
export abstract class NodeRepr_props { protected opaque!: any }; /* simulate opaque types */

// tslint:disable-next-line:interface-over-type-literal
export type NodeRepr_renderContext = {
  readonly sampleRate: number; 
  readonly blockSize: number; 
  readonly numInputs: number; 
  readonly numOutputs: number
};

// tslint:disable-next-line:interface-over-type-literal
export type NodeRepr_symbol = $$NodeRepr_symbol;

// tslint:disable-next-line:interface-over-type-literal
export type NodeRepr_t = {
  readonly symbol: NodeRepr_symbol; 
  hash?: number; 
  readonly kind: 
    {
    NAME: "Composite"; 
    VAL: (_1:{
    readonly props: NodeRepr_props; 
    readonly context: NodeRepr_renderContext; 
    readonly children: NodeRepr_t[]
  }) => NodeRepr_t
  }
  | {
    NAME: "Primitive"; 
    VAL: string
  }; 
  readonly props: NodeRepr_props; 
  readonly children: list<NodeRepr_t>
};

// tslint:disable-next-line:interface-over-type-literal
export type NodeRepr_shallow = {
  readonly symbol: NodeRepr_symbol; 
  readonly hash: number; 
  readonly kind: string; 
  readonly props: NodeRepr_props; 
  readonly generation: {
    contents: number
  }
};

// tslint:disable-next-line:max-classes-per-file 
export abstract class RenderDelegate_t { protected opaque!: any }; /* simulate opaque types */

export const NodeRepr_createPrimitive: <T1>(kind:string, props:T1, children:NodeRepr_t[]) => NodeRepr_t = function <T1>(Arg1: any, Arg2: any, Arg3: any) {
  const result = Curry._3(
/* WARNING: circular type NodeRepr_t. Only shallow converter applied. */
  ReconcilerBS.NodeRepr.createPrimitive, Arg1, Arg2, Arg3);
  return result
};

export const NodeRepr_createComposite: <T1>(kind:((_1:{
  readonly children: NodeRepr_t[]; 
  readonly context: NodeRepr_renderContext; 
  readonly props: NodeRepr_props
}) => NodeRepr_t), props:T1, children:NodeRepr_t[]) => NodeRepr_t = function <T1>(Arg1: any, Arg2: any, Arg3: any) {
  const result = Curry._3(
/* WARNING: circular type NodeRepr_t. Only shallow converter applied. */
  ReconcilerBS.NodeRepr.createComposite, Arg1, Arg2, Arg3);
  return result
};

export const NodeRepr_isNode: <T1>(a:{ readonly symbol: T1 }) => boolean = ReconcilerBS.NodeRepr.isNode;

export const NodeRepr_getHashUnchecked: (n:NodeRepr_t) => number = ReconcilerBS.NodeRepr.getHashUnchecked;

export const NodeRepr_shallowCopy: (node:NodeRepr_t) => NodeRepr_shallow = ReconcilerBS.NodeRepr.shallowCopy;

export const renderWithDelegate: (delegate:RenderDelegate_t, graphs:NodeRepr_t[]) => void = function (Arg1: any, Arg2: any) {
  const result = Curry._2(
/* WARNING: circular type NodeRepr_t. Only shallow converter applied. */
  ReconcilerBS.renderWithDelegate, Arg1, Arg2);
  return result
};

export const stepGarbageCollector: (delegate:RenderDelegate_t) => void = ReconcilerBS.stepGarbageCollector;
