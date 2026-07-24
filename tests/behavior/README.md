# Behavior tests

Behavior tests are specification-level scenarios expressed through public engine operations. Each
scenario should construct the complete input state, submit one command or perform one lifecycle
operation when practical, and compare the complete ordered result: acceptance or rejection, ticks,
events, authoritative state, outcome, and observable history or level-lifetime effects.

Use GoogleTest `TEST` cases for independent behavior and `SCOPED_TRACE` for table-driven inputs.
Prefer whole-value equality when the public result type defines it; add or improve a printer under
`tests/support` when a failure would otherwise hide the meaningful difference. Shared builders
should keep complete rule-relevant state visible rather than growing into a second rules language.

Keep white-box checks of `bomb_box::detail` algorithms under `tests/unit`. A behavior scenario may
exercise the same concern through `Engine`, but it must not depend on the internal representation.
Do not add mocks or production test hooks where a complete deterministic state and public operation
can express the behavior directly.

Phase 4 will introduce a versioned, adapter-neutral contract corpus beside these native C++ tests.
Native C ABI and Node/WebAssembly runners will execute the same authored scenarios and normalize
their complete outputs to one logical model. The goal is shared public behavior coverage, not
recompiling internal C++ unit tests under WebAssembly. Adapter-specific ownership, memory, export,
and language-representation tests remain alongside each adapter.
