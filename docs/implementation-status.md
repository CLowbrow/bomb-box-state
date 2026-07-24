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
| 3. Flat walking and authoritative turn output | **Implemented** | Cardinal movement honors declared axes, walks one cell between compatible flat supports, returns complete tick/event/state/outcome results, rejects boundaries, ledges, blocking occupied destinations, malformed input, terminal states, and deferred geometry/fixtures explicitly, and preserves history only for accepted turns. |
| 4. Stateful C ABI and browser vertical slice | **Implemented** | Opaque per-instance engines, versioned JSON loading, renderable snapshots, movement, complete tick/event results, rewind, caller-owned result memory, and a thin JavaScript ownership layer are exercised by one authored contract through native C and Node/WebAssembly. |
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
- a versioned, adapter-neutral contract scenario runs through both the native C
  ABI and Node/WebAssembly surfaces: it creates an engine, loads a simple
  world, reads renderable cells and entities, moves the player, compares the
  complete authoritative result, rewinds, and destroys the engine. Each runner
  compares normalized output with authored expectations rather than using the
  other adapter's live output as its oracle.

The browser milestone supplies state and semantic results to a rendering host;
rendering, animation, and input mapping remain outside this headless library.
Output types and adapters introduced by phases 3 and 4 should be extended by
later rules rather than replaced after each phase.

## Implemented public surface

- Project-owned technical identifiers are title-independent and use the
  descriptive `game_rules` family: C++ namespace and include directory,
  C ABI and private bridge symbols, CMake package/targets/options, WebAssembly
  module, Unreal modules, test targets, and the `game-rules-level` format
  discriminator. The earlier working-title-derived identifiers were removed
  without compatibility aliases as a deliberate pre-release breaking change.
- `game_rules/world.hpp` defines `LevelDefinition`, cells, fixtures, entities,
  coordinate conventions, validation errors, `validate_level()`, and
  `canonicalize_level()`.
- `game_rules/level_json.hpp` defines the strict version 1 JSON decoder and
  canonical encoder. It distinguishes syntax/shape errors from shared
  structural validation errors and applies configurable untrusted-input size
  and nesting limits. Generic syntax parsing uses the pinned yyjson 0.12.0
  source behind a private, symbol-isolated C bridge.
- `docs/level-format.md` is the normative wire-format specification, accompanied
  by a JSON Schema for editor-side shape validation. Entity IDs are decimal
  strings so every `uint64` value round-trips through browser tooling.
- `game_rules/engine.hpp` provides per-instance level ownership, `load_level()`,
  `has_level()`, and caller-owned `loaded_level()` snapshots. It also defines
  the dynamic `ResolvedState`, caller-owned `resolved_state()` snapshots, and
  `rewind()` results with stable `rewound` and `history_empty` status strings.
- `Engine::move()` accepts cardinal input and returns `MoveResult` with a
  stable status, attempted direction, rejection events, initial state,
  zero-based ordered tick results, final authoritative state, and outcome.
  Semantic event payloads currently cover blocked movement, player movement,
  and rewind, and are structured so later phases can add events without asking
  a host to reconstruct state from them.
- A valid load is canonicalized before replacing the current level. An invalid
  load returns stable validation errors and leaves the current level, resolved
  state, and history unchanged. A valid replacement installs a fresh initial
  state and makes all earlier level history unreachable.
- Resolved-state history is held by the engine as an undo-only stack. Accepted
  flat walks preserve their exact predecessor; rejected moves do not enter
  history. Repeated rewind and branching discard abandoned later states without
  redo storage.
- `game_rules/c_api.h` provides an opaque per-instance engine, stable cardinal
  constants, versioned JSON level loading, complete state/move/rewind JSON
  responses, and explicit caller-owned result memory. Renderable snapshots
  include canonical cells, fixtures, entities, coordinates, and outcome;
  64-bit entity IDs remain decimal strings.
- The generated WebAssembly ES module exposes a JavaScript-shaped
  `module.gameRules` interface whose engine objects hide numeric handles, own
  conversion and freeing of C response strings, accept cardinal strings, and
  guard destruction locally. Its API and version-1 response contract are
  documented in `docs/embedding-api.md`.

