import OfflineRenderer from '../index';
import { el } from '@elemaudio/core';


test('the basics', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 1,
    numOutputChannels: 1,
  });

  // Ten blocks of data
  let inps = [new Float32Array(512 * 10)];
  let outs = [new Float32Array(512 * 10)];

  core.render(el.mul(2, 3));
  core.process(inps, outs);

  // After a couple blocks, we expect to see the output data
  // has resolved to a bunch of 6s
  expect(outs[0].slice(512 * 8, 512 * 9)).toMatchSnapshot();
});

test('events', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 1,
    numOutputChannels: 1,
  });

  // Ten blocks of data
  let inps = [new Float32Array(512 * 10)];
  let outs = [new Float32Array(512 * 10)];
  let events = [];

  core.on('meter', function(e) {
    events.push(e);
  });

  core.render(el.meter(el.mul(2, 3)));
  core.process(inps, outs);

  expect(events).toMatchSnapshot();
});

test('switch and switch back', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 0,
    numOutputChannels: 1,
  });

  // Ten blocks of data
  let inps = [];
  let outs = [new Float32Array(512 * 10)];

  core.render(el.mul(2, 3));
  core.process(inps, outs);

  // After a couple blocks, we render again with a new graph
  core.render(el.mul(3, 4));
  core.process(inps, outs);

  // Once this settles, we should see the new output
  expect(outs[0].slice(512 * 8, 512 * 8 + 32)).toMatchSnapshot();

  // Finally we render a third time back to the original graph
  // to show that the switch back correctly restored the prior roots
  core.render(el.mul(2, 3));
  core.process(inps, outs);

  expect(outs[0].slice(512 * 8, 512 * 8 + 32)).toMatchSnapshot();
});
