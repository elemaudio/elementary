import OfflineRenderer from '../index';
import { el } from '@elemaudio/core';


test('time node', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 1,
    numOutputChannels: 1,
  });

  // Graph
  core.render(el.time());

  // Ten blocks of data
  let inps = [new Float32Array(512 * 10)];
  let outs = [new Float32Array(512 * 10)];

  // Get past the fade-in
  core.process(inps, outs);

  // Push another small block. We should see an incrementing output beginning
  // at 512 * 10 because of the blocks we've already pushed through
  inps = [new Float32Array(32)];
  outs = [new Float32Array(32)];

  core.process(inps, outs);
  expect(outs[0]).toMatchSnapshot();
});

test('setting time', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 0,
    numOutputChannels: 1,
  });

  // Graph
  core.render(el.time());

  // Ten blocks of data
  let inps = [];
  let outs = [new Float32Array(512 * 10)];

  // Get past the fade-in
  core.process(inps, outs);

  // Set current time, then push another small block
  outs = [new Float32Array(8)];

  core.setCurrentTime(50);
  core.process(inps, outs);
  expect(Array.from(outs[0])).toMatchObject([50, 51, 52, 53, 54, 55, 56, 57]);

  // Set current time, then push another small block
  outs = [new Float32Array(8)];

  core.setCurrentTimeMs(1000);
  core.process(inps, outs);
  expect(Array.from(outs[0])).toMatchObject([44100, 44101, 44102, 44103, 44104, 44105, 44106, 44107]);
});
