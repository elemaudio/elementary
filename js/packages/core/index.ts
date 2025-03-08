import {
  renderWithDelegate,
} from './src/Reconciler.gen';

import { updateNodeProps } from './src/Hash';

import {
  createNode,
  isNode,
  resolve,
  unpack
} from './nodeUtils';

import * as co from './lib/core';
import * as dy from './lib/dynamics';
import * as en from './lib/envelopes';
import * as fi from './lib/filters';
import * as ma from './lib/math';
import * as mc from './lib/mc';
import * as os from './lib/oscillators';
import * as si from './lib/signals';

export type { ElemNode, NodeRepr_t } from './nodeUtils';
export { default as EventEmitter } from './src/Events';

const stdlib = {
  ...co,
  ...dy,
  ...en,
  ...fi,
  ...ma,
  ...os,
  ...si,
  mc,
  // Aliases for reserved keyword conflicts
  "const": co.constant,
  "in": ma.identity,
};

const InstructionTypes = {
  CREATE_NODE: 0,
  APPEND_CHILD: 2,
  SET_PROPERTY: 3,
  ACTIVATE_ROOTS: 4,
  COMMIT_UPDATES: 5,
};

// A default render delegate which batches instruction sets while recording
// stats about the render pass.
class Delegate {
  public nodesAdded: number;
  public nodesRemoved: number;
  public edgesAdded: number;
  public propsWritten: number;

  public nodeMap: Map<number, any>;

  private currentActiveRoots: Set<number>;
  private batch: any;

  constructor() {
    this.nodeMap = new Map();
    this.currentActiveRoots = new Set();

    this.clear();
  }

  clear() {
    this.nodesAdded = 0;
    this.nodesRemoved = 0;
    this.edgesAdded = 0;
    this.propsWritten = 0;

    this.batch = {
      createNode: [],
      appendChild: [],
      setProperty: [],
      activateRoots: [],
      commitUpdates: [],
    };
  }

  getNodeMap() { return this.nodeMap; }

  createNode(hash, type) {
    this.nodesAdded++;
    this.batch.createNode.push([InstructionTypes.CREATE_NODE, hash, type]);
  }

  appendChild(parentHash, childHash, childOutputChannel) {
    this.edgesAdded++;
    this.batch.appendChild.push([InstructionTypes.APPEND_CHILD, parentHash, childHash, childOutputChannel]);
  }

  setProperty(hash, key, value) {
    this.propsWritten++;
    this.batch.setProperty.push([InstructionTypes.SET_PROPERTY, hash, key, value]);
  }

  activateRoots(roots) {
    // If we're asked to activate exactly the roots that are already active,
    // no need to push the instruction. We need the length/size check though
    // because it may be that we're asked to activate a subset of the current
    // active roots, in which case we need the instruction to prompt the engine
    // to deactivate the now excluded roots.
    let alreadyActive = roots.length === this.currentActiveRoots.size &&
      roots.every((root) => this.currentActiveRoots.has(root));

    if (!alreadyActive) {
      this.batch.activateRoots.push([InstructionTypes.ACTIVATE_ROOTS, roots]);
      this.currentActiveRoots = new Set(roots);
    }
  }

  commitUpdates() {
    this.batch.commitUpdates.push([InstructionTypes.COMMIT_UPDATES]);
  }

  getPackedInstructions() {
    return [
      ...this.batch.createNode,
      ...this.batch.appendChild,
      ...this.batch.setProperty,
      ...this.batch.activateRoots,
      ...this.batch.commitUpdates,
    ];
  }
}

// A quick shim for platforms which do not support the `performance` global
function now() {
  if (typeof performance === 'undefined') {
    return Date.now();
  }

  return performance.now();
}

// A generic Renderer just wraps the above Delegate to provide a simple
// render root.
//
// Most applications will just need this generic Renderer with a callback for
// marshaling instructions over to the engine, wherever that may be.
//
// Applications which need more fine grained control can write their own Renderer
// or their own Delegate.
class Renderer {
  private _delegate: Delegate;
  private _sendMessage: Function;
  private _nextRefId: number;

  constructor(sendMessage) {
    this._delegate = new Delegate();
    this._sendMessage = sendMessage;
    this._nextRefId = 0;
  }

  // A method for creating "refs," which looks the same as the function for creating
  // nodes but captures the context of the Renderer instance to provide scoped property
  // updates to the ref without incurring a full graph construction and reconciliation pass.
  //
  // Example:
  //  let [cutoffFreq, setCutoffFreq] = createRef("const", {value: 440}, []);
  //
  //  // Render a ref just the same as you would any other node
  //  core.render(el.lowpass(cutoffFreq, 1, el.in({channel: 0})));
  //
  //  // Subsequent property changes can be made through the property setter returned
  //  // from the call to createRef
  //  setCutoffFreq({ value: 440 });
  //
  // Note: refs should only be rendered by the Renderer instance from which they were created.
  // In other words, don't share refs between different renderer instances.
  createRef(kind, props, children) {
    let key = `__refKey:${this._nextRefId++}`;
    let node = createNode(kind, Object.assign({key}, props), children);

    let setter = (newProps) => {
      if (!this._delegate.nodeMap.has(node.hash)) {
        throw new Error('Cannot update a ref that has not been mounted; make sure you render your node first')
      }

      const nodeMapCopy = this._delegate.nodeMap.get(node.hash);

      this._delegate.clear();
      updateNodeProps(this._delegate, node.hash, nodeMapCopy.props, newProps);
      this._delegate.commitUpdates();

      // Invoke message passing
      const instructions = this._delegate.getPackedInstructions();

      return Promise.resolve(this._sendMessage(instructions));
    };

    return [node, setter] as const;
  }

  render(...args) {
    return this.renderWithOptions({ rootFadeInMs: 20, rootFadeOutMs: 20 }, ...args);
  }

  renderWithOptions(options: { rootFadeInMs: number, rootFadeOutMs: number }, ...args) {
    const t0 = now();

    this._delegate.clear();
    renderWithDelegate(this._delegate as any, args.map(resolve), options.rootFadeInMs, options.rootFadeOutMs);

    const t1 = now();

    // Invoke message passing
    const instructions = this._delegate.getPackedInstructions();

    return Promise.resolve(this._sendMessage(instructions)).then((result) => {
      // Pack render stats with the result of the message passing
      return {
        result,
        nodesAdded: this._delegate.nodesAdded,
        edgesAdded: this._delegate.edgesAdded,
        propsWritten: this._delegate.propsWritten,
        elapsedTimeMs: t1 - t0,
      };
    });
  }

  prune(nodeIds) {
    nodeIds.forEach((n) => {
      this._delegate.nodeMap.delete(n);
    });
  }
}

export {
  createNode,
  Delegate,
  stdlib as el,
  isNode,
  Renderer,
  renderWithDelegate,
  resolve,
  stdlib,
  unpack
};
