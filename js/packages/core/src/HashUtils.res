// Quick import helpers.
@module("./Hash") external updateNodeProps: ('a, int, 'b, 'c) => () = "updateNodeProps"


// We don't need much in the way of finalization here given that we don't have strong
// requirements for avalanche characteristics. Instead here we just force the output to
// positive integers.
let finalize = (n: int) => {
  land(n, 0x7fffffff)
}

// This is just FNV-1a, a very simple non-cryptographic hash algorithm intended to
// be very quick. I chose FNV-1a for its slight avalanche charateristics improvement
// noting that the finalize step we use here just clears the sign bit.
//
// Note too that this implementation uses signed 32-bit integers where the original
// algorithm is intended for unsigned.
//
// See: https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
let mixNumber = (. seed: int, n: int): int => {
  lxor(seed, n) * 0x01000193
}

@genType
let hashString = (. seed: int, s: string): int => {
  let r = ref(seed)

  for i in 0 to Js.String2.length(s) {
    r := mixNumber(. r.contents, Belt.Int.fromFloat(Js.String2.charCodeAt(s, i)))
  }

  r.contents
}

@genType
let hashNode = (. kind: string, props: Js.Dict.t<'a>, children: list<int>): int => {
  let r = hashString(. 0x811c9dc5, kind)
  let r2 = switch Js.Dict.get(props, "key") {
    | Some(k) if Js.Types.test(k, String) => hashString(. r, k)
    | _ => hashString(. r, Js.Option.getExn(Js.Json.stringifyAny(props)))
  }

  finalize(Belt.List.reduceU(children, r2, mixNumber))
}

@genType
let hashMemoInputs = (. props: 'a, children: list<int>): int => {
  let r = if Js.Types.test(props["memoKey"], String) {
    hashString(. 0x811c9dc5, props["memoKey"])
  } else {
    hashString(. 0x811c9dc5, Js.Option.getExn(Js.Json.stringifyAny(props)))
  }

  finalize(Belt.List.reduceU(children, r, mixNumber))
}
