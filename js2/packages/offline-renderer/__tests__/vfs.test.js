import OfflineRenderer from '../index';
import { el } from '@elemaudio/core';


test('vfs sample', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 1,
    numOutputChannels: 1,
    virtualFileSystem: {
      '/v/increment': Float32Array.from([1, 2, 3, 4, 5]),
    },
  });

  // Graph
  core.render(el.table({path: '/v/increment'}, el.in({channel: 0})));

  // Ten blocks of data
  let inps = [new Float32Array(512 * 10)];
  let outs = [new Float32Array(512 * 10)];

  // Get past the fade-in
  core.process(inps, outs);

  // Now we use a calculated input to trigger the sample
  inps = [Float32Array.from([0, 0.25, 0.5, 0.75, 1])];
  outs = [new Float32Array(inps[0].length)];

  // Drive our sparseq
  core.process(inps, outs);

  expect(outs[0]).toMatchSnapshot();
});
