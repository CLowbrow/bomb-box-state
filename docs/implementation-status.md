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
| 3. Flat walking and authoritative turn output | **Implemented** | Cardinal movement honors declared axes, walks one cell between compatible flat supports, returns complete tick/event/state/outcome results, rejects boundaries, ledges, blocking occupied destinations, malformed input, and terminal states, and preserves history only for accepted turns. |
| 4. Stateful C ABI and browser vertical slice | **Implemented** | Opaque per-instance engines, versioned JSON loading, renderable snapshots, movement, complete tick/event results, rewind, caller-owned result memory, and a thin JavaScript ownership layer are exercised by one authored contract through native C and Node/WebAssembly. |
| 5. Single-entity player pushes | **Implemented** | Atomic one-cell box and barrel pushes work in every cardinal direction across compatible flat supports, with unstacked-target enforcement, non-recursive destination checks, deterministic events, exact rewind, and native C/WebAssembly contract coverage. |
| 6. Falling and crushing | **Implemented** | Initial and post-push gravity compacts independent columns bottom-up in deterministic derived ticks, including stacks and ramp-center landings; player fall loss, latent crushing, barrel arming, complete load output, exact rewind, and replacement isolation are covered. |
| 7. Fixtures and terminal outcomes | **Implemented** | Color-wide AND switches, rewindable effective door state, safe occupied-door hold-open behavior, fixture-aware walking and pushes, teleporter restrictions and wins, terminal gating/history, initialization, and win-before-loss precedence are covered. |
| 8. Ramps and sliding | **Implemented** | Oriented half-step player traversal, downhill box/barrel pushes, deterministic automatic whole-stack slides, blocked retries, fixture-aware destinations, fall-before-slide ordering, and slide conflicts are covered. |
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
  Resolved state includes canonical armed-barrel IDs, palette-ordered active
  switch colors, and row-major effectively open door coordinates so derived
  state is replayable and survives rewind without becoming authored level data.
- `Engine::move()` accepts cardinal input and returns `MoveResult` with a
  stable status, attempted direction, rejection events, initial state,
  zero-based ordered tick results, final authoritative state, and outcome.
  Semantic event payloads cover blocked movement, rewind, entity movement,
  barrel arming, switch changes, door transitions, player crushing, and level
  win/loss without asking a host to reconstruct authoritative state from them.
- The movement planner recognizes an unstacked box or barrel intersecting the
  player's movement height as a push target. It validates the target and next
  cell from the immutable pre-tick state, then atomically commits both moves in
  one tick. Push events are deterministically ordered player first and pushed
  entity second. Stacked targets report `stacked_push_target`; a lower flat
  support now produces a following derived fall tick in the same accepted turn.
- Gravity plans each cell independently from one immutable pre-tick snapshot,
  compacts separated groups bottom-up without using entity IDs to choose
  behavior, and emits events in canonical coordinate and pre-tick height order.
  Falls onto ramp centers settle before the slide phase. Fatal player falls
  and would-be landings on the player produce terminal loss output.
- Players traverse oriented ramps through half-step endpoint/center movements.
  A box or barrel can be pushed from the high endpoint into the ramp center,
  after which the automatic slide planner moves the complete ramp stack to an
  unblocked low endpoint in a separate derived tick. Slides use an immutable
  pre-tick snapshot, reject shared-destination conflicts, retry after later
  state changes, honor doors and teleporters, and emit source-row-major,
  bottom-to-top events without arming barrels.
- Fixture resolution runs after each physical tick. Switch colors use
  color-wide AND behavior, inactive occupied doors stay effectively open until
  vacated, and switch/door events follow physical events in deterministic
  palette and row-major order. Closed doors and reserved teleporter columns
  participate in both walk and push legality.
- Direct teleporter-floor contact finalizes the current tick as a win before
  fixture changes or later derived work. Initialization can win immediately;
  terminal movement is gated, while rewind and level replacement retain their
  existing lifecycle behavior. A focused rule test covers win precedence over
  a same-tick loss.
- A valid load is canonicalized and fixture/gravity-stabilized before replacing
  the current level. Its result includes the supplied initial dynamic state,
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
  include canonical cells, fixtures, entities, active switch colors,
  effectively open doors, coordinates, and outcome; 64-bit entity IDs remain
  decimal strings. Fixture and terminal events are serialized at the same
  boundary.
