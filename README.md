<div align="center">
  <img height="120px" src="https://www.elementary.audio/Lockup.svg" alt="Elementary Audio logo" />
  <br /><br />

  [![CI Status](https://github.com/elemaudio/elementary/actions/workflows/main.yml/badge.svg)](https://github.com/elemaudio/elementary/actions)
  [![License](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/elemaudio/elementary/blob/main/LICENSE.md)
  [![Discord Community](https://img.shields.io/discord/826071713426178078?label=Discord)](https://discord.gg/xSu9JjHwYc)
  [![npm installs](https://img.shields.io/npm/dt/%40elemaudio/core?label=npm%20installs&color=%23f472b6)](https://www.npmjs.com/package/@elemaudio/core)

</div>

---

[**Elementary**](https://elementary.audio) is a JavaScript library for digital audio signal processing.

* **Declarative:** Elementary makes it simple to create interactive audio processes through functional, declarative programming. Describe your audio process as a function of your application state, and Elementary will efficiently update the underlying audio engine as necessary.
* **Dynamic:** Most audio processing frameworks and tools facilitate building static processes. But what happens as your audio requirements change throughout the user journey? Elementary is designed to facilitate and adapt to the dynamic nature of modern audio applications.
* **Portable:** By decoupling the JavaScript API from the underlying audio engine (the "what" from the "how"), Elementary enables writing portable applications. Whether the underlying engine is running in the browser, an audio plugin, or an embedded device, your JavaScript layer remains the same.

## Getting Started

Every Elementary application starts with the [@elemaudio/core](https://www.elementary.audio/docs/packages/core) package ([npm](https://www.npmjs.com/package/@elemaudio/core), [source](./js/packages/core)), which provides the
framework for defining your audio processes and a generic set of utilities for performing the graph rendering and reconciling steps.

Next, because Elementary is designed to be used in a number of different environments, there are several different ways to integrate.
If you're new to the project, we recommend studying the following workflows to get the feel of working in Elementary:

* Use the [@elemaudio/web-renderer](https://www.elementary.audio/docs/packages/web-renderer) package ([npm](https://www.npmjs.com/package/@elemaudio/web-renderer), [source](./js/packages/web-renderer)) with your favorite frontend UI library to make an audio web application
* Use the [@elemaudio/offline-renderer](https://www.elementary.audio/docs/packages/offline-renderer) package ([npm](https://www.npmjs.com/package/@elemaudio/offline-renderer), [source](./js/packages/offline-renderer)) with Node.js for static file processing

Once you're ready to dive in, we suggest starting with one of these ideas:

* Jump into the [online playground](https://www.elementary.audio/playground) for the quickest way of experimenting with sound
* Make an audio effects plugin following the [SRVB plugin template](https://github.com/elemaudio/srvb)
* Try the small [command line tool](https://github.com/elemaudio/elementary/tree/main/cli) here in this repository to explore an example native integration
* Check out the [Native Integrations](https://www.elementary.audio/docs/guides/Native_Integrations) guide for embedding Elementary's C++ engine in your own native code
* Read the [Custom Native Nodes](https://www.elementary.audio/docs/guides/Custom_Native_Nodes) guide for extending Elementary's built-in DSP library with your own low-level processors

## Documentation

You can find the Elementary documentation [on the website](https://elementary.audio/), or if you like, you can jump straight into:

* [Guides](https://elementary.audio/docs/guides/Making_Sound/)
* [Tutorials](https://www.elementary.audio/docs/tutorials/distortion-saturation-wave-shaping)
* [Project Showcase](https://www.elementary.audio/showcase)

If you need help, join the community on the [Elementary Audio Discord](https://discord.gg/xSu9JjHwYc) and ask any questions you have!

## Releases

Releases are made both here in GitHub and deployed to [npm](https://www.npmjs.com) following npm's semver conventions. Because your project may need to coordinate both
the native C++ library and the corresponding JavaScript dependencies, it may be helpful to understand our release conventions.

* Canary releases are always made first on the `develop` branch using git tags
    * Corresponding JavaScript packages are deployed to npm tagged `next` to indicate their status
* After a brief validation period, canary releases are promoted to `main` via git merge
    * The corresponding JavaScript packages are then tagged `latest`, making them the default install candidates on npm
    * At this time a new GitHub Release is written to mark the update and document the changelog

As a general rule of thumb, we recommend integrating Elementary via the `main` branch of this repo and the corresponding `latest` packages on npm. If you
want to target the more bleeding edge, you can integrate via the git tags on the `develop` branch and the corresponding `next` packages on npm.

## Contributing

We'd love to get you involved! Right now, the primary focus is to promote and grow the Elementary community. So if you want to help, consider:

* Making an open source project with Elementary, like [this one](https://github.com/bgins/coincident-spectra) or [this one](https://github.com/teetow/elementary_grid)
* Writing your own higher-level modules and sharing them on npm, [like this](https://github.com/nick-thompson/drumsynth/)
* Writing your own custom native nodes to extend the engine and sharing them on GitHub

If you're more interested in digging in under the hood, check out the [open issues](https://github.com/elemaudio/elementary/issues) to see where we need help right now. Note that while we're
focusing on the community around Elementary, the immediate goals in this repository are ease of use, extensibility, and performance. We will likely be
careful and slow to add new features, so if there's a particular feature you want to see here, please [open an issue](https://github.com/elemaudio/elementary/issues/new) to start a conversation around your
proposal first.

If you wish to become involved in the project beyond development, you could [become a sponsor](https://github.com/sponsors/nick-thompson). This form of assistance is deeply valued and instrumental in propelling the project forward. We are genuinely grateful for any support you choose to provide.

## License

Elementary is [MIT licensed](./LICENSE.md).
