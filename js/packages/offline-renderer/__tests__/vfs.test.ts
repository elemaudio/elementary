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

test('vfs list', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 1,
    numOutputChannels: 1,
    virtualFileSystem: {
      'one': Float32Array.from([1, 2, 3, 4, 5]),
      'two': Float32Array.from([2, 2, 3, 4, 5]),
    },
  });

  // Graph
  core.render(el.table({path: 'one'}, el.in({channel: 0})));

  expect(core.listVirtualFileSystem()).toEqual(expect.arrayContaining(['one', 'two']));
});

test('vfs prune', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 1,
    numOutputChannels: 1,
    virtualFileSystem: {
      'one': Float32Array.from([1, 2, 3, 4, 5]),
      'two': Float32Array.from([2, 2, 3, 4, 5]),
    },
  });

  // Graph
  core.render(el.table({key: 'a', path: 'one'}, el.in({channel: 0})));

  core.pruneVirtualFileSystem();
  expect(core.listVirtualFileSystem()).toEqual(['one']);

  // Add a new entry, update the graph to point to the new entry
  core.updateVirtualFileSystem({
    'three': Float32Array.from([3, 4, 5, 6]),
  });

  core.render(el.table({key: 'a', path: 'three'}, el.in({channel: 0})));

  // After the new render we have to process a block to get the table node
  // to take its new buffer resource
  let inps = [new Float32Array(512)];
  let outs = [new Float32Array(512)];
  core.process(inps, outs);

  // Then if we prune and list we should see 'one' has disappeared
  core.pruneVirtualFileSystem();
  expect(core.listVirtualFileSystem()).toEqual(['three']);
});
