import WebRenderer from '..';
import { el } from '@elemaudio/core';
import { expect, test } from 'vitest';


const audioContext = (typeof window !== 'undefined') && new (window.AudioContext || window.webkitAudioContext)();


test('vfs should show registered entries', async function() {
  const core = new WebRenderer();
  const node = await core.initialize(audioContext, {
    numberOfInputs: 0,
    numberOfOutputs: 1,
    outputChannelCount: [2],
    processorOptions: {
      virtualFileSystem: {
        'test': Float32Array.from([1, 2, 3, 4, 5]),
      },
    },
  });

  node.connect(audioContext.destination);
  expect(await core.listVirtualFileSystem()).toEqual(['test']);

  // We haven't rendered anything that holds a reference to the test entry
  await core.pruneVirtualFileSystem();
  expect(await core.listVirtualFileSystem()).toEqual([]);

  // Now we put something back in
  await core.updateVirtualFileSystem({
    'test2': Float32Array.from([2, 3, 4, 5]),
  });

  // After we render something referencing the test2 entry, prune shouldn't touch it
  expect(await core.listVirtualFileSystem()).toEqual(['test2']);
  expect(await core.render(el.table({key: 'a', path: 'test2'}, 0.5))).toMatchObject({
    nodesAdded: 3,
    edgesAdded: 2,
    propsWritten: 4,
  });

  await core.pruneVirtualFileSystem();
  expect(await core.listVirtualFileSystem()).toEqual(['test2']);
});
