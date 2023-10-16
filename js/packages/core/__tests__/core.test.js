import {
  createNode,
  renderWithDelegate,
  stepGarbageCollector,
  resolve,
  Renderer,
} from '..';


class TestRenderer extends Renderer {
  constructor() {
    super(() => {});
  }

  render(...args) {
    this._delegate.clear();

    renderWithDelegate(this._delegate, args.map(resolve));

    return {
      nodesAdded: this._delegate.nodesAdded,
      edgesAdded: this._delegate.edgesAdded,
      propsWritten: this._delegate.propsWritten,
      elapsedTimeMs: 0,
    };
  }

  getBatch() {
    return this._delegate.getPackedInstructions();
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

  // This is an odd sort step; in particular, we only sort here within each
  // type of instruction. That is, createNodes all come before deleteNodes, all come before
  // appendChilds, etc, in the snapshot files. The Renderer instance, via its getPackedInstructions
  // method, provides that group-level sort during render. However, the snapshots then sort
  // within those groups according to hash values so that our tests are not coupled to the traversal
  // order of the reconciler. This whole thing is temporary anyway; once we finish the v3 refactor,
  // we can remove this sorting and update the snapshots.
  return copy.sort((a, b) => {
    if (a[0] === b[0]) {
      return a[1] < b[1] ? -1 : 1;
    }

    return 0;
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
  tr._delegate.clear();

  // Now if we step the garbage collector enough we should see the sine node
  // and its parent root get cleaned up now that they're no longer referenced
  // in the active tree
  for (let i = 0; i < tr.getTerminalGeneration() - 1; ++i) {
    tr.gc();
  }

  expect(sortInstructionBatch(tr.getBatch())).toMatchSnapshot();
});

test('refs', function() {
  let tr = new TestRenderer();

  // Sine tone with a frequency set by ref
  let [freq, setFreq] = tr.createRef("const", {value: 440}, []);

  // Render the ref
  tr.render(
    createNode("sin", {}, [
      createNode("mul", {}, [
        2 * Math.PI,
        createNode("phasor", {}, [freq])
      ])
    ])
  );

  // Using our ref setter
  setFreq({value: 550});

  // We expect here to see a single set property instruction
  // followed by the commit instruction
  expect(tr.getBatch()).toEqual([
    [ 3, 1915043800, 'value', 550 ],
    [ 5 ],
  ]);
});
