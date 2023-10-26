// Rollup's replace plugin is apparently weird about injecting strings in typescript
// files, so we've got a quick hack here to export the injected string from a simple
// JavaScript module.
export const pkgVersion = __PKG_VERSION__;
