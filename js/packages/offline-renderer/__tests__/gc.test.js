import OfflineRenderer from '../index';
import { el } from '@elemaudio/core';


test('gc basics', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 0,
    numOutputChannels: 1,
  });

  // Ten blocks of data
  let inps = [];
  let outs = [new Float32Array(512 * 10)];

  await core.render(el.mul(2, 3));
  core.process(inps, outs);

  // After the first render we should see 2 consts, a mul, and a root,
  // and the gc should clean up none of them because they're being used.
  let earlyMapKeys = Array.from(core._renderer._delegate.nodeMap.keys());
  expect(await core.gc()).toEqual([]);

  // Now if we render and process twice, giving the earliest root enough
  // time to fade out and become dormant, we should see the gc pick up the
  // dormant nodes
  await core.render(el.mul(4, 5));
  core.process(inps, outs);
  expect(await core.gc()).toEqual([]);

  await core.render(el.mul(6, 7));
  core.process(inps, outs);

  let pruned = await core.gc();
  expect(pruned.sort()).toEqual(earlyMapKeys.sort());

  // We also expect that the renderer's nodeMap no longer contains the
  // pruned keys
  earlyMapKeys.forEach((k) => {
    expect(core._renderer._delegate.nodeMap.has(k)).toBe(false);
  });
});

