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

  @send external getActiveRoots: t => array<int> = "getActiveRoots"

  @send external createNode: (t, int, string) => () = "createNode"
  @send external deleteNode: (t, int) => () = "deleteNode"
  @send external appendChild: (t, int, int, int) => () = "appendChild"
  @send external setProperty: (t, int, string, 'a) => () = "setProperty"
  @send external activateRoots: (t, array<int>) => () = "activateRoots"
  @send external commitUpdates: t => () = "commitUpdates"
}

let mount = (delegate: RenderDelegate.t, node: NodeRepr.t) => {
  let nodeMap = RenderDelegate.getNodeMap(delegate)

  if !Map.has(nodeMap, node.hash) {
    RenderDelegate.createNode(delegate, node.hash, node.kind)
    HashUtils.updateNodeProps(delegate, node.hash, Js.Obj.empty(), node.props)

    Belt.List.forEach(node.children, child => {
      RenderDelegate.appendChild(delegate, node.hash, child.hash, child.outputChannel)
    })

    Map.set(nodeMap, node.hash, NodeRepr.shallowCopy(node))
  } else {
    let existing = Map.get(nodeMap, node.hash)
    HashUtils.updateNodeProps(delegate, existing.hash, existing.props, node.props)
  }
}

let rec visit = (
  delegate: RenderDelegate.t,
  visitSet: Set.t<int>,
  ns: list<NodeRepr.t>,
) => {
  let visited = (n: NodeRepr.t) => Set.has(visitSet, n.hash)
  let markVisited = (n: NodeRepr.t) => Set.add(visitSet, n.hash)

  // As of v3, every node in the graph is hashed at the time it's created, which
  // means that we can perform our reconciliation here in a pre-order visit fashion
  // as long as the RenderDelegate makes sure to deliver all createNode instructions
  // before it delivers any appendChild instructions.
  //
  // Pushing that requirement off to the RenderDelegate drastically simplifies this
  // graph traversal step
  switch ns {
  | list{} => ()
  | list{n, ...rest} if visited(n) => visit(delegate, visitSet, rest)
  | list{n, ...rest} => {
      markVisited(n)
      mount(delegate, n)
      visit(delegate, visitSet, Belt.List.concat(n.children, rest))
    }
  }
}

@genType
let renderWithDelegate = (delegate, graphs, rootFadeInMs, rootFadeOutMs) => {
  let visitSet = Set.make()
  let roots = Belt.List.mapWithIndex(Belt.List.fromArray(graphs), (i, g) => {
    NodeRepr.create("root", {"channel": i, "fadeInMs": rootFadeInMs, "fadeOutMs": rootFadeOutMs}, [g])
  })

  visit(delegate, visitSet, roots)
  
  RenderDelegate.activateRoots(delegate, Belt.List.toArray(Belt.List.map(roots, r => r.hash)))
  RenderDelegate.commitUpdates(delegate)
}
