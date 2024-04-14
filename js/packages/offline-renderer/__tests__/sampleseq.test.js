import OfflineRenderer from '../index';
import { el } from '@elemaudio/core';


test('sampleseq basics', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 0,
    numOutputChannels: 1,
    blockSize: 32,
    virtualFileSystem: {
      '/v/ones': Float32Array.from(Array.from({length: 128}).fill(1)),
    },
  });

  // Graph
  let [time, setTimeProps] = core.createRef("const", {value: 0}, []);

  core.render(
    el.sampleseq({
      path: '/v/ones',
      duration: 128,
      seq: [
        { time: 0, value: 0 },
        { time: 128, value: 1 },
        { time: 256, value: 0 },
        { time: 512, value: 1 },
      ]
    }, time)
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

  // Jump to time 129 and we should see 1s fade in
  setTimeProps({value: 129});
  core.process(inps, outs);

  for (let i = 1; i < outs[0].length; ++i) {
    expect(outs[0][i]).toBeGreaterThanOrEqual(0);
    expect(outs[0][i]).toBeLessThan(1);
    expect(outs[0][i]).toBeGreaterThan(outs[0][i - 1]);
  }

  // Spin for a few blocks, incrementing time, and we should see
  // the fade resolve and emit constant ones
  //
  // If we don't increment time, the sampleseq node will repeatedly
  // see an input time different from what it expects, and continue
  // re-allocating the readers, so the fade that we're observing would
  // lock in between 0 and 1
  for (let i = 0; i < 2; ++i) {
    setTimeProps({value: 129 + (i + 1) * 32})
    core.process(inps, outs);
  }

  expect(outs[0]).toMatchSnapshot();

  // Jump to time and we should see 1s fade out
  setTimeProps({value: 64});
  core.process(inps, outs);

  for (let i = 1; i < outs[0].length; ++i) {
    expect(outs[0][i]).toBeGreaterThanOrEqual(0);
    expect(outs[0][i]).toBeLessThan(1);
    expect(outs[0][i]).toBeLessThan(outs[0][i - 1]);
  }

  // Spin for a few blocks and we should see the fade resolve and
  // emit constant zeros
  for (let i = 0; i < 10; ++i) {
    core.process(inps, outs);
  }

  expect(outs[0]).toMatchSnapshot();

  // Jump one more time
  setTimeProps({value: 520});
  core.process(inps, outs);

  for (let i = 0; i < outs[0].length; ++i) {
    expect(outs[0][i]).toBeGreaterThanOrEqual(0);
    expect(outs[0][i]).toBeLessThan(1);
  }

  // Spin for a few blocks and we should see the fade resolve and
  // emit constant ones
  for (let i = 0; i < 2; ++i) {
    setTimeProps({value: 520 + (i + 1) * 32})
    core.process(inps, outs);
  }

  expect(outs[0]).toMatchSnapshot();
});
