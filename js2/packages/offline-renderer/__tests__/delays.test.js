import OfflineRenderer from '../index';
import { el } from '@elemaudio/core';


test('delay basics', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 1,
    numOutputChannels: 1,
  });

  // Graph
  core.render(el.delay({size: 10}, 0.5, 0, el.in({channel: 0})));

  // Ten blocks of data
  let inps = [new Float32Array(512 * 10)];
  let outs = [new Float32Array(512 * 10)];

  // Get past the fade-in
  core.process(inps, outs);

  inps = [Float32Array.from([1, 2, 3, 4])];
  outs = [new Float32Array(inps[0].length)];

  // Drive our delay
  core.process(inps, outs);

  expect(outs[0]).toMatchSnapshot();
});

test('delay with zero delay time', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 1,
    numOutputChannels: 1,
  });

  // Graph
  core.render(el.delay({size: 10}, 0, 0, el.in({channel: 0})));

  // Ten blocks of data
  let inps = [new Float32Array(512 * 10)];
  let outs = [new Float32Array(512 * 10)];

  // Get past the fade-in
  core.process(inps, outs);

  inps = [Float32Array.from([1, 2, 3, 4])];
  outs = [new Float32Array(inps[0].length)];

  // Drive our delay
  core.process(inps, outs);

  // Expect input == output
  expect(outs[0]).toMatchObject(inps[0]);
});

test('sdelay basics', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 1,
    numOutputChannels: 1,
  });

  // Graph
  core.render(el.sdelay({size: 10}, el.in({channel: 0}), 0));

  // Ten blocks of data
  let inps = [new Float32Array(512 * 10)];
  let outs = [new Float32Array(512 * 10)];

  // Get past the fade-in
  core.process(inps, outs);

  // Now we use a very calculated input to step the sparseq
  inps = [Float32Array.from([
    1, 2, 3, 4, 4, 3, 2, 1,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
  ])];

  outs = [new Float32Array(inps[0].length)];

  // Drive our sdelay
  core.process(inps, outs);

  expect(outs[0]).toMatchSnapshot();
});
