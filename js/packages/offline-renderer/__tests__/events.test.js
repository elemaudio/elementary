import OfflineRenderer from '../index';
import { el } from '@elemaudio/core';


test('event propagation', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 0,
    numOutputChannels: 1,
    blockSize: 512,
  });

  // Event handling
  let callback = jest.fn();
  core.on('meter', callback);

  // Graph
  core.render(el.meter(0));

  // Processing
  let inps = [];
  let outs = [new Float32Array(512 * 4)];

  core.process(inps, outs);

  // We just pushed four blocks of data through, we would expect the meter
  // node to fire on each block, thus we should see 4 calls to the meter
  // callback
  expect(callback.mock.calls).toHaveLength(4);
  expect(callback.mock.calls.map(x => x[0])).toMatchSnapshot();

  callback.mockClear();

  // New graph
  core.render(el.meter(1));

  // Processing
  core.process(inps, outs);

  // We just pushed four more blocks of data, we would expect the same
  // as above but this time the meter should be reporting the new value 1
  expect(callback.mock.calls).toHaveLength(4);
  expect(callback.mock.calls.map(x => x[0])).toMatchSnapshot();
});
