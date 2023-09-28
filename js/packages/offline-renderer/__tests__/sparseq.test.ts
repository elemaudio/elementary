import OfflineRenderer from '../index';
import { el } from '@elemaudio/core';


test('sparseq basics', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 1,
    numOutputChannels: 1,
  });

  // Graph
  core.render(el.sparseq({seq: [
    { value: 1, tickTime: 0 },
    { value: 2, tickTime: 2 },
    { value: 3, tickTime: 4 },
    { value: 4, tickTime: 8 },
  ]}, el.in({channel: 0}), 0));

  // Ten blocks of data
  let inps = [new Float32Array(512 * 10)];
  let outs = [new Float32Array(512 * 10)];

  // Get past the fade-in
  core.process(inps, outs);

  // Now we use a very calculated input to step the sparseq
  inps = [Float32Array.from([
    0,
    1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0,
    1, 0, 1, 0, 1, 0, 1, 0,
  ])];
  outs = [new Float32Array(inps[0].length)];

  // Drive our sparseq
  core.process(inps, outs);

  expect(outs[0]).toMatchSnapshot();
});

test('sparseq loop', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 1,
    numOutputChannels: 1,
  });

  let seq = [
    { value: 1, tickTime: 0 },
    { value: 2, tickTime: 2 },
    { value: 3, tickTime: 4 },
    { value: 4, tickTime: 8 },
  ];

  // Graph
  core.render(el.sparseq({seq, loop: [2, 4]}, el.in({channel: 0}), 0));

  // Ten blocks of data
  let inps = [new Float32Array(512 * 10)];
  let outs = [new Float32Array(512 * 10)];

  // Get past the fade-in
  core.process(inps, outs);

  // Now we use a very calculated input to step the sparseq every other sample
  inps = [(new Float32Array(32)).map((x, i) => (i + 1) % 2)];
  outs = [new Float32Array(inps[0].length)];

  // Drive our sparseq
  core.process(inps, outs);

  expect(outs[0]).toMatchSnapshot();
});

test('sparseq loop on then off', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 1,
    numOutputChannels: 1,
  });

  let seq = [
    { value: 1, tickTime: 0 },
    { value: 2, tickTime: 2 },
    { value: 3, tickTime: 4 },
    { value: 4, tickTime: 8 },
  ];

  // Graph
  core.render(el.sparseq({key: 'test', seq, loop: [2, 4]}, el.in({channel: 0}), 0));

  // Ten blocks of data
  let inps = [new Float32Array(512 * 10)];
  let outs = [new Float32Array(512 * 10)];

  // Get past the fade-in
  core.process(inps, outs);

  // Now we use a very calculated input to step the sparseq every other sample
  inps = [(new Float32Array(32)).map((x, i) => (i + 1) % 2)];
  outs = [new Float32Array(inps[0].length)];

  // Drive our sparseq
  core.process(inps, outs);

  // Update our graph to disable the loop
  core.render(el.sparseq({key: 'test', seq, loop: false}, el.in({channel: 0}), 0));

  // Drive our sparseq again
  core.process(inps, outs);

  // We see that the tickTime has kept going, so upon disabling loop we're way past tickTime 8
  // and seeing a constant stream of 4s. I'm not 100% sold on this behavior, I could imagine
  // automatically overwriting tickTime with the adjusted time bounded within the loop so that
  // when looping is then disabled we don't jump to a new part in the sequence...
  expect(outs[0]).toMatchSnapshot();
});

test('sparseq no trigger on reset', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 1,
    numOutputChannels: 1,
  });

  let seq = [
    { value: 1, tickTime: 0 },
    { value: 2, tickTime: 2 },
    { value: 3, tickTime: 4 },
    { value: 4, tickTime: 8 },
  ];

  // Graph
  core.render(el.sparseq({seq, loop: false}, el.in({channel: 0}), el.const({key: 'reset', value: 0})));

  // Ten blocks of data
  let inps = [new Float32Array(512 * 10)];
  let outs = [new Float32Array(512 * 10)];

  // Get past the fade-in
  core.process(inps, outs);

  // Now we use a very calculated input to step the sparseq every other sample
  inps = [(new Float32Array(8)).map((x, i) => (i + 1) % 2)];
  outs = [new Float32Array(inps[0].length)];

  // Drive our sparseq
  core.process(inps, outs);

  // Update our graph to fire the reset signal
  core.render(el.sparseq({seq, loop: false}, el.in({channel: 0}), el.const({key: 'reset', value: 1})));

  // Now a bunch of zeros, not firing the trigger train, just the reset
  inps = [new Float32Array(inps[0].length)];
  outs = [new Float32Array(inps[0].length)];

  // Drive our sparseq again
  core.process(inps, outs);

  // We should see that the value held is where we last triggered (2, we didn't make it all the way
  // to tickTime 4)
  expect(outs[0]).toMatchSnapshot();

  // Now we fire the trigger again
  inps = [Float32Array.from([1, 0, 0, 0])];
  outs = [new Float32Array(inps[0].length)];

  // And we should see the reset take effect
  expect(outs[0]).toMatchSnapshot();
});

