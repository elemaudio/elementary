import WebRenderer from '../dist/index.js';
import { el } from '@elemaudio/core';


const audioContext = (typeof window !== 'undefined') && new (window.AudioContext || window.webkitAudioContext)();


describe('Standard Library', function () {
  it('should have sparseq2', async function() {
    const core = new WebRenderer();
    const node = await core.initialize(audioContext, {
      numberOfInputs: 0,
      numberOfOutputs: 1,
      outputChannelCount: [2],
    });

    core.on('error', (e) => {
      throw e;
    });

    node.connect(audioContext.destination);
    let stats = core.render(el.sparseq2({seq: [{ value: 0, time: 0 }]}, 1));

    chai.assert.equal(stats.nodesAdded, 3); // root, sparseq, const
    chai.assert.equal(stats.edgesAdded, 2);
    chai.assert.equal(stats.propsWritten, 3); // root channel, sparseq seq, const value
  });
});

