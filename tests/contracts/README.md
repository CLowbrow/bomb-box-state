# Adapter-neutral contracts

Each contract script describes an ordered sequence against one fresh engine. Non-comment lines use
two or three pipe-separated fields:

```text
operation|expected-response.json|optional-input
```

Supported operations are `load` with a level JSON path, `get-state`, `move` with a cardinal
direction, and `rewind`. Paths are relative to the script. Both the native C ABI runner and the
Node/WebAssembly runner execute the same scripts and compare every complete response with the
authored JSON named in the second field.

Expected files are reviewed test oracles, not outputs copied from another adapter. Reordered forms
of a level should point to the same expectation when the rules require container-order independence.
