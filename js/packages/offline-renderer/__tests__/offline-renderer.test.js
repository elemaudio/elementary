import OfflineRenderer from '../index';
import { createNode, el } from '@elemaudio/core';


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

test('child limit', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 0,
    numOutputChannels: 1,
  });

  // Ten blocks of data
  let inps = [];
  let outs = [new Float32Array(512 * 10)];

  core.render(createNode("add", {}, Array.from({length: 100}).fill(1)));
  core.process(inps, outs);

  // Once this settles, we should see 100s everywhere
  expect(outs[0].slice(512 * 8, 512 * 8 + 32)).toMatchSnapshot();
});

test('render stats', async function() {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 0,
    numOutputChannels: 1,
  });

  // Render a graph and get some valid stats back
  let stats = await core.render(el.mul(2, 3));

  expect(stats).toMatchObject({
    edgesAdded: 3,
    nodesAdded: 4,
    propsWritten: 3,
  });

  // Render with an invalid property and get a failure, rejecting the
  // promise returned from core.render
  await expect(core.render(el.const({value: 'hi'}))).rejects.toMatchObject({
    success: false,
  });
});
