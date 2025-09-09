import OfflineRenderer from "..";
import { el, createNode, unpack } from "@elemaudio/core";

test("block events", async function () {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 0,
    numOutputChannels: 2,
    sampleRate: 44100,
    blockSize: 128,
  });

  // Graph
  core.render(...unpack(createNode("midinotein", {}, []), 2));

  // Ten blocks of data
  let inps = [];
  let outs = [new Float32Array(128), new Float32Array(128)];

  // Get past the fade-in
  for (let i = 0; i < 20; ++i) {
    core.process(inps, outs);
  }

  // Now we push some events and study the outputs
  const repeat = (n, x) => Array.from({ length: n }).fill(x);
  const take = (x, n) => x.slice(0, n);
  const round = (x) => [...x.map(Math.round)];

  core.pushMidiEvent(0, new Uint8Array([0x90, 60, 127]));
  core.process(inps, outs);

  expect(round(take(outs[0], 8))).toMatchObject(repeat(8, 262));
  expect(round(take(outs[1], 8))).toMatchObject(repeat(8, 1));

  // On the next block we should see that the note is still held
  core.process(inps, outs);
  expect(round(take(outs[0], 8))).toMatchObject(repeat(8, 262));
  expect(round(take(outs[1], 8))).toMatchObject(repeat(8, 1));

  // Now we'll push a note-off 4 samples into the next block
  core.pushMidiEvent(4, new Uint8Array([0x80, 60, 0]));
  core.process(inps, outs);
  expect(round(take(outs[0], 8))).toMatchObject(repeat(8, 262));
  expect(round(take(outs[1], 8))).toMatchObject([1, 1, 1, 1, 0, 0, 0, 0]);
});
