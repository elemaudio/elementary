import invariant from "invariant";

import { EventEmitter, Renderer } from "@elemaudio/core";

// Injected at build time
const pkgVersion = process.env.PKG_VERSION;

export default class WebRenderer extends EventEmitter {
  private _worklet: any;
  private _promiseMap: any;
  private _nextRequestId: number;
  private _renderer: Renderer;
  private _timer: any;

  public context: BaseAudioContext = null;

  async initialize(
    audioContext: BaseAudioContext,
    workletOptions: AudioWorkletNodeOptions = {},
    eventInterval: number = 16,
  ): Promise<AudioWorkletNode> {
    invariant(
      typeof audioContext === "object" && audioContext !== null,
      "First argument to initialize must be a valid AudioContext instance.",
    );
    invariant(
      typeof workletOptions === "object" && workletOptions !== null,
      "The optional second argument to initialize must be an object.",
    );

    // Hold a reference here for easier access to the underlying AudioContext
    this.context = audioContext;

    // A bit of a hack here, but if we try to register the same ElementaryAudioWorkletProcessor on the same
    // AudioContext twice, the browser will throw an error. We don't have any way to ask the AudioContext what
    // worklets it has already registered, so our only option here is to keep some state around to check for
    // ourselves. Writing to the AudioContext like this is hacky, but it works. An alternative would be keeping
    // our own Map<BaseAudioContext, boolean> but that feels like overkill.
    //
    // @ts-ignore
    if (typeof audioContext._elemWorkletRegistry !== "object") {
      // @ts-ignore
      audioContext._elemWorkletRegistry = {};
    }

    // @ts-ignore
    const workletRegistry = audioContext._elemWorkletRegistry;

    if (!workletRegistry.hasOwnProperty(pkgVersion)) {
      if (!audioContext.audioWorklet) {
        throw new Error(
          "BaseAudioContext.audioWorklet is missing; are you running in a secure context (https)?",
        );
      }

      // This resolves to the dist directory at runtime
      const workletScriptUrl = new URL("./index.worklet.js", import.meta.url);
      await audioContext.audioWorklet.addModule(workletScriptUrl);

      workletRegistry[pkgVersion] = true;
    }

    this._promiseMap = new Map();
    this._nextRequestId = 0;

    const { processorOptions = {}, ...otherOptions } = workletOptions;

    const wasmBinaryUrl =
      processorOptions.wasmBinaryUrl ??
      new URL("./elementary-wasm.wasm", import.meta.url);

    const wasmBinary = await fetch(wasmBinaryUrl).then((response) =>
      response.arrayBuffer(),
    );

    this._worklet = new AudioWorkletNode(
      audioContext,
      `ElementaryAudioWorkletProcessor@${pkgVersion}`,
      Object.assign(
        {
          numberOfInputs: 0,
          numberOfOutputs: 1,
          outputChannelCount: [2],
          processorOptions: Object.assign(
            {
              wasmBinary,
            },
            processorOptions,
          ),
        },
        otherOptions,
      ),
    );

    // We defer the resolution of this method's result until we get the load
    // event back from the worklet. That way, if the user is awaiting the result
    // of the initialize event before calling `render`, we guarantee that we've
    // actually created our Renderer instance here in time.
    return await new Promise((resolve, reject) => {
      this._worklet.port.onmessage = (e) => {
        const [type, payload] = e.data;

        if (type === "load") {
          this._renderer = new Renderer(async (batch) => {
            return await this._sendWorkletRequest("renderInstructions", {
              batch,
            });
          });

          // TODO: Clean up? Unsubscribe option?
          this._timer = window.setInterval(() => {
            this._worklet.port.postMessage({
              requestType: "processQueuedEvents",
            });
          }, eventInterval);

          resolve(this._worklet);
          return this.emit(type, payload);
        }

        if (type === "events") {
          return payload.forEach((e) => {
            this.emit(e.type, e.event);
          });
        }

        if (type === "reply") {
          const { requestId, result } = payload;
          const { resolve, reject } = this._promiseMap.get(requestId);

          this._promiseMap.delete(requestId);

          return resolve(result);
        }
      };
    });
  }

  _sendWorkletRequest(requestType, payload) {
    invariant(
      this._worklet,
      "Can't send request before worklet is ready. Have you initialized your WebRenderer instance?",
    );

    let requestId = this._nextRequestId++;

    this._worklet.port.postMessage({
      requestId,
      requestType,
      payload,
    });

    return new Promise((resolve, reject) => {
      this._promiseMap.set(requestId, { resolve, reject });
    });
  }

  createRef(kind, props, children) {
    return this._renderer.createRef(kind, props, children);
  }

  async render(...args) {
    const { result, ...stats } = await this._renderer.render(...args);

    if (!result.success) {
      return Promise.reject(result);
    }

    return Promise.resolve(stats);
  }

  async updateVirtualFileSystem(vfs) {
    const valid = typeof vfs === "object" && vfs !== null;

    invariant(
      valid,
      "Virtual file system must be an object mapping string type keys to Array<Float32Array> | Float32Array type values",
    );

    Object.keys(vfs).forEach(function (key) {
      const validValue =
        typeof vfs[key] === "object" &&
        (Array.isArray(vfs[key]) || vfs[key] instanceof Float32Array);

      invariant(
        validValue,
        "Virtual file system must be an object mapping string type keys to Array<Float32Array> | Float32Array type values",
      );
    });

    return await this._sendWorkletRequest("updateSharedResourceMap", {
      resources: vfs,
    });
  }

  async pruneVirtualFileSystem() {
    return await this._sendWorkletRequest("pruneVirtualFileSystem", {});
  }

  async listVirtualFileSystem() {
    return await this._sendWorkletRequest("listVirtualFileSystem", {});
  }

  async reset() {
    return await this._sendWorkletRequest("reset", {});
  }

  async gc() {
    let pruned = await this._sendWorkletRequest("gc", {});
    this._renderer.prune(pruned);
    return pruned;
  }

  async setCurrentTime(t) {
    return await this._sendWorkletRequest("setCurrentTime", {
      time: t,
    });
  }

  async setCurrentTimeMs(t) {
    return await this._sendWorkletRequest("setCurrentTimeMs", {
      time: t,
    });
  }

  async setBeatTime(t: number) {
    return await this._sendWorkletRequest("setBeatTime", {
      time: t,
    });
  }

  async setBpm(bpm: number) {
    return await this._sendWorkletRequest("setBpm", {
      bpm: bpm,
    });
  }

  async setTimeSignature(numerator: number, denominator: number) {
    return await this._sendWorkletRequest("setTimeSignature", {
      numerator: numerator,
      denominator: denominator,
    });
  }

  async pushMidiEvent(time, value) {
    return await this._sendWorkletRequest("pushMidiEvent", {
      time,
      value,
    });
  }

  async pushParamValueEvent(time: number, index: number, value: number) {
    return await this._sendWorkletRequest("pushParamValueEvent", {
      time,
      index,
      value,
    });
  }
}
