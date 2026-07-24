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
| 5. Single-entity player pushes | **Implemented** | Atomic one-cell box and barrel pushes work in every cardinal direction across compatible flat supports, with unstacked-target enforcement, non-recursive destination checks, deterministic events, exact rewind, and native C/WebAssembly contract coverage. |
| 6. Falling and crushing | **Implemented** | Initial and post-push gravity compacts independent columns bottom-up in deterministic derived ticks, including stacks and ramp-center landings; player fall loss, latent crushing, barrel arming, complete load output, exact rewind, and replacement isolation are covered. |
| 7. Fixtures and terminal outcomes | **Not started** | Fixture data is validated and gravity can produce a loss, but switches, effective door state, teleporters, and win outcomes are not resolved. |
| 8. Ramps and sliding | **Not started** | Ramp geometry and endpoints are validated, but traversal and automatic sliding are not implemented. |
| 9. Single explosions | **Not started** | Falling barrels become authoritatively armed, but detonation and height-aware blast behavior do not exist. |
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
  Resolved state includes canonical armed-barrel IDs so arming is replayable
  and survives rewind without becoming authored level data.
- `Engine::move()` accepts cardinal input and returns `MoveResult` with a
  stable status, attempted direction, rejection events, initial state,
  zero-based ordered tick results, final authoritative state, and outcome.
  Semantic event payloads cover blocked movement, rewind, entity movement,
  barrel arming, player crushing, and level loss without asking a host to
  reconstruct authoritative state from them.
- The movement planner recognizes an unstacked box or barrel intersecting the
  player's movement height as a push target. It validates the target and next
  cell from the immutable pre-tick state, then atomically commits both moves in
  one tick. Push events are deterministically ordered player first and pushed
  entity second. Stacked targets report `stacked_push_target`; a lower flat
  support now produces a following derived fall tick in the same accepted turn.
- Gravity plans each cell independently from one immutable pre-tick snapshot,
  compacts separated groups bottom-up without using entity IDs to choose
  behavior, and emits events in canonical coordinate and pre-tick height order.
  Falls onto ramp centers settle before the deferred slide phase. Fatal player
  falls and would-be landings on the player produce terminal loss output.
- A valid load is canonicalized and gravity-stabilized before replacing the
  current level. Its result includes the supplied initial dynamic state,
  initialization ticks, final authoritative state, and outcome. An invalid
  load returns stable validation errors and leaves the current level, resolved
  state, and history unchanged. A valid replacement installs only its final
  initialization state and makes all earlier level history unreachable.
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

The loader permits physically unstable authored entities, preserves that
canonical supplied snapshot in `loaded_level()`, and runs Phase 6 gravity before
installing `resolved_state()` as the first command boundary. Initialization can
therefore produce falling, barrel-arming, and player-loss ticks. Fixture state,
ramp sliding, and barrel detonation remain deferred to their later phases, so
initialization is not yet stabilization under the complete specification.

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
| Native debug | `cmake --preset native-debug`; `cmake --build --preset native-debug`; `ctest --preset native-debug --output-on-failure` | Passed: 47 of 47 tests: 37 behavior cases, 4 C ABI boundary cases, 3 focused unit cases, 1 cross-adapter contract runner, and 2 production consumer/header smokes. |
| Native release | `cmake --preset native-release`; `cmake --build --preset native-release`; `ctest --preset native-release --output-on-failure` | Passed: 47 of 47 tests. |
| WebAssembly debug | `cmake --preset wasm-debug`; `cmake --build --preset wasm-debug`; `ctest --preset wasm-debug --output-on-failure` | Passed: portable core and stateful adapter build; 1 of 1 Node tests ran the authored browser vertical-slice contract. |
| Native sanitized | `cmake --preset native-sanitized`; `cmake --build --preset native-sanitized` | Configure and build passed with deferred GoogleTest discovery. Tests were not executed because `AGENTS.md` prohibits running the sanitizer preset inside the Codex workspace sandbox, where the runtime stalls at test startup and can leave CPU-consuming processes. CI now has an outside-sandbox sanitizer job. |
| JSON Schema syntax | `jq empty docs/level-format.schema.json` | Passed. |
| Vendored yyjson | `shasum -a 256 vendor/yyjson/yyjson.c vendor/yyjson/yyjson.h vendor/yyjson/LICENSE`; global-symbol inspection with `nm` | All files match the recorded 0.12.0 import checksums. Only the two `game_rules_*` bridge symbols are global; no upstream `yyjson_*` implementation symbol is exported. |
| Install package | `cmake --install out/build/native-debug --prefix <temporary-directory>` | Passed. The static archive is self-contained, no GoogleTest artifact or dependency is installed, and the package includes the yyjson MIT license, third-party notice, and provenance README. |
| Unreal | Not run. | The Unreal wrapper remains a scaffold and no Unreal toolchain verification has been recorded. |

## Known limitations

- Initialization currently stabilizes gravity only. It does not yet derive
  switch or door state, slide ramp occupants, detonate armed barrels, or resolve
  teleporter wins.
- Movement resolves player walking, single-entity pushes, and derived falling.
  Ramp traversal and fixture destinations are rejected explicitly as
  `unsupported_geometry` and `unsupported_fixture` until their rule phases are
  implemented.
- A falling barrel becomes authoritatively armed, but detonation is deferred to
  phase 9. Until that phase, an armed barrel can remain at a command boundary
  even though complete-spec resolution will eventually explode it before more
  input is accepted.
- Falling-on-player crushing is implemented in the gravity planner and covered
  through its focused internal rule seam. No currently implemented public
  player action can create an entity above the player; ramps and blasts will
  exercise that path through public behavior scenarios in later phases.
- Fixture behavior, ramp traversal and sliding, and explosions remain
  schema-only.
- Sanitized tests must be run outside the Codex workspace sandbox; the runtime
  stalls at test startup inside it.

## Recommended next task

Implement phase 7 fixtures and terminal outcomes next. Add initial and
post-tick switch derivation, effective door state, safe door transitions,
teleporter restrictions and wins, win-before-loss precedence, and complete
terminal history behavior before beginning ramp traversal and sliding.
