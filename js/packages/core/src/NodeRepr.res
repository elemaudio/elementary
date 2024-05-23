// Abstract props type which allows us to essentially treat props as any
@genType
type props

// Explicit casting which maps a generic input to our abstract props type
external asPropsType: 'a => props = "%identity"
external asObjectType: props => Js.t<'a> = "%identity"
external asDict: {..} => Js.Dict.t<'a> = "%identity"

@genType
type rec t = {
  symbol: string,
  hash: int,
  kind: string,
  props: props,
  outputChannel: int,
  children: list<t>,
}

@genType
type shallow = {
  symbol: string,
  hash: int,
  kind: string,
  props: props,
  outputChannel: int,
  generation: ref<int>,
}

// Symbol constant for helping to identify node objects
let symbol = "__ELEM_NODE__"

@genType
let create = (kind, props: 'a, children: array<t>): t => {
  let childrenList = Belt.List.fromArray(children)

  // Here we make sure that a node's hash depends of course on its type, props, and children,
  // but importantly also depends on the outputChannel that we're addressing on each of those
  // children. Without doing that, two different nodes that reference different outputs of the
  // same one child node would hash to the same value even though they represent different signal
  // paths.
  {
    symbol,
    kind,
    hash: HashUtils.hashNode(.
      kind,
      asDict(props),
      Belt.List.map(childrenList, n => HashUtils.mixNumber(. n.hash, n.outputChannel)),
    ),
    props: asPropsType(props),
    outputChannel: 0,
    children: childrenList,
  }
}

@genType
let isNode = (a): bool => {
  // We identify Node-type objects from JavaScript/TypeScript by looking
  // for the symbol property
  switch Js.Types.classify(a) {
    | JSObject(_) =>
      switch Js.Types.classify(a["symbol"]) {
        | JSString(s) => s === symbol
        | _ => false
      }
    | _ => false
  }
}

@genType
let shallowCopy = (node: t): shallow => {
  {
    symbol: node.symbol,
    kind: node.kind,
    hash: node.hash,
    props: asPropsType(Js.Obj.assign(Js.Obj.empty(), asObjectType(node.props))),
    outputChannel: node.outputChannel,
    generation: ref(0),
  }
}
