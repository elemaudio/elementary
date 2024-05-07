import OfflineRenderer from '../index';
import { el, createNode } from '@elemaudio/core';


test('mc table', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 0,
    numOutputChannels: 1,
    virtualFileSystem: {
      '/v/ones': Float32Array.from([1, 1, 1]),
      '/v/stereo': [
        Float32Array.from([27, 27, 27]),
        Float32Array.from([15, 15, 15]),
      ],
    },
  });

  // Graph
  let mcTable = createNode("mc.table", {path: '/v/stereo'}, [0]);
  let graph = el.add(...el.unpack(mcTable, 2));
  await core.render(graph)

  // Ten blocks of data
  let inps = [];
  let outs = [new Float32Array(512 * 10)];

  // Get past the fade-in
  core.process(inps, outs);

  // Process another small block
  inps = [];
  outs = [new Float32Array(16)];

  core.process(inps, outs);

  // expect(outs[0]).toMatchSnapshot();
  console.log(outs[0]);
});

