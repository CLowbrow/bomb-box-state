# Architecture constraints

## Portability boundary

`bomb_box_state` is the authoritative rules library. It may use portable C++20, the standard
library, and the pinned private yyjson C99 source internally, but it must not depend on Unreal
headers, Emscripten headers, browser APIs, rendering, audio, input devices, wall-clock time,
threads, or random-number sources.

Platform code belongs in `integrations/`:

- `integrations/wasm` links the core and exports its C ABI to JavaScript.
- `integrations/unreal` wraps a platform-specific core build in an Unreal runtime plugin.

The C API uses fixed-width integers, opaque handles (when stateful calls are added), caller-owned
buffers, and explicit error codes. Do not pass C++ standard-library objects across that ABI.

The versioned level JSON codec is part of the portable core but lives in separate translation units.
It accepts and returns in-memory strings and has no filesystem or platform dependency. Generic JSON
syntax is parsed by the pinned, unmodified yyjson 0.12.0 source. A private C bridge gives upstream
functions internal linkage and exports only Bomb Box-prefixed bridge symbols, avoiding collisions
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
internal orchestration mechanism rather than a host state-injection API. Flat walking commits from
an immutable command-boundary snapshot and returns the initial state, zero-based ordered ticks with
semantic events and per-tick states, the final authoritative state, and outcome. Rejections return
the unchanged state without entering history. The primitive stateful C API and WebAssembly adapter
are the next layer so the same create/load/input/state loop is exercised before advanced physics.
Initialization physics and the remaining movement systems are introduced afterward.

`decode_level_json()` produces that same canonical `LevelDefinition` only after strict format and
structural validation. JSON decoding is deliberately separate from `Engine::load_level()` so an
invalid document cannot create partially committed engine state, and so integrations can attach
their own distribution metadata outside the rule-relevant document.
