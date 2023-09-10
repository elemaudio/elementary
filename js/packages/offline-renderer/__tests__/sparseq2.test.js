import OfflineRenderer from '../index';
import { el } from '@elemaudio/core';


test('sparseq2 basics', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 0,
    numOutputChannels: 1,
  });

  // Graph
  core.render(
    el.sparseq2({seq: [
      { time: 512 * 10 + 4, value: 1 },
      { time: 512 * 10 + 8, value: 2 },
      { time: 512 * 10 + 12, value: 3 },
      { time: 512 * 10 + 16, value: 4 },
    ]}, el.time())
  );

  // Ten blocks of data
  let inps = [];
  let outs = [new Float32Array(512 * 10)];

  // Get past the fade-in
  core.process(inps, outs);

  // Push another small block. We should see the sequence play out
  inps = [];
  outs = [new Float32Array(32)];

  core.process(inps, outs);
  expect(outs[0]).toMatchSnapshot();
});

test('sparseq2 interp', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 0,
    numOutputChannels: 1,
  });

  // Graph
  core.render(
    el.sparseq2({
      interpolate: 1,
      seq: [
        { time: 512 * 10 + 0, value: 1 },
        { time: 512 * 10 + 4, value: 2 },
        { time: 512 * 10 + 8, value: 3 },
        { time: 512 * 10 + 12, value: 4 },
      ],
    }, el.time())
  );

  // Ten blocks of data
  let inps = [];
  let outs = [new Float32Array(512 * 10)];

  // Get past the fade-in
  core.process(inps, outs);

  // Push another small block. We should see the sequence play out with
  // intermediate interpolated values
  inps = [];
  outs = [new Float32Array(32)];

  core.process(inps, outs);
  expect(outs[0]).toMatchSnapshot();
});

test('sparseq2 looping', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 0,
    numOutputChannels: 1,
  });

  // Graph
  let loop = (start, end, t) => el.add(start, el.mod(t, el.sub(end, start)));

  core.render(
    el.sparseq2({seq: [
      { time: 512 * 10 + 0, value: 1 },
      { time: 512 * 10 + 4, value: 2 },
      { time: 512 * 10 + 8, value: 3 },
      { time: 512 * 10 + 12, value: 4 },
    ]}, loop(5120, 5120 + 16, el.time()))
  );

  // Ten blocks of data
  let inps = [];
  let outs = [new Float32Array(512 * 10)];

  // Get past the fade-in
  core.process(inps, outs);

  // Push another small block. We should see a loop through the sequence
  // on a 16 sample interval
  inps = [];
  outs = [new Float32Array(32)];

  core.process(inps, outs);
  expect(outs[0]).toMatchSnapshot();
});

test('sparseq2 skip ahead', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 1,
    numOutputChannels: 1,
  });

  // Graph
  let loop = (start, end, t) => el.add(start, el.mod(t, el.sub(end, start)));

  core.render(
    el.sparseq2({seq: [
      { time: 512 * 10 + 0, value: 1 },
      { time: 512 * 10 + 4, value: 2 },
      { time: 512 * 10 + 8, value: 3 },
      { time: 512 * 10 + 12, value: 4 },
    ]}, el.in({channel: 0}))
  );

  // Ten blocks of data
  let inps = [new Float32Array(512 * 10)];
  let outs = [new Float32Array(512 * 10)];

  // Get past the fade-in
  core.process(inps, outs);

  // Push another small block. We should see the sequence skip ahead
  // to a future value
  inps = [Float32Array.from([
    5120, 5120, 5120, 5120, 5120, 5120, 5120, 5120,
    5128, 5128, 5128, 5128, 5128, 5128, 5128, 5128,
  ])];

  outs = [new Float32Array(16)];

  core.process(inps, outs);
  expect(outs[0]).toMatchSnapshot();
});
