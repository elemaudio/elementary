module NodeRepr = {

  // Abstract props type which allows us to essentially treat props as any
  @genType
  type props

  // Explicit casting which maps a generic input to our abstract props type
  external asPropsType: 'a => props = "%identity"
  external asObjectType: props => Js.t<'a> = "%identity"

  type renderContext = {
    sampleRate: int,
    blockSize: int,
    numInputs: int,
    numOutputs: int,
  }

  // GenType doesn't seem to support the ES6 symbol type out of the box, so
  // we have it defined in a neighboring file and import the type here
  @genType.import("./Symbol")
  type symbol

  @genType
  type rec t = {
    symbol: symbol,
    mutable hash?: int,
    kind: [
      | #Primitive(string)
      | #Composite(ref<option<t>>, ({"context": renderContext, "props": props, "children": array<t>}) => t)
    ],
    props: props,
    children: list<t>,
  }

  @genType
  type shallow = {
    symbol: symbol,
    hash: int,
    kind: string,
    props: props,
    generation: ref<int>,
  }

  // Symbol constant for helping to identify node objects
  let symbol = %raw("Symbol.for('ELEM_NODE')")

  @genType
  let createPrimitive = (kind, props, children: array<t>): t => {
    {symbol: symbol, kind: #Primitive(kind), props: asPropsType(props), children: Belt.List.fromArray(children)}
  }

  @genType
  let createComposite = (fn, props, children: array<t>): t => {
    {symbol: symbol, kind: #Composite(ref(None), fn), props: asPropsType(props), children: Belt.List.fromArray(children)}
  }

  @genType
  let isNode = (a): bool => {
    // We quickly identify Node-type objects from JavaScript/TypeScript by looking
    // for the symbol property.
    switch Js.Types.classify(a) {
      | JSObject(_) =>
        switch Js.Types.classify(a["symbol"]) {
          | JSSymbol(s) => s === symbol
          | _ => false
        }
      | _ => false
    }
  }

  @genType
  let getHashUnchecked = (n: t): int => {
    switch n.hash {
      | Some(x) => x
      | None => Js.Exn.raiseError("Missing hash property")
    }
  }

  @genType
  let shallowCopy = (node: t): shallow => {
    switch node.kind {
      | #Composite(_) => Js.Exn.raiseError("Attempting to shallow copy a composite node")
      | #Primitive(k) => {
        {
          symbol: symbol,
          kind: k,
          hash: getHashUnchecked(node),
          props: asPropsType(Js.Obj.assign(Js.Obj.empty(), asObjectType(node.props))),
          generation: ref(0),
        }
      }
    }
  }
}

// External interface for ES6 Map
module Map = {
  type t<'a, 'b>

  @new external make: unit => t<'a, 'b> = "Map"

  @send external has: (t<'a, 'b>, 'a) => bool = "has"
  @send external get: (t<'a, 'b>, 'a) => 'b = "get"
  @send external delete: (t<'a, 'b>, 'a) => () = "delete"
  @send external set: (t<'a, 'b>, 'a, 'b) => () = "set"
  @send external values: (t<'a, 'b>) => Js.Array2.array_like<'b> = "values"

  let valuesArray = (m: t<'a, 'b>): array<'b> => {
    Js.Array2.from(values(m))
  }
}

// External interface for ES6 Set
module Set = {
  type t<'a>

  @new external make: unit => t<'a> = "Set"

  @send external has: (t<'a>, 'a) => bool = "has"
  @send external add: (t<'a>, 'a) => () = "add"
}

// External interface for the RendererDelegate instance that we receive in renderWithDelegate
module RenderDelegate = {
  type t

  // Maps hashes to node objects
  @send external getNodeMap: t => Map.t<int, NodeRepr.shallow> = "getNodeMap"

  // Maps composite function instances to memoized composite function instances
  @send external getMemoMap: t => Map.t<'a, 'b> = "getMemoMap"

  @send external getRenderContext: t => NodeRepr.renderContext = "getRenderContext"
  @send external getActiveRoots: t => array<int> = "getActiveRoots"
  @send external getTerminalGeneration: t => int = "getTerminalGeneration"

  @send external createNode: (t, int, string) => () = "createNode"
  @send external deleteNode: (t, int) => () = "deleteNode"
  @send external appendChild: (t, int, int) => () = "appendChild"
  @send external setProperty: (t, int, string, 'a) => () = "setProperty"
  @send external activateRoots: (t, array<int>) => () = "activateRoots"
  @send external commitUpdates: t => () = "commitUpdates"
}

let getOrCreateMemo = (memoMap, fn) => {
  if Map.has(memoMap, fn) {
    Map.get(memoMap, fn)
  } else {
    let memTable = Map.make()
    let memoized = (lookupKey: int, context: NodeRepr.renderContext, props: NodeRepr.props, children: list<NodeRepr.t>) => {
      if Map.has(memTable, lookupKey) {
        Map.get(memTable, lookupKey)
      } else {
        let resolved = fn({
          "context": context,
          "props": props,
          "children": Belt.List.toArray(children),
        })

        Map.set(memTable, lookupKey, resolved)
        resolved
      }
    }

    Map.set(memoMap, fn, memoized)
    memoized
  }
}

let mount = (
  delegate: RenderDelegate.t,
  node: NodeRepr.t,
  kind: string,
  hash: int,
  childHashes: list<int>,
) => {
  let nodeMap = RenderDelegate.getNodeMap(delegate)

  if Map.has(nodeMap, hash) {
    let existing = Map.get(nodeMap, hash)
    HashUtils.updateNodeProps(delegate, hash, existing.props, node.props)

    Map.set(nodeMap, hash, NodeRepr.shallowCopy(node))
  } else {
    RenderDelegate.createNode(delegate, hash, kind)
    HashUtils.updateNodeProps(delegate, hash, Js.Obj.empty(), node.props)

    Belt.List.forEach(childHashes, ch => {
      RenderDelegate.appendChild(delegate, hash, ch)
    })

    Map.set(nodeMap, hash, NodeRepr.shallowCopy(node))
  }
}

let rec visit = (
  delegate: RenderDelegate.t,
  visitSet: Set.t<NodeRepr.t>,
  compositeMap: Map.t<NodeRepr.t, NodeRepr.t>,
  ns: list<NodeRepr.t>,
) => {
  let visited = (x: NodeRepr.t) => Set.has(visitSet, x)

  switch ns {
  | list{} => ()
  | list{n, ...rest} if visited(n) => visit(delegate, visitSet, compositeMap, rest)
  | list{n, ...rest} => {
      let childrenVisited = n.children->Belt.List.every(visited)

      // If our children haven't yet been visited, push them onto the list
      // and reinsert `n` to be visited afterward
      if !childrenVisited {
        visit(delegate, visitSet, compositeMap, Belt.List.concat(n.children, ns))
      } else {
        let childHashes = Belt.List.map(n.children, child => Js.Option.getExn(child.hash))

        switch n.kind {
          | #Composite(res, fn) => {
            // Here we're visiting a composite node. If we've already seen the node its function
            // should be resolved, which we find in `res`. Else, we have to unroll it now
            let context = RenderDelegate.getRenderContext(delegate)
            let resolved = switch res.contents {
              | Some(n) => n
              | None => fn({
                "context": context,
                "props": n.props,
                "children": Belt.List.toArray(n.children),
              })
            }

            res := Some(resolved)

            // If we've already visited the resolved sub-tree, we can mark the composite node itself
            // with the resolved hash and mark visited. Otherwise, we push both the resolved sub-tree
            // and the composite node itself back onto the visit list
            if (visited(resolved)) {
              n.hash = Some(Js.Option.getExn(resolved.hash))
              Set.add(visitSet, n)
              visit(delegate, visitSet, compositeMap, rest)
            } else {
              visit(delegate, visitSet, compositeMap, Belt.List.add(Belt.List.add(rest, n), resolved))
            }
          }
          | #Primitive(k) => {
            // Else we've got a primitive node that needs to be hashed and mounted before
            // visiting the remaining nodes
            let hash = HashUtils.hashNode(. k, NodeRepr.asObjectType(n.props), childHashes)

            n.hash = Some(hash)
            mount(delegate, n, k, hash, childHashes)
            Set.add(visitSet, n)

            visit(delegate, visitSet, compositeMap, rest)
          }
        }
      }
    }
  }
}

@genType
let renderWithDelegate = (delegate, graphs) => {
  let visitSet = Set.make()
  let compositeMap = Map.make()
  let roots = Belt.List.mapWithIndex(Belt.List.fromArray(graphs), (i, g) => {
    NodeRepr.createPrimitive("root", NodeRepr.asPropsType({"channel": i}), [g])
  })

  visit(delegate, visitSet, compositeMap, roots)

  RenderDelegate.activateRoots(delegate, Belt.List.toArray(Belt.List.map(roots, r => NodeRepr.getHashUnchecked(r))))
  RenderDelegate.commitUpdates(delegate)
}

@genType
let stepGarbageCollector = (delegate: RenderDelegate.t): () => {
  let nodeMap = RenderDelegate.getNodeMap(delegate)
  let term = RenderDelegate.getTerminalGeneration(delegate)

  // We accumulate a list of nodes for which the deleteNode instruction was
  // issued so that we can remove them from the node map
  // The GC pass is simple: we iterate all known nodes and increment their generation.
  // Any subsequent render pass which touches existing nodes will reset their generation
  // back to 0. Nodes which are not revisited will get older and older until they exceed
  // the threshold, at which point we remove them.
  //
  // Here we accumulate a list of all such nodes for which we gave the deleteNode instruction
  // so that we can subsequently prune the nodeMap
  let deleted = Js.Array2.reduce(Map.valuesArray(nodeMap), (acc, n) => {
    n.generation := n.generation.contents + 1

    if (n.generation.contents >= term) {
      RenderDelegate.deleteNode(delegate, n.hash)
      Belt.List.add(acc, n)
    } else {
      acc
    }
  }, list{});

  if (Belt.List.length(deleted) > 0) {
    RenderDelegate.commitUpdates(delegate)
    Belt.List.forEach(deleted, n => Map.delete(nodeMap, n.hash));
  }
}