The loader still performs structural validation and canonical storage without
physics stabilization. It deliberately permits unsupported initial entities
because stabilization depends on later falling, ramp, fixture, and explosion
phases. For phase 2, the canonical supplied entities and an `ongoing` outcome
form the initial command-boundary state. Later phases must replace that
provisional initialization step with full stabilization and terminal outcome
derivation. `loaded_level()` remains the supplied definition snapshot, while
`resolved_state()` is the dynamic state that future turns will change.

## Test infrastructure

- Native unit and behavior tests use GoogleTest 1.17.0 fetched from its exact
  release commit and verified archive hash as a build-only dependency.
  Individual cases are discovered by CTest and retain `unit`, `behavior`, and
  `native` labels.
- Public behavior scenarios, focused white-box history tests, and the
  framework-free production-mode consumer smoke are separate targets. The
  core and consumer smoke retain no-exceptions/no-RTTI compilation while the
  richer runners do not change the core's installed or exported dependencies.
- Test-only domain printers report structured states, turns, ticks, and events
  on equality failures. Table-driven cases attach their input as failure
  context.
- GoogleTest discovery occurs at CTest time. This prevents a sanitized build
  from starting the sanitizer runtime merely to enumerate cases inside the
  Codex workspace sandbox.
- The versioned adapter-neutral corpus under
  `tests/contracts/browser_vertical_slice/v1/` runs through both native C ABI
  and Node/WebAssembly runners. Both compare against authored expectations;
  internal C++ unit tests are not duplicated under Emscripten.

## Verification record

Most recently recorded on 2026-07-23:

| Surface | Commands | Result |
| --- | --- | --- |
| Native debug | `cmake --preset native-debug`; `cmake --build --preset native-debug`; `ctest --preset native-debug --output-on-failure` | Passed: 33 of 33 tests: 25 behavior cases, 3 C ABI boundary cases, 2 focused unit cases, 1 cross-adapter contract runner, and 2 production consumer/header smokes. |
| Native release | `cmake --preset native-release`; `cmake --build --preset native-release`; `ctest --preset native-release --output-on-failure` | Passed: 33 of 33 tests. |
| WebAssembly debug | `cmake --preset wasm-debug`; `cmake --build --preset wasm-debug`; `ctest --preset wasm-debug --output-on-failure` | Passed: portable core and stateful adapter build; 1 of 1 Node tests ran the authored browser vertical-slice contract. |
| Native sanitized | `cmake --preset native-sanitized`; `cmake --build --preset native-sanitized` | Configure and build passed with deferred GoogleTest discovery. Tests were not executed because `AGENTS.md` prohibits running the sanitizer preset inside the Codex workspace sandbox, where the runtime stalls at test startup and can leave CPU-consuming processes. CI now has an outside-sandbox sanitizer job. |
| JSON Schema syntax | `jq empty docs/level-format.schema.json` | Passed. |
| Vendored yyjson | `shasum -a 256 vendor/yyjson/yyjson.c vendor/yyjson/yyjson.h vendor/yyjson/LICENSE`; global-symbol inspection with `nm` | All files match the recorded 0.12.0 import checksums. Only the two `game_rules_*` bridge symbols are global; no upstream `yyjson_*` implementation symbol is exported. |
| Install package | `cmake --install out/build/native-debug --prefix <temporary-directory>` | Passed. The static archive is self-contained, no GoogleTest artifact or dependency is installed, and the package includes the yyjson MIT license, third-party notice, and provenance README. |
| Unreal | Not run. | The Unreal wrapper remains a scaffold and no Unreal toolchain verification has been recorded. |

## Known limitations

- Loading does not run initialization stabilization or produce initialization
  ticks, semantic events, or derived terminal outcomes. Its initial resolved
  state currently mirrors the canonical supplied entities with an `ongoing`
  outcome.
- Movement currently resolves only player walking across compatible flat
  support surfaces. Same-height occupied space is rejected as `occupied`
  until phase 5 adds pushes. Ramp entry and fixture destinations are rejected
  explicitly as `unsupported_geometry` and `unsupported_fixture` until their
  rule phases are implemented.
- Fixture, gravity, ramp, and explosion behavior is schema-only.
- Sanitized tests must be run outside the Codex workspace sandbox; the runtime
  stalls at test startup inside it.

## Recommended next task

Implement phase 5 single-entity player pushes next. Extend the existing
authoritative move/tick/event model and the version-1 cross-adapter contract
where the new behavior reaches the public boundary. Do not begin gravity,
fixtures, ramps, or explosions before push legality and atomicity have complete
specification-level coverage.
