# npm packages

This directory manages the main `@elemaudio` packages available on npm via Lerna.
We use Lerna's idea of versioning and publishing to manage our packages, with versioning
done locally and publishing automatically from CI via Github Actions.

To mark a new version:

```bash
npx lerna version -m 'Publish v2.0.0' --no-push
```

Finally, from Github Actions we have a publish action which runs lerna's `from-package` feature
to push any package versions to the registry that are not already there.

```bash
# See our Github Actions workflow file
npx lerna publish from-package
```
