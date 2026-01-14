import OfflineRenderer from "..";
import { el } from "@elemaudio/core";

const repeat = (n, x) => Array.from({ length: n }).fill(x);
const take = (x, n) => x.slice(0, n);
const round = (x) => [...x.map(Math.round)];

test("midi events", async function () {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 0,
    numOutputChannels: 2,
    sampleRate: 44100,
    blockSize: 128,
  });

  // Graph
  core.render(...el.midinoteunpack({}));

  // Ten blocks of data
  let inps = [];
  let outs = [new Float32Array(128), new Float32Array(128)];

  // Get past the fade-in
  for (let i = 0; i < 20; ++i) {
    core.process(inps, outs);
  }

  // Now we push some events and study the outputs
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

test("midi cc events", async function () {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 0,
    numOutputChannels: 1,
    sampleRate: 44100,
    blockSize: 128,
  });

  // Graph with raw CC output (0-127)
  core.render(el.midicc({ control: 1, channel: 2 }));

  // Data
  let inps = [];
  let outs = [new Float32Array(128)];

  // Get past the fade-in
  for (let i = 0; i < 20; ++i) {
    core.process(inps, outs);
  }

  // Now we push a CC event (CC 1, value 127) and study the outputs
  core.pushMidiEvent(0, new Uint8Array([0xb2, 1, 127]));
  core.process(inps, outs);

  // Should see raw value of 127
  expect(round(take(outs[0], 8))).toMatchObject(repeat(8, 127));

  // On the next block we should see that the value is still held
  core.process(inps, outs);
  expect(round(take(outs[0], 8))).toMatchObject(repeat(8, 127));

  // Now we'll push a new CC value (64) 4 samples into the next block
  core.pushMidiEvent(4, new Uint8Array([0xb2, 1, 64]));
  core.process(inps, outs);

  // Should see transition from 127 to 64 at sample 4
  expect(round(take(outs[0], 8))).toMatchObject([
    127, 127, 127, 127, 64, 64, 64, 64,
  ]);
});

test("param value events", async function () {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 0,
    numOutputChannels: 1,
    sampleRate: 44100,
    blockSize: 128,
  });

  // Graph
  core.render(el.param({ index: 0 }));

  // Data
  let inps = [];
  let outs = [new Float32Array(128)];

  // Get past the fade-in
  for (let i = 0; i < 20; ++i) {
    core.process(inps, outs);
  }

  core.pushParamValueEvent(4, 0, 15.0);
  core.process(inps, outs);
  expect([...take(outs[0], 8)]).toMatchObject([0, 0, 0, 0, ...repeat(4, 15)]);
});
