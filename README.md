<div align="center">
  <img height="120px" src="https://www.elementary.audio/Lockup.svg" alt="Elementary Audio logo" />
  <br /><br />

  [![License](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/elemaudio/elementary/blob/main/LICENSE.md)
  [![CI Status](https://github.com/elemaudio/elementary/actions/workflows/main.yml/badge.svg)](https://github.com/elemaudio/elementary/actions)
  [![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](./CONTRIBUTING.md)

</div>

---

[**Elementary**](https://elementary.audio) is a JavaScript/C++ library for building audio applications.

* **Declarative:** Elementary makes it simple to create interactive audio processes through functional, declarative programming. Describe your audio process as a function of your application state, and Elementary will efficiently update the underlying audio engine as necessary.
* **Dynamic:** Most audio processing frameworks and tools facilitate building static processes. But what happens as your audio requirements change throughout the user journey? Elementary is designed to facilitate and adapt to the dynamic nature of modern audio applications.
* **Portable:** By decoupling the JavaScript API from the underlying audio engine (the "what" from the "how"), Elementary enables writing portable applications. Whether the underlying engine is running in the browser, an audio plugin, or an embedded device, the JavaScript layer remains the same.

## Getting Started

Elementary is designed to be used in a number of different environments, which makes for several different ways to get started. If you're new to the project, we recommend
starting with one of the following workflows to get the feel of working in Elementary:

* Try the [online playground](https://elementary.audio/play/) for the fastest way to start making sound
* Use the [web-renderer](https://www.elementary.audio/docs/packages/web-renderer) with your favorite frontend UI library to make an audio web application
* Use the [offline-renderer](https://www.elementary.audio/docs/packages/offline-renderer) with Node.js for static file processing

Once you're comfortable with the basics, you can dive deeper into integrating and extending Elementary with your existing stack:

* Try the simple [command line tool](https://github.com/elemaudio/elementary/tree/develop/cli) here in this repository to explore an example native integration
* Check out the [Native Integrations](https://www.elementary.audio/docs/native-integrations) guide for embedding Elementary's C++ engine in your own native code
* Read the [Custom Native Nodes](https://www.elementary.audio/docs/custom-native-nodes) guide for extending Elementary's built-in DSP library with your own low-level processors

## Documentation

You can find the Elementary documentation [on the website](https://elementary.audio/), or if you like, you can jump straight into:

* [Guides](https://elementary.audio/guides/)
* [Tutorials](https://elementary.audio/tutorials/)
* [API Reference](https://elementary.audio/reference/)

If you need help, join the community on the [Elementary Audio Discord](https://discord.gg/xSu9JjHwYc) and ask any questions you have!

## Contributing

TODO

## License

Elementary is [MIT licensed](./LICENSE.md).
