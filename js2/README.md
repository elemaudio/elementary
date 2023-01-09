# @elemaudio npm packages

This directory manages the main `@elemaudio` packages available on npm. We do so
using Lerna from this directory root with each package listed in the `packages/`
directory.

We use Lerna's idea of versioning and publishing to manage our packages, with versioning
done locally and publishing automatically from CI via Github Actions.

To mark a new version:

```bash
npx lerna version --no-push
```

Finally, from Github Actions we have a publish action which runs any time a tag
is pushed to the remote. We use Lerna's `from-git` feature to seek out those tags
and publish the associated packages.

```bash
lerna publish from-git
```
