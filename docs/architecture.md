# Architecture constraints

## Portability boundary

`game_rules_state` is the authoritative rules library. It may use portable C++20, the standard
library, and the pinned private yyjson C99 source internally, but it must not depend on Unreal
headers, Emscripten headers, browser APIs, rendering, audio, input devices, wall-clock time,
threads, or random-number sources.

Platform code belongs in `integrations/`:

- `integrations/wasm` links the core and exports its C ABI to JavaScript.
- `integrations/unreal` wraps a platform-specific core build in an Unreal runtime plugin.

The C API uses fixed-width integers, opaque per-instance engine handles, caller-owned JSON result
buffers, and explicit status/error strings. Do not pass C++ standard-library objects across that
ABI. The WebAssembly adapter adds only JavaScript ownership and JSON parsing; its response contract
is documented in [the embedding API](embedding-api.md).

The versioned level JSON codec is part of the portable core but lives in separate translation units.
It accepts and returns in-memory strings and has no filesystem or platform dependency. Generic JSON
syntax is parsed by the pinned, unmodified yyjson 0.12.0 source. A private C bridge gives upstream
functions internal linkage and exports only `game_rules_*` bridge symbols, avoiding collisions
when an embedding host uses another yyjson build. yyjson types do not enter a public header. Static
library consumers that construct `LevelDefinition` directly do not pull the codec objects into their
final binary unless they reference it. See the [level format specification](level-format.md) and
[third-party notices](../THIRD_PARTY_NOTICES.md).

## Determinism guardrails

- Encode rule heights as integer half-steps; the current geometry never needs arbitrary floating
  point values.
- Give entities stable integer IDs, but never use ID or container iteration order to resolve a
  simultaneous physics conflict.
- Compute a tick from an immutable pre-tick snapshot and commit its result atomically.
- Keep serialization canonical: explicit coordinate convention, explicit enum spellings, and stable
  event ordering.
- Avoid hidden global state. An engine instance should own all mutable simulation state.
- Behavior tests should compare complete tick/event/state results across native and Wasm builds.

## Verification boundary

The installed engine and its public package must not acquire dependencies from the test system.
Native unit and behavior tests may use a test-only framework, filesystem fixtures, and richer
diagnostic support because those facilities are excluded from the `GameRules::State` target and its
install/export surface. The core itself is still compiled with its production no-exceptions,
no-RTTI, portability, and sanitizer settings when tests link it.

Cross-adapter confidence comes from shared logical contracts, not from compiling every C++ unit
test for every host. Native C++ runs the complete unit and behavior suites. Beginning with the
stateful phase-4 boundary, native C ABI and Node/WebAssembly runners execute the same versioned,
adapter-neutral public scenarios and normalize their outputs to the same logical result model.
Those scenarios compare authored expected acceptance, ticks, ordered events, authoritative state,
outcome, and lifecycle effects. They must not treat one adapter's live output as the oracle for
another, because a shared core defect could otherwise make both agree on the wrong result.

Boundary-specific tests remain necessary for C compilation, opaque-handle and buffer ownership,
WebAssembly memory and export behavior, JavaScript representation of 64-bit entity IDs, and later
Unreal ownership and packaging. Internal unit tests, fuzz targets, and benchmarks are not part of
the cross-adapter contract corpus.

## Intended implementation layers

1. Value types, validated level schema, and versioned level JSON encoding.
2. Immutable world snapshots and deterministic queries.
3. Flat walking, turn results, and history orchestration for a minimal native
   input-to-state loop.
4. Primitive stateful C API and a thin JavaScript/WebAssembly adapter for the
   same loop.
5. Tick planners and turn orchestration for pushes, gravity, fixtures, ramps,
   and blast waves.
6. Terminal handling, complete conflict coverage, and additional thin host
   adapters such as Unreal.

The current `Engine` owns a canonical, validated `LevelDefinition`, the current dynamic
`ResolvedState`, and an undo-only stack of earlier resolved states. Loading validates and prepares a
candidate before replacing the prior definition and resetting history. Snapshots are returned by
value so callers cannot retain a mutable view into engine-owned storage. History transitions are an
internal orchestration mechanism rather than a host state-injection API. Internal world and state
query helpers centralize coordinate stepping, bounds and fixture lookup, support heights, occupancy,
entity ordering, and armed-barrel bookkeeping so rule planners share the same deterministic
semantics. Player walking and pushing are planned from an immutable command-boundary state by a
dedicated physical-movement planner; `Engine` remains responsible for command gating, fixture
derivation, derived stabilization, history commit, and public result assembly. Flat walking,
single-entity player pushes, and gravity return zero-based ordered ticks with semantic events and
per-tick states. Gravity is an independent column
planner: it calculates final landing heights bottom-up, commits every possible fall in one tick,
preserves spatial event order, and records armed barrel IDs in resolved state. Fixture resolution
derives palette-ordered active switch colors and row-major effectively open doors after each
physical tick, appending deterministic semantic changes to that tick. Teleporter contact finalizes
the tick before fixture or later derived work and gives a win precedence over a same-tick loss.
Ramp traversal changes player height by one half-step between oriented endpoints and the ramp
center. Downhill pushes enter the ramp center atomically, then a separate immutable slide planner
moves every unblocked ramp stack to its low endpoint, rejects shared-destination conflicts, and
retries blocked stacks after later physical or fixture changes. Derived stabilization always runs
gravity before sliding so entities falling onto a ramp settle and arm before the complete stack can
slide in a later tick. The single-explosion planner removes one settled armed source from an
immutable pre-blast state, targets adjacent flat and oriented ramp heights, applies legal one-cell
blast pops, arms affected barrels, and reports direct player loss atomically. Gravity and sliding
then settle consequences in later ticks. Simultaneously ready sources, later explosion waves, and
chain scheduling remain a separate orchestration phase so entity IDs never choose a source order.
Loading runs initial fixture derivation followed by the same derived/post-tick path before installing
the first history state and returns the supplied initial state, initialization ticks, stabilized or
terminal state, and outcome. An accepted push commits the player and pushed entity together before
any required fall tick; any failed source, target, or destination check leaves state and history
unchanged. The primitive stateful C API and WebAssembly adapter carry that same
create/load/input/state/rewind loop across the embedding boundary. Single-source blasts are
integrated; simultaneous explosion waves and chains are the next movement-system extension.

`decode_level_json()` produces that same canonical `LevelDefinition` only after strict format and
structural validation. JSON decoding is deliberately separate from `Engine::load_level()` so an
invalid document cannot create partially committed engine state, and so integrations can attach
their own distribution metadata outside the rule-relevant document.
