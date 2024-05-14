import {
  createNode,
  unpack,
  resolve,
} from '../nodeUtils';

import type {ElemNode, NodeRepr_t} from '../nodeUtils';


export function sampleseq(props: {
  key?: string,
  seq?: Array<{value: number, time: number}>,
  duration: number,
  path: string,
  channels: number,
}, time: ElemNode): Array<NodeRepr_t> {
  let {channels, ...other} = props;
  return unpack(createNode("mc.sampleseq", other, [resolve(time)]), channels);
}

export function sampleseq2(props: {
  key?: string,
  seq?: Array<{value: number, time: number}>,
  duration: number,
  path: string,
  stretch?: number,
  shift?: number,
  channels: number,
}, time: ElemNode): Array<NodeRepr_t> {
  let {channels, ...other} = props;
  return unpack(createNode("mc.sampleseq2", other, [resolve(time)]), channels);
}

export function table(props: {
  key?: string,
  path: string,
  channels: number,
}, t: ElemNode): Array<NodeRepr_t> {
  let {channels, ...other} = props;
  return unpack(createNode("mc.table", other, [resolve(t)]), channels);
}
