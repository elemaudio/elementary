import WebRenderer from '..';
import { el } from '@elemaudio/core';
import { expect, test } from 'vitest';


const audioContext = (typeof window !== 'undefined') && new (window.AudioContext || window.webkitAudioContext)();


test('can mount sampleseq2', async function() {
  const core = new WebRenderer();
  const node = await core.initialize(audioContext, {
    numberOfInputs: 0,
    numberOfOutputs: 1,
    outputChannelCount: [2],
  });

  node.connect(audioContext.destination);
  expect(await core.render(el.sampleseq2({}, el.time()))).toMatchObject({
    nodesAdded: 3,
  });
});

test('can mount sampleseq', async function() {
  const core = new WebRenderer();
  const node = await core.initialize(audioContext, {
    numberOfInputs: 0,
    numberOfOutputs: 1,
    outputChannelCount: [2],
  });

  node.connect(audioContext.destination);
  expect(await core.render(el.sampleseq({}, el.time()))).toMatchObject({
    nodesAdded: 3,
  });
});
