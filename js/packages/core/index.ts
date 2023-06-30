import {
  renderWithDelegate,
  stepGarbageCollector,
} from './src/Reconciler.gen';

import {
  createNode,
  isNode,
  resolve,
} from './nodeUtils';

import * as co from './lib/core';
import * as dy from './lib/dynamics';
import * as en from './lib/envelopes';
import * as ma from './lib/math';
import * as fi from './lib/filters';
import * as os from './lib/oscillators';
import * as si from './lib/signals';

import type {NodeRepr_t} from './src/Reconciler.gen';
export type {NodeRepr_t};

const stdlib = {
  ...co,
  ...dy,
  ...en,
  ...fi,
  ...ma,
  ...os,
  ...si,
  // Aliases for reserved keyword conflicts
  "const": co.constant,
  "in": ma.identity,
};

const InstructionTypes = {
  CREATE_NODE: 0,
  DELETE_NODE: 1,
  APPEND_CHILD: 2,
  SET_PROPERTY: 3,
  ACTIVATE_ROOTS: 4,
  COMMIT_UPDATES: 5,
};

// A default render delegate which batches instruction sets and invokes the
// provided batchHandler callback on transaction commit, while also recording
// stats about the render pass.
class Delegate {
  public nodesAdded: number;
  public nodesRemoved: number;
  public edgesAdded: number;
  public propsWritten: number;

  private nodeMap: Map<number, any>;
  private currentActiveRoots: Set<number>;

  private renderContext: any;
  private batch: Array<any>;
  private batchHandler: Function;

  constructor(sampleRate, batchHandler) {
    this.nodesAdded = 0;
    this.nodesRemoved = 0;
    this.edgesAdded = 0;
    this.propsWritten = 0;
    this.nodeMap = new Map();
    this.currentActiveRoots = new Set();

    this.renderContext = {
      sampleRate,
      blockSize: 512,
      numInputs: 1,
      numOutputs: 1,
    };

    this.batch = [];
    this.batchHandler = batchHandler;
  }

  getNodeMap() { return this.nodeMap; }
  getTerminalGeneration() { return 4; }
  getRenderContext() { return this.renderContext; }

  createNode(hash, type) {
    this.nodesAdded++;
    this.batch.push([InstructionTypes.CREATE_NODE, hash, type]);
  }

  deleteNode(hash) {
    this.nodesRemoved++;
    this.batch.push([InstructionTypes.DELETE_NODE, hash]);
  }

  appendChild(parentHash, childHash) {
    this.edgesAdded++;
    this.batch.push([InstructionTypes.APPEND_CHILD, parentHash, childHash]);
  }

  setProperty(hash, key, value) {
    this.propsWritten++;
    this.batch.push([InstructionTypes.SET_PROPERTY, hash, key, value]);
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
      this.batch.push([InstructionTypes.ACTIVATE_ROOTS, roots]);
      this.currentActiveRoots = new Set(roots);
    }
  }

  commitUpdates() {
    this.batch.push([InstructionTypes.COMMIT_UPDATES]);
    this.batchHandler(this.batch);
    this.batch = [];
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

  constructor(sampleRate, sendMessage) {
    this._delegate = new Delegate(sampleRate, (batch) => {
      sendMessage(batch);
    });
  }

  render(...args) {
    const t0 = now();

    this._delegate.nodesAdded = 0;
    this._delegate.nodesRemoved = 0;
    this._delegate.edgesAdded = 0;
    this._delegate.propsWritten = 0;

    renderWithDelegate(this._delegate as any, args.map(resolve));

    const t1 = now();

    // Return render stats
    return {
      nodesAdded: this._delegate.nodesAdded,
      edgesAdded: this._delegate.edgesAdded,
      propsWritten: this._delegate.propsWritten,
      elapsedTimeMs: t1 - t0,
    };
  }
}

export {
  Renderer,
  createNode,
  isNode,
  resolve,
  renderWithDelegate,
  stepGarbageCollector,
  stdlib,
  stdlib as el,
};