- The generated WebAssembly ES module exposes a JavaScript-shaped
  `module.gameRules` interface whose engine objects hide numeric handles, own
  conversion and freeing of C response strings, accept cardinal strings, and
  guard destruction locally. Its API and version-1 response contract are
  documented in `docs/embedding-api.md`.

The loader permits physically unstable authored entities, preserves that
canonical supplied snapshot in `loaded_level()`, derives initial fixture state,
and runs gravity and ramp sliding with post-tick fixture and teleporter
resolution before installing `resolved_state()` as the first command boundary.
Initialization can therefore produce switch/door transitions, falling,
barrel-arming, whole-stack slides, and terminal win/loss ticks. Barrel
detonation remains deferred to its later phase, so initialization is not yet
stabilization under the complete specification.

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
| Native debug | `cmake --preset native-debug`; `cmake --build --preset native-debug`; `ctest --preset native-debug --output-on-failure` | Passed: 69 of 69 tests: 57 behavior cases, 5 C ABI boundary cases, 4 focused unit cases, 1 cross-adapter contract runner, and 2 production consumer/header smokes. |
| Native release | `cmake --preset native-release`; `cmake --build --preset native-release`; `ctest --preset native-release --output-on-failure` | Passed: 69 of 69 tests. |
| WebAssembly debug | `cmake --preset wasm-debug`; `cmake --build --preset wasm-debug`; `ctest --preset wasm-debug --output-on-failure` | Passed: portable core and stateful adapter build; 1 of 1 Node tests ran the authored browser vertical-slice contract. |
| Native sanitized | `cmake --preset native-sanitized`; `cmake --build --preset native-sanitized` | Configure and build passed with deferred GoogleTest discovery. On this Apple Silicon/macOS host, test discovery timed out before cases ran because the Apple sanitizer runtime stalled at process startup; the same behavior was reproduced in a normal terminal, so execution is delegated to the Ubuntu sanitizer CI job. |
| JSON Schema syntax | `jq empty docs/level-format.schema.json` | Passed. |
| Vendored yyjson | `shasum -a 256 vendor/yyjson/yyjson.c vendor/yyjson/yyjson.h vendor/yyjson/LICENSE`; global-symbol inspection with `nm` | All files match the recorded 0.12.0 import checksums. Only the two `game_rules_*` bridge symbols are global; no upstream `yyjson_*` implementation symbol is exported. |
| Install package | `cmake --install out/build/native-debug --prefix <temporary-directory>` | Passed after the Phase 7 public-state/event additions. The static archive is self-contained, no GoogleTest artifact or dependency is installed, and the package includes the yyjson MIT license, third-party notice, and provenance README. |
| Unreal | Not run. | The Unreal wrapper remains a scaffold and no Unreal toolchain verification has been recorded. |

## Known limitations

- Initialization derives fixture state and stabilizes gravity and ramp slides,
  but it does not yet detonate armed barrels.
- Movement resolves flat and ramp walking, single-entity pushes, derived
  falling, and automatic whole-stack ramp slides. Switches, doors, and
  teleporters participate fully.
- A falling barrel becomes authoritatively armed, but detonation is deferred to
  phase 9. Until that phase, an armed barrel can remain at a command boundary
  even though complete-spec resolution will eventually explode it before more
  input is accepted.
- Falling-on-player crushing is implemented in the gravity planner and covered
  through its focused internal rule seam. Ramp initialization now covers legal
  player-topped stack movement, but no currently implemented public action can
  create a falling entity above the player; blasts will exercise that path
  through public behavior scenarios in a later phase.
- Explosions remain schema-only.
- Sanitized tests must run in the Ubuntu CI job or another known-working Linux
  environment. On the current Apple Silicon/macOS host, the Apple sanitizer
  runtime stalls during GoogleTest discovery in both normal terminals and the
  Codex sandbox.

## Recommended next task

Implement phase 9 single explosions next. Add settled armed-barrel detonation,
height-aware same-cell and adjacent-cell effects, blast-driven movement and
fall/ramp follow-up, fixture interactions, terminal precedence, and exact
rewind behavior before introducing simultaneous explosion waves and chains.
