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
  children: list<t>,
}

@genType
type shallow = {
  symbol: string,
  hash: int,
  kind: string,
  props: props,
  generation: ref<int>,
}

// Symbol constant for helping to identify node objects
let symbol = "__ELEM_NODE__"

@genType
let create = (kind, props: 'a, children: array<t>): t => {
  let childrenList = Belt.List.fromArray(children)
  let childHashes = Belt.List.map(childrenList, n => n.hash)

  {
    symbol: symbol,
    kind: kind,
    hash: HashUtils.hashNode(. kind, asDict(props), childHashes),
    props: asPropsType(props),
    children: childrenList
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
    generation: ref(0),
  }
}
