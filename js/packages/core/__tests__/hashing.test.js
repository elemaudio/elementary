import {
  el,
  renderWithDelegate,
} from '..';


const InstructionTypes = {
  CREATE_NODE: 0,
  DELETE_NODE: 1,
  APPEND_CHILD: 2,
  SET_PROPERTY: 3,
  ACTIVATE_ROOTS: 4,
  COMMIT_UPDATES: 5,
};

class HashlessRenderer {
  constructor(config) {
    this.nodeMap = new Map();
    this.memoMap = new Map();
    this.batch = [];
    this.roots = [];

    this.nextMaskId = 0;
    this.maskTable = new Map();
  }

  getMaskId(hash) {
    if (!this.maskTable.has(hash)) {
      this.maskTable.set(hash, this.nextMaskId++);
    }

    return this.maskTable.get(hash);
  }

  getNodeMap() {
    return this.nodeMap;
  }

  getMemoMap() {
    return this.memoMap;
  }

  getRenderContext() {
    return { sampleRate: 44100 };
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

  createNode(hash, type) {
    this.batch.push([InstructionTypes.CREATE_NODE, this.getMaskId(hash), type]);
  }

  deleteNode(hash) {
    this.batch.push([InstructionTypes.DELETE_NODE, this.getMaskId(hash)]);
  }

  appendChild(parentHash, childHash) {
    this.batch.push([InstructionTypes.APPEND_CHILD, this.getMaskId(parentHash), this.getMaskId(childHash)]);
  }

  setProperty(hash, key, val) {
    this.batch.push([InstructionTypes.SET_PROPERTY, this.getMaskId(hash), key, val]);
  }

  activateRoots(roots) {
    this.batch.push([InstructionTypes.ACTIVATE_ROOTS, roots.map(x => this.getMaskId(x))]);
  }

  commitUpdates() {
    this.batch.push([InstructionTypes.COMMIT_UPDATES]);
  }

  render(...args) {
    this.roots = renderWithDelegate(this, args);
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

// To test that our algorithm works even if the hashing function changes. We
// want to see that the same sets of instructions come through
test('instruction set similarity without hash values', function() {
  let tr = new HashlessRenderer();

  tr.render(el.cycle(440));
  expect(sortInstructionBatch(tr.getBatch())).toMatchSnapshot();
});

test('instruction set similarity without hash values 2', function() {
  let tr = new HashlessRenderer();

  let synthVoice = (hz) => el.mul(
    0.25,
    el.add(
      el.blepsaw(el.mul(hz, 1.001)),
      el.blepsquare(el.mul(hz, 0.994)),
      el.blepsquare(el.mul(hz, 0.501)),
      el.blepsaw(el.mul(hz, 0.496)),
    ),
  );

  let train = el.train(4.8);
  let arp = [0, 4, 7, 11, 12, 11, 7, 4].map(x => 261.63 * 0.5 * Math.pow(2, x / 12));

  let modulate = (x, rate, amt) => el.add(x, el.mul(amt, el.cycle(rate)));
  let env = el.adsr(0.01, 0.5, 0, 0.4, train);
  let filt = (x) => el.lowpass(
    el.add(40, el.mul(modulate(1840, 0.05, 1800), env)),
    1,
    x
  );

  let out = el.mul(0.25, filt(synthVoice(el.seq({seq: arp, hold: true}, train, 0))));
  tr.render(out, out);
  expect(sortInstructionBatch(tr.getBatch())).toMatchSnapshot();
});