test('sparseq interpolation', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 1,
    numOutputChannels: 1,
  });

  let seq = [
    { value: 1, tickTime: 0 },
    { value: 2, tickTime: 2 },
    { value: 3, tickTime: 4 },
    { value: 4, tickTime: 8 },
  ];

  // Graph
  core.render(el.sparseq({seq, interpolate: 1}, el.in({channel: 0}), 0));

  // Ten blocks of silent data
  let inps = [new Float32Array(512 * 10)];
  let outs = [new Float32Array(512 * 10)];

  // Get past the root node fade-in
  core.process(inps, outs);

  // Now we use a very calculated input to step the sparseq every other sample
  inps = [(new Float32Array(24)).map((x, i) => (i + 1) % 2)];
  outs = [new Float32Array(inps[0].length)];

  // Drive our sparseq
  core.process(inps, outs);

  // We expect to see linearly interpolated values (first order hold) on
  // ticks 1, 3, 5, 6, 7, then a held value of 4 thereafter. Note that each
  // tick here constitutes two samples, and we are not providing the sub-tick
  // interval parameter so this matches our expectation.
  expect(outs[0]).toMatchSnapshot();
});

test('sparseq interpolation with loop', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 1,
    numOutputChannels: 1,
  });

  let seq = [
    { value: 1, tickTime: 0 },
    { value: 2, tickTime: 2 },
    { value: 3, tickTime: 4 },
    { value: 4, tickTime: 8 },
  ];

  // Graph
  core.render(el.sparseq({seq, interpolate: 1, loop: [1, 3]}, el.in({channel: 0}), 0));

  // Ten blocks of silent data
  let inps = [new Float32Array(512 * 10)];
  let outs = [new Float32Array(512 * 10)];

  // Get past the root node fade-in
  core.process(inps, outs);

  // Now we use a very calculated input to step the sparseq every other sample
  inps = [(new Float32Array(24)).map((x, i) => (i + 1) % 2)];
  outs = [new Float32Array(inps[0].length)];

  // Drive our sparseq
  core.process(inps, outs);

  // We expect to see linearly interpolated values (first order hold) between
  // 1 and 3 since we loop from tickTime [1, 3].
  expect(outs[0]).toMatchSnapshot();
});

test('sparseq sub-tick interpolation', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    sampleRate: 1000,
    numInputChannels: 1,
    numOutputChannels: 1,
  });

  let seq = [
    { value: 1, tickTime: 0 },
    { value: 2, tickTime: 2 },
    { value: 3, tickTime: 4 },
    { value: 4, tickTime: 8 },
  ];

  // Graph. We use a tickInterval of 0.002s (2ms) here because we've set the sampleRate above
  // to 1000Hz, thus each sample represents 1ms time. Our clock signal below will alternate between
  // 1 and 0 on each sample, thus a clock cycle is 2ms.
  core.render(el.sparseq({seq, interpolate: 1, tickInterval: 0.002}, el.in({channel: 0}), el.const({key: 'reset', value: 0})));

  // Ten blocks of silent data
  let inps = [new Float32Array(512 * 10)];
  let outs = [new Float32Array(512 * 10)];

  // Get past the root node fade-in
  core.process(inps, outs);

  // Now we use a very calculated input to step the sparseq every other sample
  inps = [(new Float32Array(24)).map((x, i) => (i + 1) % 2)];
  outs = [new Float32Array(inps[0].length)];

  // Drive our sparseq
  core.process(inps, outs);

  // We expect to see linearly interpolated values here with sub-tick resolution
  expect(outs[0]).toMatchSnapshot();

  // Now we stop the clock and expect to see that the sub-tick interpolation doesn't
  // exceed the length of a single clock cycle. Otherwise, stopping the clock would
  // be a meaningless control change.
  core.render(el.sparseq({seq, interpolate: 1, tickInterval: 0.002}, el.in({channel: 0}), el.const({key: 'reset', value: 1})));

  inps = [(new Float32Array(24)).map((x, i) => (i > 7) ? 0 : (i + 1) % 2)];
  outs = [new Float32Array(inps[0].length)];

  core.process(inps, outs);

  expect(outs[0]).toMatchSnapshot();
});

