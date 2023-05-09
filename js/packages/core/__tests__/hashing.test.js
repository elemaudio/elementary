import {
  el,
  renderWithDelegate,
} from '..';


class HashlessRenderer {
  constructor(config) {
    this.nodeMap = new Map();
    this.memoMap = new Map();
    this.batch = [];
    this.roots = [];

    this.nextMaskId = 0;
    this.maskTable = {};
  }

  getMaskId(hash) {
    let r = this.nextMaskId++;

    this.maskTable[hash] = r;
    return r;
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
    this.batch.push(["createNode", this.getMaskId(hash), type]);
  }

  deleteNode(hash) {
    this.batch.push(["deleteNode", this.maskTable[hash]]);
  }

  appendChild(parentHash, childHash) {
    this.batch.push(["appendChild", this.maskTable[parentHash], this.maskTable[childHash]]);
  }

  setProperty(hash, key, val) {
    this.batch.push(["setProperty", this.maskTable[hash], key, val]);
  }

  activateRoots(roots) {
    this.batch.push(["activateRoots", roots.map(x => this.maskTable[x])]);
  }

  commitUpdates() {
    this.batch.push(["commitUpdates"]);
  }

  render(...args) {
    this.roots = renderWithDelegate(this, args);
  }
}

// To test that our algorithm works even if the hashing function changes. We
// want to see that the same sets of instructions come through
test('instruction set similarity without hash values', function() {
  let tr = new HashlessRenderer();

  tr.render(el.cycle(440));
  expect(tr.getBatch()).toMatchSnapshot();
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
  expect(tr.getBatch()).toMatchSnapshot();
});
