import OfflineRenderer from "..";
import { el } from "@elemaudio/core";

test("mc table", async function () {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 0,
    numOutputChannels: 1,
    virtualFileSystem: {
      "/v/ones": Float32Array.from([1, 1, 1]),
      "/v/stereo": [
        Float32Array.from([27, 27, 27]),
        Float32Array.from([15, 15, 15]),
      ],
    },
  });

  // Graph
  await core.render(
    el.add(...el.mc.table({ path: "/v/stereo", channels: 2 }, 0)),
  );

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

  // Let's try a graph unpacking more channels than the resource has
  await core.render(
    el.add(...el.mc.table({ path: "/v/stereo", channels: 3 }, 0)),
  );

  // Get past the fade-in
  for (let i = 0; i < 100; ++i) {
    core.process(inps, outs);
  }

  // Process another small block
  core.process(inps, outs);
  expect(outs[0]).toMatchSnapshot();

  // And again unpacking fewer channels
  await core.render(
    el.add(...el.mc.table({ path: "/v/stereo", channels: 1 }, 0)),
  );

  // Get past the fade-in
  for (let i = 0; i < 100; ++i) {
    core.process(inps, outs);
  }

  // Process another small block
  core.process(inps, outs);
  expect(outs[0]).toMatchSnapshot();

  // Now here we expect that the first graph has been totally nudged out
  // of relevance, except for the nodes that remain shared which are the table
  // and the const 0. If we gc then, we should remove that add() and root() from
  // the original graph.
  expect(await core.gc()).toEqual([1611541315, 1811703364]);

  // But now the act of removing that add node should have pruned the outlet connections
  // list on the table node. We'll test that original graph again to ensure that after
  // rebuilding that connection everything still looks good.
  await core.render(
    el.add(...el.mc.table({ path: "/v/stereo", channels: 2 }, 0)),
  );

  // Get past the fade-in
  for (let i = 0; i < 100; ++i) {
    core.process(inps, outs);
  }

  // Process another small block
  core.process(inps, outs);
  expect(outs[0]).toMatchSnapshot();
});

test("mc sampleseq", async function () {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 0,
    numOutputChannels: 1,
    virtualFileSystem: {
      "/v/stereo": [
        Float32Array.from(Array.from({ length: 128 }).fill(27)),
        Float32Array.from(Array.from({ length: 128 }).fill(15)),
      ],
    },
  });

  let [time, setTimeProps] = core.createRef("const", { value: 0 }, []);

  core.render(
    el.add(
      ...el.mc.sampleseq(
        {
          path: "/v/stereo",
          channels: 2,
          duration: 128,
          seq: [
            { time: 0, value: 0 },
            { time: 128, value: 1 },
            { time: 256, value: 0 },
            { time: 512, value: 1 },
          ],
        },
        time,
      ),
    ),
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
  setTimeProps({ value: 520 });

  // Spin for a few blocks and we should see the gain fade resolve and
  // emit the constant sum of the two channels
  for (let i = 0; i < 10; ++i) {
    core.process(inps, outs);
  }

  expect(outs[0]).toMatchSnapshot();
});

test("mc capture", async function () {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 0,
    numOutputChannels: 4,
    blockSize: 32,
  });

  let [gate, setGateProps] = core.createRef("const", { value: 0 }, []);

  core.render(
    ...el.mc.capture({ name: "test", channels: 4 }, gate, 1, 2, 3, 4),
  );

  // Ten blocks of data to get past the root node fade
  let inps = [];
  let outs = [
    new Float32Array(32),
    new Float32Array(32),
    new Float32Array(32),
    new Float32Array(32),
  ];

  // Get past the fade-in
  for (let i = 0; i < 1000; ++i) {
    core.process(inps, outs);
  }

  let eventCallback = jest.fn();
  core.on("mc.capture", eventCallback);

  setGateProps({ value: 1 });
  core.process(inps, outs);
  expect(outs).toMatchSnapshot();

  setGateProps({ value: 0 });
  core.process(inps, outs);

  expect(eventCallback.mock.calls).toHaveLength(1);
  let args = eventCallback.mock.calls[0];
  let evt = args[0];

  expect(evt.data).toHaveLength(4);
  expect(evt.source).toBe("test");

  for (let i = 0; i < 4; ++i) {
    expect(evt.data[i]).toHaveLength(32);

    for (let j = 0; j < 32; ++j) {
      expect(evt.data[i][j]).toBe(i + 1);
    }
  }
});

