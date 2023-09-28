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
