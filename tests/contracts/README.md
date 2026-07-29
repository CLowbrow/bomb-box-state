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

The C17 differential runners reuse this syntax and add two runner-lifecycle operations:
`destroy-engine|-` and `create-engine|-`. A dash means that the operation has no authored engine
response oracle; both runners emit the same canonical lifecycle record for comparison. A runner
starts with one fresh engine, repeated `load` operations cover replacement, and a level input path
may name malformed bytes. These extensions do not change existing scripts or expected outputs.

Unlike the contract-oracle runner, each differential runner writes one complete JSON response per
operation to standard output. `tools/c-port/compare_transcript.py` executes the reference and
candidate in separate processes and reports the first structured difference with operation index,
transcript line, and JSON path.