test("mc.sample", async function () {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 1,
    numOutputChannels: 1,
    blockSize: 32,
    virtualFileSystem: {
      "/v/stereo": [
        Float32Array.from(Array.from({ length: 512 }).fill(27)),
        Float32Array.from(Array.from({ length: 512 }).fill(15)),
      ],
    },
  });

  let [gate, setGateProps] = core.createRef("const", { value: 0 }, []);

  await core.render(
    el.add(
      ...el.mc.sample(
        {
          path: "/v/stereo",
          channels: 2,
        },
        gate,
      ),
    ),
  );

  let inps = [new Float32Array(32)];
  let outs = [new Float32Array(32)];

  // Get past the fade-in
  for (let i = 0; i < 1000; ++i) {
    core.process(inps, outs);
  }

  // Now we expect to see zeros because we haven't triggered the sample
  core.process(inps, outs);
  expect(outs).toMatchObject([new Float32Array(32)]);

  // Trigger and we should see sample playback
  await setGateProps({ value: 1 });

  // Gain fade on sample boundary
  for (let i = 0; i < 5; ++i) {
    core.process(inps, outs);

    for (let j = 1; j < 32; ++j) {
      expect(outs[0][j]).toBeGreaterThanOrEqual(0);
      expect(outs[0][j]).toBeLessThan(42);
      expect(outs[0][j]).toBeGreaterThan(outs[0][j - 1]);
    }
  }

  // Gain fade resolves on the next block
  core.process(inps, outs);

  // Then in the subsequent block we should see constant 42s
  core.process(inps, outs);
  expect(outs).toMatchObject([Float32Array.from({ length: 32 }, () => 42)]);

  // Test for triggers mid-block
  await core.render(
    el.add(
      ...el.mc.sample(
        {
          path: "/v/stereo",
          channels: 2,
          mode: "gate",
        },
        el.in({ channel: 0 }),
      ),
    ),
  );

  // Get past the fade-in
  for (let i = 0; i < 1000; ++i) {
    core.process(inps, outs);
  }

  // Now we send a very intentional trigger
  for (let i = 8; i < 16; ++i) {
    inps[0][i] = 1.0;
  }

  core.process(inps, outs);

  // From 0 to 8 we expect silence
  expect(outs[0].slice(0, 8)).toMatchObject(new Float32Array(8));

  // From 8 to 16 we expect to see the gain ramp up
  for (let i = 8; i < 16; ++i) {
    expect(outs[0][i]).toBeGreaterThanOrEqual(0);
    expect(outs[0][i]).toBeLessThan(42);
    expect(outs[0][i]).toBeGreaterThanOrEqual(outs[0][i - 1]);
  }

  // Then we expect to see the gain ramp down
  for (let i = 17; i < 24; ++i) {
    expect(outs[0][i]).toBeLessThanOrEqual(outs[0][i - 1]);
  }
});

test("mc.sample again", async function () {
  let core = new OfflineRenderer();

  await core.initialize({
    numInputChannels: 0,
    numOutputChannels: 1,
    blockSize: 32,
    virtualFileSystem: {
      "/v/stereo": [
        Float32Array.from([1, 2, 3, 4, 5, 6, 7, 8]),
        Float32Array.from([1, 2, 3, 4, 5, 6, 7, 8]),
      ],
    },
  });

  await core.render(
    el.add(
      ...el.mc.sample(
        {
          path: "/v/stereo",
          channels: 2,
          mode: "loop",
          key: "test",
        },
        1,
      ),
    ),
  );

  let inps = [];
  let outs = [new Float32Array(32)];

  // Fade in
  for (let i = 0; i < 1000; ++i) {
    core.process(inps, outs);
  }

  expect(outs[0]).toMatchSnapshot();

  await core.render(
    el.add(
      ...el.mc.sample(
        {
          path: "/v/stereo",
          channels: 2,
          mode: "loop",
          key: "test",
          playbackRate: 2.0,
        },
        1,
      ),
    ),
  );

  core.process(inps, outs);
  expect(outs[0]).toMatchSnapshot();

  await core.render(
    el.add(
      ...el.mc.sample(
        {
          path: "/v/stereo",
          channels: 2,
          mode: "loop",
          // We need a new trigger for the start/stop offset to take hold
          key: "test2",
          playbackRate: 2.0,
          startOffset: 1,
          stopOffset: 1,
        },
        1,
      ),
    ),
  );

  // Fade in
  for (let i = 0; i < 1000; ++i) {
    core.process(inps, outs);
  }

  expect(outs[0]).toMatchSnapshot();
});
