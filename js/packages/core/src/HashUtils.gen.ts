/* TypeScript file generated from HashUtils.res by genType. */
/* eslint-disable import/first */


// @ts-ignore: Implicit any on import
import * as HashUtilsBS__Es6Import from './HashUtils.bs';
const HashUtilsBS: any = HashUtilsBS__Es6Import;

import type {Dict_t as Js_Dict_t} from './Js.gen';

import type {list} from '../src/shims/RescriptPervasives.shim';

export const hashString: (_1:number, _2:string) => number = HashUtilsBS.hashString;

export const hashNode: (_1:string, _2:Js_Dict_t<string>, _3:list<number>) => number = HashUtilsBS.hashNode;

export const hashMemoInputs: (_1:{ readonly memoKey: string }, _2:list<number>) => number = HashUtilsBS.hashMemoInputs;
