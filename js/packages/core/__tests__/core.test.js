import {
  createNode,
  renderWithDelegate,
  stepGarbageCollector,
} from '..';


class TestRenderer {
  constructor(config) {
    this.nodeMap = new Map();
    this.memoMap = new Map();
    this.batch = [];
    this.roots = [];
    this.config = Object.assign({
      logCreateNode: true,
      logDeleteNode: true,
      logAppendChild: true,
      logSetProperty: true,
      logActivateRoots: true,
      logCommitUpdates: true,
    }, config);
  }

  getNodeMap() {
    return this.nodeMap;
  }

  getMemoMap() {
    return this.memoMap;
  }

  getRenderContext() {
    return {};
  }

  getActiveRoots() {
    return this.roots;
  }

  getTerminalGeneration() {
    return 4;
  }

  getBatch() {
    return this.batch;
  }

  clearBatch() {
    this.batch = [];
  }

  createNode(...args) {
    if (this.config.logCreateNode) {
      this.batch.push(["createNode", ...args]);
    }
  }

  deleteNode(...args) {
    if (this.config.logDeleteNode) {
      this.batch.push(["deleteNode", ...args]);
    }
  }

  appendChild(...args) {
    if (this.config.logAppendChild) {
      this.batch.push(["appendChild", ...args]);
    }
  }

  setProperty(...args) {
    if (this.config.logSetProperty) {
      this.batch.push(["setProperty", ...args]);
    }
  }

  activateRoots(...args) {
    if (this.config.logActivateRoots) {
      this.batch.push(["activateRoots", ...args]);
    }
  }

  commitUpdates(...args) {
    if (this.config.logCommitUpdates) {
      this.batch.push(["commitUpdates", ...args]);
    }
  }

  render(...args) {
    this.roots = renderWithDelegate(this, args);
  }

  gc() {
    stepGarbageCollector(this);
  }
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

  expect(tr.getBatch()).toMatchSnapshot();
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

  expect(tr.getBatch()).toMatchSnapshot();
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
  expect(tr.getBatch()).toMatchSnapshot();
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
  expect(tr.getBatch()).toMatchSnapshot();
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

  // Clear the call stack
  tr.clearBatch();

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

  expect(tr.getBatch()).toMatchSnapshot();
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

  expect(tr.getBatch()).toMatchSnapshot();
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

  // Clear the call stack
  tr.clearBatch();

  // Change one of the keyed values, should see structural equality
  // with no new nodes created
  voices[0].freq = 441;

  tr.render(createNode("add", {}, voices.map(renderVoice)))

  expect(tr.getBatch()).toMatchSnapshot();
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

  tr.clearBatch();

  tr.render(renderVoice({key: 'hi', freq: 440}));
  expect(tr.getBatch()).toMatchSnapshot();
});

test('simple memo', function() {
  const tr = new TestRenderer();

  // If you look in the git history at the memo test implementation I had written prior to this,
  // you'll see that I was relying on having an actual memoization table which held multiple prior
  // computation results. That was how the original memo implementation was written too. But here I'm
  // now intentionally clear about that memoization only performs a comparison between what is currently
  // rendered and what is next up. The reason for this is that even if the memo table holds results from
  // several renders ago, any graph topology changes that are now strictly unused will be cleaned up in the
  // realtime rendering engine and eventually propagated back to javascript for removal. Attempting to shortcut
  // a render by pulling a memoized result from far back in time might yield a subtree which doesn't exist anymore.
  //
  // So, here we have a proper test for memoization with a comparison between what currently exists and
  // what we want to render next.
  const voices = [
    {key: 'fq1', freq: 440},
    {key: 'fq2', freq: 440},
    {key: 'fq3', freq: 440},
    {key: 'fq4', freq: 440},
  ];

  let memoCalls = 0;

  const synth = ({props}) => {
    memoCalls++;

    return (
      createNode("add", {}, props.voices.map(function(voice) {
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
      }))
    );
  };

  tr.render(createNode(synth, {voices}, []));

  // So the first time, we expect that the memoized render function has
  // been called
  expect(memoCalls).toBe(1);
  tr.clearBatch();

  // Now when we render again, but mutate the structure above the memoized
  // subtrees, we should expect that the memoizer does not even invoke this
  // function again because none of the voice data changed. Thus we only ever
  // compute part of the tree as needed.
  tr.render(
    createNode("tanh", {}, [
      createNode(synth, {voices}, [])
    ])
  );

  expect(memoCalls).toBe(1);

  // And of course we expect that the tanh and the new root were the only
  // new things made here
  expect(tr.getBatch()).toMatchSnapshot();
});

