import invariant from 'invariant';

import {
  EventEmitter,
  Renderer,
} from '@elemaudio/core';

// NEEDS WASM_ASYNC COMPILATION FLAG IN THE WASM BUILD SCRIPT
import Module from './elementary-wasm';

export default class OfflineRenderer extends EventEmitter {
  private _module: any;
  private _native: any;
  private _renderer: Renderer;
  private _numInputChannels: number;
  private _numOutputChannels: number;
  private _blockSize: number;

  async initialize(options) {
    // Default option assignment
    const config = Object.assign({
      numInputChannels: 0,
      numOutputChannels: 2,
      sampleRate: 44100,
      blockSize: 512,
      virtualFileSystem: {},
    }, options);

    // Unpack
    const {
      numInputChannels,
      numOutputChannels,
      sampleRate,
      blockSize,
      virtualFileSystem,
    } = config;

    this._numInputChannels = numInputChannels;
    this._numOutputChannels = numOutputChannels;
    this._blockSize = blockSize;

    try {
      this._module = await Module();
      this._native = new this._module.ElementaryAudioProcessor(numInputChannels, numOutputChannels);
      this._native.prepare(sampleRate, blockSize);
    } catch (e) {
      if (e instanceof WebAssembly.RuntimeError) {
        throw new Error('Failed to load the Elementary WASM backend. Running Elementary within Node.js requires Node v18, or Node v16 with --experimental-wasm-eh enabled.');
      }

      throw e;
    }

    const validVFS = typeof virtualFileSystem === 'object' &&
      virtualFileSystem !== null &&
      Object.keys(virtualFileSystem).length > 0;

    if (validVFS) {
      for (let [key, val] of Object.entries(virtualFileSystem)) {
        this._native.updateSharedResourceMap(key, val, (message) => {
          this.emit('error', new Error(message));
        });
      }
    }

    this._renderer = new Renderer((batch) => {
      return this._native.postMessageBatch(batch);
    });
  }

  async render(...args) {
    const {result, ...stats} = await this._renderer.render(...args);

    if (!result.success) {
      return Promise.reject(result);
    }

    return Promise.resolve(stats);
  }

  createRef(kind, props, children) {
    return this._renderer.createRef(kind, props, children);
  }

  process(inputs: Array<Float32Array>, outputs: Array<Float32Array>) {
    if (!Array.isArray(inputs) || inputs.length !== this._numInputChannels)
      throw new Error(`Invalid input data; expected an array of ${this._numInputChannels} Float32Array buffers.`);

    if (!Array.isArray(outputs) || outputs.length !== this._numOutputChannels)
      throw new Error(`Invalid output data; expected an array of ${this._numOutputChannels} Float32Array buffers.`);

    // Nothing to do
    if (outputs.length === 0) {
      return;
    }

    // Process internal.
    //
    // We step through the desired output buffer in blocks to ensure we
    // process the event queue regularly. If the user wants smaller block sizes
    // they can configure it as such or simply call `process` multiple times themselves.
    for (let k = 0; k < outputs[0].length; k += this._blockSize) {
      // Write the input data to the internal memory
      inputs.forEach((buf, i) => {
        const internalData = this._native.getInputBufferData(i);

        for (let j = 0; j < this._blockSize; ++j) {
          internalData[j] = (k + j) < buf.length ? buf[k + j] : 0;
        }
      });

      this._native.process(this._blockSize);

      this._native.processQueuedEvents((evtBatch) => {
        evtBatch.forEach(({type, event}) => {
          this.emit(type, event);
        });
      });

      // Write the internal memory to a new output buffer and return
      outputs.forEach((buf, i) => {
        const internalData = this._native.getOutputBufferData(i);

        for (let j = 0; j < this._blockSize; ++j) {
          if (k + j < buf.length) {
            buf[k + j] = internalData[j];
          }
        }
      });
    }
  }

  updateVirtualFileSystem(vfs) {
    const valid = typeof vfs === 'object' && vfs !== null;

    invariant(valid, "Virtual file system must be an object mapping string type keys to Array|Float32Array type values");

    Object.keys(vfs).forEach(function(key) {
      const validValue = typeof vfs[key] === 'object' &&
        (Array.isArray(vfs[key]) || (vfs[key] instanceof Float32Array));

      invariant(validValue, "Virtual file system must be an object mapping string type keys to Array|Float32Array type values");
    });

    for (let [key, val] of Object.entries(vfs)) {
      this._native.updateSharedResourceMap(key, val, (message) => {
        this.emit('error', new Error(message));
      });
    }
  }

  pruneVirtualFileSystem() {
    this._native.pruneSharedResourceMap();
  }

  listVirtualFileSystem() {
    return this._native.listSharedResourceMap();
  }

  reset() {
    this._native.reset();
  }

  setCurrentTime(t) {
    this._native.setCurrentTime(t);
  }

  setCurrentTimeMs(t) {
    this._native.setCurrentTimeMs(t);
  }
}
