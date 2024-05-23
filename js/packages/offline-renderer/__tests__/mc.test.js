import OfflineRenderer from '../index';
import { el } from '@elemaudio/core';


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
  await core.render(el.add(...el.mc.table({path: '/v/stereo', channels: 2}, 0)));

  // Ten blocks of data
  let inps = [];
  let outs = [new Float32Array(512 * 10)];

  // Get past the fade-in
  core.process(inps, outs);

  // Process another small block
  inps = [];
  outs = [new Float32Array(16)];

  core.process(inps, outs);

  expect(outs[0]).toMatchSnapshot();
});

test('mc sampleseq', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 0,
    numOutputChannels: 1,
    virtualFileSystem: {
      '/v/stereo': [
        Float32Array.from(Array.from({length: 128}).fill(27)),
        Float32Array.from(Array.from({length: 128}).fill(15)),
      ],
    },
  });

  let [time, setTimeProps] = core.createRef("const", {value: 0}, []);

  core.render(
    el.add(...el.mc.sampleseq({
      path: '/v/stereo',
      channels: 2,
      duration: 128,
      seq: [
        { time: 0, value: 0 },
        { time: 128, value: 1 },
        { time: 256, value: 0 },
        { time: 512, value: 1 },
      ]
    }, time))
  );

  // Ten blocks of data to get past the root node fade
  let inps = [];
  let outs = [new Float32Array(10 * 512)];

  // Get past the fade-in
  core.process(inps, outs);

  // Now we expect to see zeros because we're fixed at time 0
  outs = [new Float32Array(32)];
  core.process(inps, outs);
  expect(outs[0]).toMatchSnapshot();

  // Jump in time
  setTimeProps({value: 520});

  // Spin for a few blocks and we should see the gain fade resolve and
  // emit the constant sum of the two channels
  for (let i = 0; i < 10; ++i) {
    core.process(inps, outs);
  }

  expect(outs[0]).toMatchSnapshot();
});