test('nested memo', function() {
  const tr = new TestRenderer({
      logCreateNode: true,
      logDeleteNode: false,
      logAppendChild: false,
      logSetProperty: false,
      logCommitUpdates: false,
  });

  const generator = ({props}) => {
    return (
      createNode("le", {}, [
        createNode("phasor", {}, [props.rate]),
        0.5,
      ])
    );
  };

  const smoother = ({props, children: [x]}) => {
    return (
      createNode("pole", {}, [
        createNode("mul", {}, [
          0.11,
          x
        ])
      ])
    );
  }

  const composite = (props) => {
    return (
      createNode(smoother, {}, [
        createNode(generator, {...props}, [])
      ])
    );
  }

  tr.render(composite({rate: 12}));

  expect(tr.getBatch()).toMatchSnapshot();


  tr.clearBatch();
  tr.render(composite({rate: 14}));

  expect(tr.getBatch()).toMatchSnapshot();
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

  {
    // The first time through, we expect `train` to be called once, and for
    // the latter two instances in our stereo graph to be resolved via the visit
    // flag shortcut.
    let t = createNode(train, {}, [5]);

    tr.render(
      createNode("seq", {seq: [1, 2, 3]}, [t, t]),
      createNode("seq", {seq: [1, 2, 3]}, [t]),
    );

    expect(calls).toBe(1);
  }

  tr.clearBatch();

  {
    // The second time through, we've dropped our previous reference to `t`, so
    // we have a new, unresolved composite node. We need to evaluate it again, but
    // due to the new implicit memoization, we expect train will not actually be called
    // again and the tree resolved here will be identical to the prior.
    let t = createNode(train, {}, [5]);

    tr.render(
      createNode("seq", {seq: [1, 2, 3]}, [t, t]),
      createNode("seq", {seq: [1, 2, 3]}, [t]),
    );

    expect(calls).toBe(1);
  }

  expect(tr.getBatch()).toMatchSnapshot();
});

test('memo and keyed props', function() {
  let tr = new TestRenderer();
  let calls = 0;

  let CycleComposite = ({props}) => {
    calls++;

    return (
      createNode("sin", {}, [
        createNode("mul", {}, [
          createNode("const", {value: 2 * Math.PI}, []),
          createNode("phasor", {}, [
            createNode("const", {key: props.key, value: props.value}, [])
          ])
        ])
      ])
    );
  };

  // Here we render an instance of the composite node with a given set of properties
  // and children. We expect the composite function to be invoked and for the subtree
  // to be mounted.
  tr.clearBatch();
  tr.render(createNode(CycleComposite, {key: 'hi', value: 440}, []));
  expect(calls).toBe(1);
  expect(tr.getBatch()).toMatchSnapshot();

  // Some time later, we render another instance of the node. We'll find here that the
  // input to the composite function does not hit the memo cache, so we invoke the function
  // again and carry out the diff. Here we expect only to see a setProperty call to update
  // the underlying keyed const node to a value of 880
  tr.clearBatch();
  tr.render(createNode(CycleComposite, {key: 'hi', value: 880}, []));
  expect(calls).toBe(2);
  expect(tr.getBatch()).toMatchSnapshot();

  // Finally, we render again, this time hitting the memo cache because we identical inputs
  // to our first render. We expect to see that calls has not incremented, because we have a cached
  // structure that we can pull out of the memo table, but we need the recursive props update
  // to ensure that the existing node props get updated accordingly.
  tr.clearBatch();
  tr.render(createNode(CycleComposite, {key: 'hi', value: 440}, []));
  expect(calls).toBe(2);
  expect(tr.getBatch()).toMatchSnapshot();
})

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
  tr.clearBatch();
  tr.gc();

  tr.render(
    createNode("cos", {}, [
      createNode("mul", {}, [
        2 * Math.PI,
        createNode("phasor", {}, [440])
      ])
    ])
  );

  expect(tr.getBatch()).toMatchSnapshot();

  // Now if we step the garbage collector enough we should see the sine node
  // and its parent root get cleaned up now that they're no longer referenced
  // in the active tree
  for (let i = 0; i < tr.getTerminalGeneration() - 1; ++i) {
    tr.clearBatch();
    tr.gc();
  }

  expect(tr.getBatch()).toMatchSnapshot();
});
