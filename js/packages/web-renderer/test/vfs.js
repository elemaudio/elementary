import WebRenderer from '../dist/index.js';
import { el } from '@elemaudio/core';


const audioContext = (typeof window !== 'undefined') && new (window.AudioContext || window.webkitAudioContext)();


describe('Virtual File System', function () {
  describe('list', function () {
    it('should show registered entries', async function() {
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
      chai.assert.deepEqual(await core.listVirtualFileSystem(), ['test']);

      // We haven't rendered anything that holds a reference to the test entry
      core.pruneVirtualFileSystem();
      chai.assert.deepEqual(await core.listVirtualFileSystem(), []);

      // Now we put something back in
      core.updateVirtualFileSystem({
        'test2': Float32Array.from([2, 3, 4, 5]),
      });

      // After we render something referencing the test2 entry, prune shouldn't touch it
      chai.assert.deepEqual(await core.listVirtualFileSystem(), ['test2']);
      chai.assert.include(core.render(el.table({key: 'a', path: 'test2'}, 0.5)), {
        nodesAdded: 3,
        edgesAdded: 2,
        propsWritten: 4,
      });;

      core.pruneVirtualFileSystem();
      chai.assert.deepEqual(await core.listVirtualFileSystem(), ['test2']);

      // Now we'll put test3 in and remove our reference to test2
      //
      // TODO: It's hard to actually test this in an automated fashion in the web context
      // because we need the audio context to avoid a suspended state, which it defaults to
      // before any user interaction. We need samples flowing through the table node in order
      // for it to take/drop its references to resources. Commenting this one out for now.
      //
      // core.updateVirtualFileSystem({
      //   'test3': Float32Array.from([2, 3, 4]),
      // });

      // chai.assert.include(core.render(el.table({key: 'a', path: 'test3'}, 0.5)), {
      //   nodesAdded: 0,
      //   edgesAdded: 0,
      //   propsWritten: 1,
      // });;

      // core.pruneVirtualFileSystem();
      // chai.assert.deepEqual(await core.listVirtualFileSystem(), ['test3']);

      node.disconnect();
    });
  });
});

