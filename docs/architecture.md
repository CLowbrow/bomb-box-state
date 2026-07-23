# Architecture constraints

## Portability boundary

`bomb_box_state` is the authoritative rules library. It may use portable C++20 and the standard
library internally, but it must not depend on Unreal headers, Emscripten headers, browser APIs,
rendering, audio, input devices, wall-clock time, threads, or random-number sources.

Platform code belongs in `integrations/`:

- `integrations/wasm` links the core and exports its C ABI to JavaScript.
- `integrations/unreal` wraps a platform-specific core build in an Unreal runtime plugin.

The C API uses fixed-width integers, opaque handles (when stateful calls are added), caller-owned
buffers, and explicit error codes. Do not pass C++ standard-library objects across that ABI.

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

1. Value types and validated level schema.
2. Immutable world snapshots and deterministic queries.
3. Tick planners for movement, gravity, ramps, fixtures, and blast waves.
4. Turn orchestration and terminal handling.
5. Serialization and the primitive C API.
6. Thin JavaScript and Unreal-facing adapters.

The current `Engine` owns a canonical, validated `LevelDefinition`. Loading validates a candidate
before atomically replacing the prior definition, and snapshots are returned by value so callers
cannot retain a mutable view into engine-owned storage. Gameplay state, initialization
stabilization, command history, and the primitive stateful C API are introduced by their later
implementation phases.

