/* TypeScript file generated from Reconciler.res by genType. */
/* eslint-disable import/first */


// @ts-ignore: Implicit any on import
import * as Curry__Es6Import from 'rescript/lib/es6/curry.js';
const Curry: any = Curry__Es6Import;

// @ts-ignore: Implicit any on import
import * as ReconcilerBS__Es6Import from './Reconciler.bs';
const ReconcilerBS: any = ReconcilerBS__Es6Import;

import type {t as NodeRepr_t} from './NodeRepr.gen';

// tslint:disable-next-line:max-classes-per-file 
export abstract class RenderDelegate_t { protected opaque!: any }; /* simulate opaque types */

export const renderWithDelegate: (delegate:RenderDelegate_t, graphs:NodeRepr_t[], rootFadeInMs:number, rootFadeOutMs:number) => void = function (Arg1: any, Arg2: any, Arg3: any, Arg4: any) {
  const result = Curry._4(ReconcilerBS.renderWithDelegate, Arg1, Arg2, Arg3, Arg4);
  return result
};
