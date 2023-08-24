import {
  createNode,
  renderWithDelegate,
  stepGarbageCollector,
  resolve,
  Delegate,
} from '..';


class TestRenderer {
  constructor() {
    this._lastBatch = null;
    this._delegate = new Delegate(44100, (batch) => {
      this._lastBatch = batch;
    });
  }

  render(...args) {
    renderWithDelegate(this._delegate, args.map(resolve));

    // Dummy stats
    return {};
  }

  getBatch() {
    return this._lastBatch;
  }

  getTerminalGeneration() {
    return this._delegate.getTerminalGeneration();
  }

  gc() {
    stepGarbageCollector(this._delegate);
  }
}

function sortInstructionBatch(x) {
  let copy = [...x];

  return copy.sort((a, b) => {
    if (a[0] === b[0]) {
      return a[1] < b[1] ? -1 : 1;
    }

    return a[0] < b[0] ? -1 : 1;
  });
}

test('the basics', function() {
  let tr = new TestRenderer();

  tr.render(
    createNode("sin", {}, [
      createNode("mul", {}, [
        createNode("const", {value: 2 * Math.PI}, []),
        createNode("phasor", {}, [
          createNode("const", {key: 'fq', value: 440}, [])
        ])
      ])
    ])
  );

  expect(sortInstructionBatch(tr.getBatch())).toMatchSnapshot();
});

test('numeric literals', function() {
  let tr = new TestRenderer();

  tr.render(
    createNode("sin", {}, [
      createNode("mul", {}, [
        2 * Math.PI,
        createNode("phasor", {}, [440])
      ])
    ])
  );

  expect(sortInstructionBatch(tr.getBatch())).toMatchSnapshot();
});

test('distinguish by props', function() {
  let tr = new TestRenderer();

  const renderVoice = (path, seq) => {
    return (
      createNode("sample", {path}, [
        createNode("seq", {seq}, [
          createNode("le", {}, [
            createNode("phasor", {}, [
              createNode("const", {value: 2}, [])
            ]),
            createNode("const", {value: 0.5}, [])
          ]),
        ]),
      ])
    );
  };

  tr.render(
    renderVoice('test/path.wav', [0, 0, 1]),
    renderVoice('test/path.wav', [0, 1, 0]),
  );

  // Here we haven't specified any keys, so the renderer should do a lot of
  // structural sharing. However, because we're rendering a different sequence,
  // we expect to see the pulse train (starting at the "le" node) shared, but
  // then see two different root nodes, two different sample nodes, and two
  // different seq nodes.
  expect(sortInstructionBatch(tr.getBatch())).toMatchSnapshot();
});

test('multi-channel basics', function() {
  const tr = new TestRenderer();

  const monoProcess = (
    createNode("sin", {}, [
      createNode("mul", {}, [
        createNode("const", {value: 2 * Math.PI}, []),
        createNode("phasor", {}, [
          createNode("const", {key: 'fq', value: 440}, [])
        ])
      ])
    ])
  );

  // Run the same thing in the left and the right channel
  tr.render(monoProcess, monoProcess);

  // We should see structural sharing except for the root nodes
  expect(sortInstructionBatch(tr.getBatch())).toMatchSnapshot();
});

test('simple sharing', function() {
  let tr = new TestRenderer();

  tr.render(
    createNode("sin", {}, [
      createNode("mul", {}, [
        createNode("const", {value: 2 * Math.PI}, []),
        createNode("phasor", {}, [
          createNode("const", {key: 'fq', value: 440}, [])
        ])
      ])
    ])
  );

  // Second render inserts a tanh at the top, we should find this and
  // share all of the child nodes during this render pass
  tr.render(
    createNode("tanh", {}, [
      createNode("sin", {}, [
        createNode("mul", {}, [
          createNode("const", {value: 2 * Math.PI}, []),
          createNode("phasor", {}, [
            createNode("const", {key: 'fq', value: 440}, [])
          ])
        ])
      ])
    ])
  );

  expect(sortInstructionBatch(tr.getBatch())).toMatchSnapshot();
});

test('distinguished subtrees by key', function() {
  let tr = new TestRenderer();

  const voices = [
    {key: 'fq1', freq: 440},
    {key: 'fq2', freq: 440},
    {key: 'fq3', freq: 440},
    {key: 'fq4', freq: 440},
  ];

  const renderVoice = (voice) => {
    return (
      createNode("sin", {}, [
        createNode("mul", {}, [
          createNode("const", {value: 2 * Math.PI}, []),
          createNode("phasor", {}, [
            createNode("const", {key: voice.key, value: voice.freq}, [])
          ])
        ])
      ])
    );
  };

  tr.render(createNode("add", {}, voices.map(renderVoice)))

  expect(sortInstructionBatch(tr.getBatch())).toMatchSnapshot();
});

