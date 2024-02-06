import WebRenderer from '..';
import { el } from '@elemaudio/core';
import { expect, test } from 'vitest';


const audioContext = (typeof window !== 'undefined') && new (window.AudioContext || window.webkitAudioContext)();


test('std lib should have sparseq2', async function() {
  const core = new WebRenderer();
  const node = await core.initialize(audioContext, {
    numberOfInputs: 0,
    numberOfOutputs: 1,
    outputChannelCount: [2],
  });

  node.connect(audioContext.destination);
  let stats = await core.render(el.sparseq2({seq: [{ value: 0, time: 0 }]}, 1));

  expect(stats.nodesAdded).toEqual(3); // root, sparseq, const
  expect(stats.edgesAdded).toEqual(2);
  expect(stats.propsWritten).toEqual(3); // root channel, sparseq seq, const value
});
