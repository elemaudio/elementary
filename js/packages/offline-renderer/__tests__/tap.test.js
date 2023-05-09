import OfflineRenderer from '../index';
import { el } from '@elemaudio/core';


test('feedback taps', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 1,
    numOutputChannels: 1,
  });

  // Graph
  core.render(
    el.tapOut({name: 'test'}, el.add(
      el.tapIn({name: 'test'}),
      el.in({channel: 0}),
    )),
  );

  // Ten blocks of data
  let inps = [new Float32Array(512 * 10)];
  let outs = [new Float32Array(512 * 10)];

  // Get past the fade-in
  core.process(inps, outs);

  // Now we use a calculated input
  inps = [new Float32Array(512)];
  outs = [new Float32Array(512)];

  for (let i = 0; i < inps[0].length; ++i) {
    inps[0][i] = 1;
  }

  // Now on the first block, we should see just the ones pass through because
  // of the implicit block-size delay in the feedback tap structure
  core.process(inps, outs);
  expect(outs[0]).toMatchSnapshot();

  // Next, one block later, we should see a bunch of twos (summing the input 1s
  // with the feedback 1s)
  core.process(inps, outs);
  expect(outs[0]).toMatchSnapshot();

  // Finally, one block later, we should see a bunch of threes (summing the input 1s
  // with the feedback 2s)
  core.process(inps, outs);
  expect(outs[0]).toMatchSnapshot();
});
