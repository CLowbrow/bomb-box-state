# Implementation status

This document records current implementation coverage and verification. It is
project tracking, not a gameplay specification. `README.md` remains the
normative source for gameplay and state-transition behavior.

## Status definitions

- **Not started**: no intentional implementation of the phase exists beyond
  general scaffolding or prerequisites delivered by an earlier phase.
- **In progress**: some scoped behavior exists, but required behavior or tests
  for the phase are incomplete.
- **Implemented**: the phase's current scope is present and its behavior tests
  pass. Any verification limitation must still be recorded below.

## Suggested-order progress

The current delivery priority is a playable browser-facing vertical slice. The
engine should first prove that a host can create a world, submit a player input,
and consume a new authoritative world state. Advanced interactions and physics
follow only after that loop works through WebAssembly.

| Phase | Status | Current coverage |
| --- | --- | --- |
| 1. World schema and level lifetime | **Implemented** | Typed coordinates, explicit axis conventions, flat and ramp cells, fixtures, stable entity IDs, half-step heights, stacks, structural validation, deterministic canonical storage and JSON, owned snapshots, and atomic valid/invalid level replacement. |
| 2. Resolved-state history and rewind | **Implemented** | Caller-owned resolved-state snapshots, engine-owned undo-only history, rewind results, initialized `history_empty` behavior, repeated rewind, branching after rewind, and atomic load-time history replacement are covered. |
| 3. Flat walking and authoritative turn output | **Not started** | No gameplay movement command API, flat-cell walking rules, or public movement result exists yet. |
| 4. Stateful C ABI and browser vertical slice | **Not started** | The C ABI and WebAssembly module cannot yet create an engine, load a level, submit movement or rewind, or publish host-readable state and turn results. |
| 5. Single-entity player pushes | **Not started** | Boxes and barrels exist in the schema, but player push behavior is not implemented. |
| 6. Falling and crushing | **Not started** | Unsupported initial entities are structurally accepted but are not stabilized. |
| 7. Fixtures and terminal outcomes | **Not started** | Fixture data is validated, but switches, effective door state, teleporters, and win/loss behavior are not resolved. |
| 8. Ramps and sliding | **Not started** | Ramp geometry and endpoints are validated, but traversal and automatic sliding are not implemented. |
| 9. Single explosions | **Not started** | Barrel entities exist in the schema, but arming, explosion, and height-aware blast behavior do not. |
| 10. Explosion waves and chains | **Not started** | Simultaneous impulses, conflicts, waves, and chain reactions are not implemented. |
| 11. Lifecycle, conflict, and cross-adapter hardening | **Not started** | Phase-one loading behavior has native tests, but the complete lifecycle/conflict corpus and cross-adapter parity coverage do not exist. |

### Immediate vertical slice

Phase 3 is intentionally narrower than the complete movement rules. Its test
levels use flat cells and a single player, with no interactions that require
pushes, gravity, ramps, fixtures, or explosions. It is complete when:

- `Engine` accepts a cardinal movement command on a stable, ongoing state and
  implements one-cell walking between compatible flat cells;
- each command returns an explicit acceptance or rejection, ordered tick and
  semantic event output when accepted, the complete resulting resolved state,
  and the current outcome;
- accepted moves preserve the prior resolved state for rewind, while rejected
  moves leave both state and history unchanged; and
- specification-level native tests cover all accepted directions, world
  boundaries and other relevant rejections, deterministic complete results,
  repeated rewind, and branching after rewind.

Phase 4 carries that loop across the embedding boundary before any additional
gameplay system is added. It is complete when:

- the primitive C ABI provides opaque engine creation and destruction, level
  loading from versioned JSON, movement, rewind, and caller-owned access to the
  current state and complete command result;
- the WebAssembly ES module exposes those operations through a thin JavaScript
  interface without leaking C++ ownership or requiring a host to reconstruct
  authoritative state from events; and
- an automated JavaScript integration scenario creates an engine, loads a
  simple world, reads renderable cells and entities, moves the player, observes
  the updated authoritative state, rewinds it, and destroys the engine.

The browser milestone supplies state and semantic results to a rendering host;
rendering, animation, and input mapping remain outside this headless library.
Output types and adapters introduced by phases 3 and 4 should be extended by
later rules rather than replaced after each phase.

## Implemented public surface

- `bomb_box/world.hpp` defines `LevelDefinition`, cells, fixtures, entities,
  coordinate conventions, validation errors, `validate_level()`, and
  `canonicalize_level()`.
- `bomb_box/level_json.hpp` defines the strict version 1 JSON decoder and
  canonical encoder. It distinguishes syntax/shape errors from shared
  structural validation errors and applies configurable untrusted-input size
  and nesting limits. Generic syntax parsing uses the pinned yyjson 0.12.0
  source behind a private, symbol-isolated C bridge.
