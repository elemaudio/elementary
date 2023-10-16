import OfflineRenderer from '../index';
import { el } from '@elemaudio/core';


test('refs', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 0,
    numOutputChannels: 1,
  });

  // Graph
  let [cutoffFreq, setCutoffFreq] = core.createRef("const", {value: 500}, []);
  core.render(cutoffFreq);

  // Ten blocks of data
  let inps = [];
  let outs = [new Float32Array(512 * 10)];

  // Get past the fade-in
  core.process(inps, outs);

  // Push another small block. We should see an incrementing output beginning
  // at 512 * 10 because of the blocks we've already pushed through
  inps = [];
  outs = [new Float32Array(8)];

  core.process(inps, outs);
  expect(outs[0]).toEqual(Float32Array.from([500, 500, 500, 500, 500, 500, 500, 500]));

  // Now we can set the ref without doing a render pass
  setCutoffFreq({value: 800});

  core.process(inps, outs);
  expect(outs[0]).toEqual(Float32Array.from([800, 800, 800, 800, 800, 800, 800, 800]));
});
