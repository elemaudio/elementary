// We use this to shim a crypto library in the WebAudioWorklet environment, which
// is necessary for Emscripten's implementation of std::random_device, which we need
// after pulling in signalsmith-stretch.
//
// I've seen some issues on esmcripten that seem to suggest an insecure polyfill like
// this should already be provided, but I'm getting runtime assertion errors that it's
// not available. An alternative solution would be to upstream a change to the stretch
// library to rely on an injected random device in C++ territory, so that for our case
// we could inject some simple PRNG.
globalThis.crypto = {
  getRandomValues: (array) => {
    for (var i = 0; i < array.length; i++) {
      array[i] = (Math.random() * 256) | 0;
    }
  }
};