test('structural equality with value change', function() {
  const tr = new TestRenderer();

  const voices = [
    {key: 'fq1', freq: 440},
    {key: 'fq2', freq: 440},
    {key: 'fq3', freq: 440},
    {key: 'fq4', freq: 440},
  ];

  const renderVoice = (voice) => {
    return (
      createNode("sin", {}, [
        createNode("mul", {}, [
          createNode("const", {value: 2 * Math.PI}, []),
          createNode("phasor", {}, [
            createNode("const", {key: voice.key, value: voice.freq}, [])
          ])
        ])
      ])
    );
  };

  tr.render(createNode("add", {}, voices.map(renderVoice)))

  // Change one of the keyed values, should see structural equality
  // with no new nodes created
  voices[0].freq = 441;

  tr.render(createNode("add", {}, voices.map(renderVoice)))

  expect(sortInstructionBatch(tr.getBatch())).toMatchSnapshot();
});

// Testing here to ensure that the root node activation works as expected.
test('switch and switch back', function() {
  const tr = new TestRenderer();

  const renderVoice = (voice) => {
    return (
      createNode("sin", {}, [
        createNode("mul", {}, [
          createNode("const", {value: 2 * Math.PI}, []),
          createNode("phasor", {}, [
            createNode("const", {key: voice.key, value: voice.freq}, [])
          ])
        ])
      ])
    );
  };

  // We're going to render three times with graphs A, B, A. We want to see on
  // the third render that almost nothing was done because we still have the nodes
  // around that we need, i.e. they haven't been garbage collected, but we will need
  // to see that the correct root nodes are in place.
  tr.render(renderVoice({key: 'hi', freq: 440}));
  tr.render(renderVoice({key: 'bye', freq: 880}));

  tr.render(renderVoice({key: 'hi', freq: 440}));
  expect(sortInstructionBatch(tr.getBatch())).toMatchSnapshot();
});

test('composite with keyed children', function() {
  const tr = new TestRenderer();
  const composite = ({context, props, children}) => {
    return createNode("add", {}, children);
  };

  tr.render(createNode(composite, {}, [
    createNode("const", {key: 'g', value: 0}, [])
  ]));

  expect(sortInstructionBatch(tr.getBatch())).toMatchSnapshot();

  tr.render(createNode(composite, {}, [
    createNode("const", {key: 'g', value: 1}, [])
  ]));

  expect(sortInstructionBatch(tr.getBatch())).toMatchSnapshot();
});

test('shared reference to composite node', function() {
  let tr = new TestRenderer();
  let calls = 0;

  let train = ({props, children: [rate]}) => {
    calls++;

    return (
      createNode("le", {}, [
        createNode("phasor", {}, [rate]),
        0.5,
      ])
    );
  };

  // We expect `train` to be called once in resolving the composite node, and for
  // the latter two instances in our stereo graph to be resolved via the cache
  let t = createNode(train, {}, [5]);

  tr.render(
    createNode("seq", {seq: [1, 2, 3]}, [t, t]),
    createNode("seq", {seq: [1, 2, 3]}, [t]),
  );

  expect(calls).toBe(1);

  // Now if we make multiple composite node instances each referencing the same composite
  // function, we have to unroll each of them with their associated inputs. Due to the v2.0.1
  // change which disables memoization we no longer attempt to prevent that second call by memoization
  calls = 0;

  let u = createNode(train, {}, [5]);
  let v = createNode(train, {}, [5]);

  tr.render(
    createNode("seq", {seq: [1, 2, 3]}, [u, v]),
    createNode("seq", {seq: [1, 2, 3]}, [v]),
  );

  expect(calls).toBe(2);
});

test('garbage collection', function() {
  let tr = new TestRenderer();

  // First, render a sine tone with `sin`
  tr.render(
    createNode("sin", {}, [
      createNode("mul", {}, [
        2 * Math.PI,
        createNode("phasor", {}, [440])
      ])
    ])
  );

  // Next we're going to render a sine tone using cosine. We'll expect to
  // see most of the prior structure preserved.
  tr.gc();

  tr.render(
    createNode("cos", {}, [
      createNode("mul", {}, [
        2 * Math.PI,
        createNode("phasor", {}, [440])
      ])
    ])
  );

  expect(sortInstructionBatch(tr.getBatch())).toMatchSnapshot();

  // Now if we step the garbage collector enough we should see the sine node
  // and its parent root get cleaned up now that they're no longer referenced
  // in the active tree
  for (let i = 0; i < tr.getTerminalGeneration() - 1; ++i) {
    tr.gc();
  }

  expect(sortInstructionBatch(tr.getBatch())).toMatchSnapshot();
});