test('sparseq sub-tick interpolation with loop', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    sampleRate: 1000,
    numInputChannels: 1,
    numOutputChannels: 1,
  });

  let seq = [
    { value: 0, tickTime: 0 },
    { value: 0, tickTime: 4 },
    { value: 1, tickTime: 8 },
  ];

  // Graph. We use a tickInterval of 0.002s (2ms) here because we've set the sampleRate above
  // to 1000Hz, thus each sample represents 1ms time. Our clock signal below will alternate between
  // 1 and 0 on each sample, thus a clock cycle is 2ms.
  core.render(el.sparseq({seq, interpolate: 1, tickInterval: 0.002, loop: [4, 8], offset: 4}, el.in({channel: 0}), 0));

  // Ten blocks of silent data
  let inps = [new Float32Array(512 * 10)];
  let outs = [new Float32Array(512 * 10)];

  // Get past the root node fade-in
  core.process(inps, outs);

  // Now we use a very calculated input to step the sparseq every other sample
  inps = [(new Float32Array(24)).map((x, i) => (i + 1) % 2)];
  outs = [new Float32Array(inps[0].length)];

  // Drive our sparseq
  core.process(inps, outs);

  // We expect to see linearly interpolated values here with sub-tick resolution
  // and repeated looping
  expect(outs[0]).toMatchSnapshot();
});

test('sparseq sub-tick interpolation with loop higher res', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    sampleRate: 2000,
    numInputChannels: 1,
    numOutputChannels: 1,
  });

  let seq = [
    { value: 0, tickTime: 0 },
    { value: 0, tickTime: 4 },
    { value: 1, tickTime: 8 },
  ];

  // Graph.
  core.render(el.sparseq({seq, interpolate: 1, tickInterval: 0.01, loop: [4, 8], offset: 4}, el.train(100), 0));

  // Ten blocks of silent data
  let inps = [new Float32Array(512 * 10)];
  let outs = [new Float32Array(512 * 10)];

  // Get past the root node fade-in
  core.process(inps, outs);

  // One block of silent data
  inps = [new Float32Array(512)];
  outs = [new Float32Array(512)];

  // Drive our sparseq
  core.process(inps, outs);

  // We expect to see linearly interpolated values here with sub-tick resolution
  // and repeated looping
  for (let i = 0; i < outs[0].length; ++i) {
    expect(outs[0][i]).toBeGreaterThanOrEqual(0);
    expect(outs[0][i]).toBeLessThanOrEqual(1);
  }

  expect(outs[0]).toMatchSnapshot();
});

test('sparseq loop follow', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 1,
    numOutputChannels: 1,
  });

  let seq = [
    { value: 1, tickTime: 0 },
    { value: 2, tickTime: 1 },
    { value: 3, tickTime: 2 },
    { value: 4, tickTime: 3 },
  ];

  // Graph
  core.render(el.sparseq({key: 'test', seq, loop: [1, 3]}, el.in({channel: 0}), 0));

  // Ten blocks of data
  let inps = [new Float32Array(512 * 10)];
  let outs = [new Float32Array(512 * 10)];

  // Get past the fade-in
  core.process(inps, outs);

  // Now we use a very calculated input to step the sparseq every other sample
  inps = [(new Float32Array(32)).map((x, i) => (i + 1) % 2)];
  outs = [new Float32Array(inps[0].length)];

  // Drive our sparseq
  core.process(inps, outs);
  expect(outs[0]).toMatchSnapshot();

  // Now we move the loop points with follow: true
  core.render(el.sparseq({key: 'test', seq, loop: [0, 2], follow: true}, el.in({channel: 0}), 0));

  // And process some more
  inps = [(new Float32Array(32)).map((x, i) => (i + 1) % 2)];
  outs = [new Float32Array(inps[0].length)];

  core.process(inps, outs);
  expect(outs[0]).toMatchSnapshot();
});

