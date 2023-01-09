# @elemaudio/core

The official Elementary Audio core package; this package provides the standard library for composing
audio processing nodes, as well as utilities for constructing and addressing composite nodes.

This package will generally be used in combination with one of the available renderers. Please see the full
documentation at [https://www.elementary.audio/docs](https://www.elementary.audio/docs)

## Installation

```js
npm install --save @elemaudio/core
```

## Usage

```js
import {
  el,
  createNode,
  isNode,
  resolve,
} from '@elemaudio/core';
```

### el

A plain object through which you can access all of the available standard library nodes. For
documentation on exactly which nodes are available and what they do, see the [Core Library Reference](https://www.elementary.audio/docs/reference).

### createNode

```js
function createNode(kind: string | Function, props: Object, children: array<NodeRepr_t | number>): NodeRepr_t;
```

A factory function for creating an audio node, `NodeRepr_t`. Every function available on `el` ultimately decays
to a series of calls to `createNode`.

Typically, you'll only need to pay attention to this API for creating "Composite" nodes, which we discuss in
[Understanding Memoization](https://www.elementary.audio/docs/guides/Understanding_Memoization). In that case, you'll pass a function to `createNode`
along with a set of props and children with which your function will be invoked during the render process.

```js
// An example composite node which composes over two series filters
function myFilterComposite({props, children}) {
  return el.lowpass(
    props.cutoff,
    0.707,
    el.peak(props.cutoff / 2, 0.707, children[0])
  );
}

// A helper function which feels similar to the `el.*` functions which hides away
// the explicit call to `createNode`.
function myFilter(props, input) {
  return createNode(myFilter, props, input);
}
```

### isNode

```js
function isNode(a: any): bool;
```

A simple utility for identifying if the input argument is of type `NodeRepr_t`. You'll rarely need this,
but it's worth noting especially for TypeScript users that some of the `el.*` library functions have
a return type of `NodeRepr_t | number`, and you may find utility in `isNode` in those scenarios.


### resolve

```js
function resolve(n: NodeRepr_t | number): NodeRepr_t;
```

Very similar to the above `isNode`; this utility accepts an input which is either a `NodeRepr_t` or a `number`
and resolves to a `NodeRepr_t`. Again, you'll rarely need it but perhaps for cases where the `el.*` library
functions yield a `NodeRepr_t | number` type.
