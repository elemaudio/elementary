<div align="center">
  <img height="120px" src="https://www.elementary.audio/Lockup.svg" alt="Elementary Audio logo" />
  <br /><br />

  [![License](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/elemaudio/elementary/blob/main/LICENSE.md)
  [![CI Status](https://github.com/elemaudio/elementary/actions/workflows/main.yml/badge.svg)](https://github.com/elemaudio/elementary/actions)
  ![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)

</div>

---

[**Elementary**](https://elementary.audio) is a JavaScript/C++ library for building audio applications.

* **Declarative:** Elementary makes it simple to create interactive audio processes through functional, declarative programming. Describe your audio process as a function of your application state, and Elementary will efficiently update the underlying audio engine as necessary.
* **Dynamic:** Most audio processing frameworks and tools facilitate building static processes. But what happens as your audio requirements change throughout the user journey? Elementary is designed to facilitate and adapt to the dynamic nature of modern audio applications.
* **Portable:** By decoupling the JavaScript API from the underlying audio engine (the "what" from the "how"), Elementary enables writing portable applications. Whether the underlying engine is running in the browser, an audio plugin, or an embedded device, the JavaScript layer remains the same.

## Getting Started

Elementary is designed to be used in a number of different environments, which makes for several different ways to get started. If you're new to the project, we recommend
starting with one of the following workflows to get the feel of working in Elementary:

* Use the [web-renderer](https://www.elementary.audio/docs/packages/web-renderer) with your favorite frontend UI library to make an audio web application
* Use the [offline-renderer](https://www.elementary.audio/docs/packages/offline-renderer) with Node.js for static file processing

Once you're comfortable with the basics, you can dive deeper into integrating and extending Elementary with your existing stack:

* Try the small [command line tool](https://github.com/elemaudio/elementary/tree/main/cli) here in this repository to explore an example native integration
* Check out the [Native Integrations](https://www.elementary.audio/docs/guides/Native_Integrations) guide for embedding Elementary's C++ engine in your own native code
* Read the [Custom Native Nodes](https://www.elementary.audio/docs/guides/Custom_Native_Nodes) guide for extending Elementary's built-in DSP library with your own low-level processors

## Documentation

You can find the Elementary documentation [on the website](https://elementary.audio/), or if you like, you can jump straight into:

* [Guides](https://elementary.audio/docs/guides/Making_Sound/)
* [Tutorials](https://www.elementary.audio/resources)
* [API Reference](https://elementary.audio/docs/reference)

If you need help, join the community on the [Elementary Audio Discord](https://discord.gg/xSu9JjHwYc) and ask any questions you have!

## Contributing

We'd love to get you involved! Right now, the primary focus is to promote and grow the Elementary community. So if you want to help, consider:

* Making an open source project with Elementary, like [this one](https://github.com/bgins/coincident-spectra) or [this one](https://github.com/teetow/elementary_grid)
* Writing your own higher-level modules and sharing them on npm, [like this](https://github.com/nick-thompson/drumsynth/)
* Writing your own custom native nodes to extend the engine and sharing them on GitHub

If you're more interested in digging in under the hood, check out the [open issues](https://github.com/elemaudio/elementary/issues) to see where we need help right now. Note that while we're
focusing on the community around Elementary, the immediate goals in this repository are ease of use, extensibility, and performance. We will likely be
careful and slow to add new features, so if there's a particular feature you want to see here, please [open an issue](https://github.com/elemaudio/elementary/issues/new) to start a conversation around your
proposal first.

## License

Elementary is [MIT licensed](./LICENSE.md).
