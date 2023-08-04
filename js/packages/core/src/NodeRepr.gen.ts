/* TypeScript file generated from NodeRepr.res by genType. */
/* eslint-disable import/first */


// @ts-ignore: Implicit any on import
import * as Curry__Es6Import from 'rescript/lib/es6/curry.js';
const Curry: any = Curry__Es6Import;

// @ts-ignore: Implicit any on import
import * as NodeReprBS__Es6Import from './NodeRepr.bs';
const NodeReprBS: any = NodeReprBS__Es6Import;

import type {list} from '../src/shims/RescriptPervasives.shim';

// tslint:disable-next-line:max-classes-per-file 
// tslint:disable-next-line:class-name
export abstract class props { protected opaque!: any }; /* simulate opaque types */

// tslint:disable-next-line:interface-over-type-literal
export type t = {
  readonly symbol: string; 
  readonly hash: number; 
  readonly kind: string; 
  readonly props: props; 
  readonly children: list<t>
};

// tslint:disable-next-line:interface-over-type-literal
export type shallow = {
  readonly symbol: string; 
  readonly hash: number; 
  readonly kind: string; 
  readonly props: props; 
  readonly generation: {
    contents: number
  }
};

export const create: (kind:string, props:{}, children:t[]) => t = function (Arg1: any, Arg2: any, Arg3: any) {
  const result = Curry._3(NodeReprBS.create, Arg1, Arg2, Arg3);
  return result
};

export const isNode: <T1>(a:{ readonly symbol: T1 }) => boolean = NodeReprBS.isNode;

export const shallowCopy: (node:t) => shallow = NodeReprBS.shallowCopy;
