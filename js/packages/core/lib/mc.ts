import {
  createNode,
  isNode,
  resolve,
  ElemNode,
  NodeRepr_t,
  unpack,
} from "../nodeUtils";

import invariant from "invariant";

export function sample(
  props: {
    key?: string;
    path: string;
    channels: number;
    mode?: string;
    startOffset?: number;
    stopOffset?: number;
    playbackRate?: number;
  },
  gate: ElemNode,
): Array<NodeRepr_t> {
  let { channels, ...other } = props;

  invariant(
    typeof channels === "number" && channels > 0,
    "Must provide a positive number channels prop",
  );

  return unpack(createNode("mc.sample", other, [resolve(gate)]), channels);
}

export function sampleseq(
  props: {
    key?: string;
    seq: Array<{ value: number; time: number }>;
    duration: number;
    path: string;
    channels: number;
  },
  time: ElemNode,
): Array<NodeRepr_t> {
  let { channels, ...other } = props;

  invariant(
    typeof channels === "number" && channels > 0,
    "Must provide a positive number channels prop",
  );

  return unpack(createNode("mc.sampleseq", other, [resolve(time)]), channels);
}

export function sampleseq2(
  props: {
    key?: string;
    seq: Array<{ value: number; time: number }>;
    duration: number;
    path: string;
    stretch?: number;
    shift?: number;
    channels: number;
  },
  time: ElemNode,
): Array<NodeRepr_t> {
  let { channels, ...other } = props;

  invariant(
    typeof channels === "number" && channels > 0,
    "Must provide a positive number channels prop",
  );

  return unpack(createNode("mc.sampleseq2", other, [resolve(time)]), channels);
}

export function table(
  props: {
    key?: string;
    path: string;
    channels: number;
  },
  t: ElemNode,
): Array<NodeRepr_t> {
  let { channels, ...other } = props;

  invariant(
    typeof channels === "number" && channels > 0,
    "Must provide a positive number channels prop",
  );

  return unpack(createNode("mc.table", other, [resolve(t)]), channels);
}

export function capture(
  props: {
    name?: string;
    channels: number;
  },
  g: ElemNode,
  ...args: Array<NodeRepr_t>
): Array<NodeRepr_t> {
  let { channels, ...other } = props;

  invariant(
    typeof channels === "number" && channels > 0,
    "Must provide a positive number channels prop",
  );

  return unpack(
    createNode("mc.capture", other, [resolve(g), ...args.map(resolve)]),
    channels,
  );
}
