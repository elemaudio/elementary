import invariant from 'invariant';

import {
  EventEmitter,
  Renderer,
} from '@elemaudio/core';

/* @ts-ignore */
import WorkletProcessor from './raw/WorkletProcessor';
import WasmModule from './raw/elementary-wasm';

export default class WebAudioRenderer extends EventEmitter {
  private _worklet: any;
  private _renderer: Renderer;
  private _timer: any;

  async initialize(audioContext: AudioContext, workletOptions: AudioWorkletNodeOptions = {}, eventInterval: number = 16): Promise<AudioWorkletNode> {
    invariant(typeof audioContext === 'object' && audioContext !== null, 'First argument to initialize must be a valid AudioContext instance.');
    invariant(typeof workletOptions === 'object' && workletOptions !== null, 'The optional second argument to initialize must be an object.');

    // A bit of a hack here, but if we try to register the same ElementaryAudioWorkletProcessor on the same
    // AudioContext twice, the browser will throw an error. We don't have any way to ask the AudioContext what
    // worklets it has already registered, so our only option here is to keep some state around to check for
    // ourselves. Writing to the AudioContext like this is hacky, but it works. An alternative would be keeping
    // our own Map<AudioContext, boolean> but that feels like overkill.
    //
    // @ts-ignore
    if (!audioContext.__elemRegistered) {
      const blob = new Blob([WasmModule, WorkletProcessor], {type: 'text/javascript'});
      const blobUrl = URL.createObjectURL(blob);

      if (!audioContext.audioWorklet) {
        throw new Error("BaseAudioContext.audioWorklet is missing; are you running in a secure context (https)?");
      }

      // This neat trick with the Blob URL allows me to inject the module without
      // needing to serve it from somewhere on the file system. The files loaded
      // from the raw/* directory are loaded as raw, minified strings.
      await audioContext.audioWorklet.addModule(blobUrl);

      // @ts-ignore
      audioContext.__elemRegistered = true;
    }

    this._worklet = new AudioWorkletNode(audioContext, 'ElementaryAudioWorkletProcessor', Object.assign({
      numberOfInputs: 0,
      numberOfOutputs: 1,
      outputChannelCount: [2],
    }, workletOptions));

    // We defer the resolution of this method's result until we get the load
    // event back from the worklet. That way, if the user is awaiting the result
    // of the initialize event before calling `render`, we guarantee that we've
    // actually created our Renderer instance here in time.
    return await new Promise((resolve, reject) => {
      this._worklet.port.onmessage = (e) => {
        const [type, evt] = e.data;

        if (type === 'load') {
          this._renderer = new Renderer(evt.sampleRate, (batch) => {
            this._worklet.port.postMessage({
              type: 'renderInstructions',
              batch,
            });
          });

          resolve(this._worklet);
        }

        if (type === 'error') {
          return this.emit(type, new Error(evt));
        }

        if (type === 'eventBatch') {
          return evt.forEach(({type, event}) => {
            this.emit(type, event);
          });
        }

        this.emit(type, evt);
      };

      // TODO: Clean up? Unsubscribe option?
      this._timer = window.setInterval(() => {
        this._worklet.port.postMessage({
          type: 'processQueuedEvents',
        });
      }, eventInterval);
    });
  }

  render(...args) {
    return this._renderer.render(...args);
  }

  updateVirtualFileSystem(vfs) {
    const valid = typeof vfs === 'object' && vfs !== null;

    invariant(valid, "Virtual file system must be an object mapping string type keys to Array|Float32Array type values");

    Object.keys(vfs).forEach(function(key) {
      const validValue = typeof vfs[key] === 'object' &&
        (Array.isArray(vfs[key]) || (vfs[key] instanceof Float32Array));

      invariant(validValue, "Virtual file system must be an object mapping string type keys to Array|Float32Array type values");
    });

    this._worklet.port.postMessage({
      type: 'updateSharedResourceMap',
      resources: vfs,
    });
  }

  reset() {
    if (this._worklet) {
      this._worklet.port.postMessage({
        type: 'reset',
      });
    }
  }
}