- `docs/level-format.md` is the normative wire-format specification, accompanied
  by a JSON Schema for editor-side shape validation. Entity IDs are decimal
  strings so every `uint64` value round-trips through browser tooling.
- `bomb_box/engine.hpp` provides per-instance level ownership, `load_level()`,
  `has_level()`, and caller-owned `loaded_level()` snapshots. It also defines
  the dynamic `ResolvedState`, caller-owned `resolved_state()` snapshots, and
  `rewind()` results with stable `rewound` and `history_empty` status strings.
- A valid load is canonicalized before replacing the current level. An invalid
  load returns stable validation errors and leaves the current level, resolved
  state, and history unchanged. A valid replacement installs a fresh initial
  state and makes all earlier level history unreachable.
- Resolved-state history is held by the engine as an undo-only stack. The
  internal transition mechanism preserves exact states, supports repeated
  rewind and branching without redo storage, and is ready for accepted
  movement turns to commit states in phase 3.
- The C ABI currently exposes only its API version and `schema_ready` status.
  A stateful primitive C boundary remains future work.

The loader still performs structural validation and canonical storage without
physics stabilization. It deliberately permits unsupported initial entities
because stabilization depends on later falling, ramp, fixture, and explosion
phases. For phase 2, the canonical supplied entities and an `ongoing` outcome
form the initial command-boundary state. Later phases must replace that
provisional initialization step with full stabilization and terminal outcome
derivation. `loaded_level()` remains the supplied definition snapshot, while
`resolved_state()` is the dynamic state that future turns will change.

## Verification record

Most recently recorded on 2026-07-23:

| Surface | Commands | Result |
| --- | --- | --- |
| Native debug | `cmake --preset native-debug`; `cmake --build --preset native-debug`; `ctest --preset native-debug --output-on-failure` | Passed: 4 of 4 tests, including the resolved-state history behavior suite. |
| Native release | `cmake --preset native-release`; `cmake --build --preset native-release`; `ctest --preset native-release --output-on-failure` | Passed: 4 of 4 tests. |
| WebAssembly debug | `cmake --build --preset wasm-debug`; `ctest --preset wasm-debug --output-on-failure` | Passed: portable core and adapter build; 1 of 1 Node smoke test. The existing preset was already configured with Emscripten. |
| Native sanitized | `cmake --preset native-sanitized`; `cmake --build --preset native-sanitized` | Configure and build passed. Test execution was stopped at startup because the sanitizer runtime is known to stall in the Codex workspace sandbox and leave CPU-consuming processes; `AGENTS.md` now prohibits running this preset's tests inside that sandbox. |
| JSON Schema syntax | `jq empty docs/level-format.schema.json` | Passed. |
| Vendored yyjson | `shasum -a 256 vendor/yyjson/yyjson.c vendor/yyjson/yyjson.h vendor/yyjson/LICENSE`; global-symbol inspection with `nm` | All files match the recorded 0.12.0 import checksums. Only the two Bomb Box-prefixed bridge symbols are global; no upstream `yyjson_*` implementation symbol is exported. |
| Install package | `cmake --install out/build/native-debug --prefix <temporary-directory>` | Passed. The static archive is self-contained and the package includes the yyjson MIT license, third-party notice, and provenance README. |
| Unreal | Not run. | The Unreal wrapper remains a scaffold and no Unreal toolchain verification has been recorded. |

## Known limitations

- Loading does not run initialization stabilization or produce initialization
  ticks, semantic events, or derived terminal outcomes. Its initial resolved
  state currently mirrors the canonical supplied entities with an `ongoing`
  outcome.
- There is no movement command API or turn/tick orchestration. Consequently,
  accepted-turn history entries use the same internal mechanism verified by
  the phase-two behavior suite but cannot yet be produced through a public
  movement command.
- Fixture, gravity, ramp, and explosion behavior is schema-only.
- The stateful C ABI and cross-adapter behavior corpus remain unimplemented.
- The level JSON codec is C++-only today; the C ABI and WebAssembly adapter do
  not yet expose stateful decode/load calls.
- Sanitized tests must be run outside the Codex workspace sandbox; the runtime
  stalls at test startup inside it.

## Recommended next task

Implement phase 3 without pulling later physics into the first vertical slice:
add the public cardinal movement command and result types, then implement
one-cell walking on simple flat-cell levels. Return a complete authoritative
result for accepted movement and explicit rejection information otherwise.
Add specification-level scenarios for all four directions, applicable blocked
movement and invalid-input cases, deterministic complete state/event output,
history preservation only for accepted turns, repeated rewind, and branching
with a new move after rewind. Once that passes natively, phase 4 is the next
priority; do not begin pushes, gravity, fixtures, ramps, or explosions first.
