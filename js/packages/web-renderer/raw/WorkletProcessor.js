const EventTypes = {
  CREATE_NODE: 0,
  DELETE_NODE: 1,
  APPEND_CHILD: 2,
  SET_PROPERTY: 3,
  ACTIVATE_ROOTS: 4,
  COMMIT_UPDATES: 5,
  UPDATE_RESOURCE_MAP: 6,
  RESET: 7,
};


// A recursive function looking for transferable objects per the Web Worker API
// @see https://developer.mozilla.org/en-US/docs/Web/API/Web_Workers_API/Transferable_objects
//
// Right now we're only looking for the ArrayBuffers behind Float32Array instances as that's
// the only type of transferable object that the native engine delivers, but this could be
// extended to other types easily.
function findTransferables(val, transferables = []) {
  if (val instanceof Float32Array) {
    transferables.push(val.buffer);
  } else if (typeof val === 'object') {
    if (Array.isArray(val)) {
      for (let i = 0; i < val.length; ++i) {
        findTransferables(val[i], transferables);
      }
    } else if (val !== null) {
      for (let key of Object.keys(val)) {
        findTransferables(val[key], transferables);
      }
    }
  }

  return transferables;
}

class ElementaryAudioWorkletProcessor extends AudioWorkletProcessor {
  constructor(options) {
    super(options);

    const numInputChannels = options.numberOfInputs;
    const numOutputChannels = options.outputChannelCount.reduce((acc, next) => acc + next, 0);

    this._module = Module();
    this._native = new this._module.ElementaryAudioProcessor(numInputChannels, numOutputChannels);

    // The `sampleRate` variable is a globally defined constant in the AudioWorkletGlobalScope.
    // We also manually set a block size of 128 samples here, per the Web Audio API spec.
    //
    // See: https://webaudio.github.io/web-audio-api/#rendering-loop
    this._native.prepare(sampleRate, 128);

    const hasProcOpts = options.hasOwnProperty('processorOptions') &&
      typeof options.processorOptions === 'object' &&
      options.processorOptions !== null;

    if (hasProcOpts) {
      const {virtualFileSystem, ...other} = options.processorOptions;

      const validVFS = typeof virtualFileSystem === 'object' &&
        virtualFileSystem !== null &&
        Object.keys(virtualFileSystem).length > 0;

      if (validVFS) {
        for (let [key, val] of Object.entries(virtualFileSystem)) {
          this._native.updateSharedResourceMap(key, val, (message) => {
            this.port.postMessage(['error', message]);
          });
        }
      }
    }

    this.port.onmessage = (e) => {
      let {requestId, requestType, payload} = e.data;

      switch (requestType) {
        case 'processQueuedEvents':
          this._native.processQueuedEvents((evtBatch) => {
            if (evtBatch.length > 0) {
              let transferables = findTransferables(evtBatch);
              this.port.postMessage(['eventBatch', evtBatch], transferables);
            }
          });

          break;
        case 'renderInstructions':
          return this.port.postMessage(['reply', {
            requestId,
            result: this._native.postMessageBatch(payload.batch),
          }]);
        case 'updateSharedResourceMap':
          for (let [key, val] of Object.entries(payload.resources)) {
            this._native.updateSharedResourceMap(key, val, (message) => {
              this.port.postMessage(['error', message]);
            });
          }

          return this.port.postMessage(['reply', {
            requestId,
            result: null,
          }]);
        case 'reset':
          this._native.reset();

          return this.port.postMessage(['reply', {
            requestId,
            result: null,
          }]);
        case 'pruneVirtualFileSystem':
          this._native.pruneSharedResourceMap();

          return this.port.postMessage(['reply', {
            requestId,
            result: null,
          }]);
        case 'listVirtualFileSystem':
          return this.port.postMessage(['reply', {
            requestId,
            result: this._native.listSharedResourceMap(),
          }]);
        case 'setCurrentTime':
          return this.port.postMessage(['reply', {
            requestId,
            result: this._native.setCurrentTime(payload.time),
          }]);
        case 'setCurrentTimeMs':
          return this.port.postMessage(['reply', {
            requestId,
            result: this._native.setCurrentTimeMs(payload.time),
          }]);
        default:
          break;
      }
    };

    this.port.postMessage(['load', {
      sampleRate,
      blockSize: 128,
      numInputChannels,
      numOutputChannels,
    }]);
  }

  process (inputs, outputs, parameters) {
    if (inputs.length > 0) {
      let m = 0;

      // For each input
      for (let i = 0; i < inputs.length; ++i) {
        // For each channel on this input
        for (let j = 0; j < inputs[i].length; ++j) {
          const internalInputData = this._native.getInputBufferData(m++);

          // For each sample on this input channel
          for (let k = 0; k < inputs[i][j].length; ++k) {
            internalInputData[k] = inputs[i][j][k];
          }
        }
      }
    }

    const numSamples = (outputs.length > 0 && outputs[0].length > 0)
      ? outputs[0][0].length
      : 0;

    this._native.process(numSamples);

    if (outputs.length > 0) {
      let m = 0;

      // For each output
      for (let i = 0; i < outputs.length; ++i) {
        // For each channel on this output
        for (let j = 0; j < outputs[i].length; ++j) {
          const internalOutputData = this._native.getOutputBufferData(m++);

          // For each sample on this input channel
          for (let k = 0; k < outputs[i][j].length; ++k) {
            outputs[i][j][k] = internalOutputData[k];
          }
        }
      }
    }

    // Tells the browser to keep this node alive and continue calling process
    return true;
  }
}

registerProcessor(`ElementaryAudioWorkletProcessor@${__PKG_VERSION__}`, ElementaryAudioWorkletProcessor);
