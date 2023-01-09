// Quick import helpers.
//
// ReScript seems relatively unsupportive of bitwise operations, which are
// critical to our hash functions, so we've written them in TypeScript in
// a neighboring file and we import them here.
@module("./Hash") external hashNumber: (int, int) => int = "hashNumber"
@module("./Hash") external hashString: (int, string) => int = "hashString"
@module("./Hash") external hashNode: (string, 'a, array<int>) => int = "hashNode"
@module("./Hash") external hashMemoInputs: ('a, array<int>) => int = "hashMemoInputs"
@module("./Hash") external hashToHexId: (int) => string = "hashToHexId"
@module("./Hash") external hexIdToHash: (string) => int = "hexIdToHash"

// Blargh
@module("./Hash") external updateNodeProps: ('a, string, 'b, 'c) => () = "updateNodeProps"
