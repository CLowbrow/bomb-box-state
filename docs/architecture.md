# Architecture

## Overview

`game_rules_state` is a headless simulation library. A host loads a level and
sends cardinal movement or rewind commands; the engine returns complete,
authoritative snapshots and ordered semantic events. The host owns rendering,
animation, audio, controls, persistence, and scheduling.

Each `game_rules::Engine` owns one independent session:

- the canonical definition of the loaded level;
- the current resolved state; and
- an undo-only stack of earlier resolved states.

There is no global mutable game state. A process can create, replace, run, and
destroy multiple engines independently.

## Public surfaces

The same rules implementation is available through several thin boundaries:

- `GameRules::State` is the native C++20 static library.
- `game_rules/c_api.h` exposes opaque engine handles and caller-owned JSON
  strings for C and foreign-function consumers.
- `integrations/wasm` exports the C boundary and adds JavaScript ownership,
  parsing, and lifecycle checks.
- `integrations/unreal` is a plugin scaffold for staging a platform-compatible
  core build into Unreal Engine.

C++ standard-library types never cross the C ABI. The C and WebAssembly
response contract is documented in the [embedding API](embedding-api.md).

## State transitions

Level loading parses and validates a candidate before changing the active
session. A valid level is canonicalized, stabilized with the normal gameplay
rules, and then installed as a fresh history root. An invalid load leaves the
current level and its history untouched.

A movement command is planned from the immutable command-boundary state. The
engine commits the player action atomically, then resolves all derived work—
gravity, ramp slides, fixture changes, explosion waves, and terminal outcomes—
until the world is stable or terminal. Each derived tick includes its events
and authoritative state. Only the final resolved state becomes a rewind
boundary.

Rewind restores an exact earlier resolved state. It does not replay events or
rerun the simulation, and abandoned future states are not retained for redo.

The [gameplay rules](rules.md) define the precise ordering and conflict rules.

## Portability boundary

The core uses portable C++20, the standard library, and a pinned private copy
of yyjson. It does not depend on Unreal or Emscripten headers, browser APIs, a
renderer, input devices, a filesystem, environment variables, wall-clock time,
threads, or random-number sources. Platform-specific code belongs under
`integrations/`.

The JSON codec accepts and returns in-memory strings. yyjson is compiled behind
a private bridge with upstream symbols hidden, and its types do not appear in
public headers. Consumers that construct `LevelDefinition` directly do not
pull the codec objects into a final static link unless they use it. See the
[level format](level-format.md) and [third-party notices](../THIRD_PARTY_NOTICES.md).

## Determinism and ownership

- Heights use integer half-steps rather than floating point.
- Stable entity IDs identify entities but never decide simultaneous physics.
- Every tick is computed from one pre-tick snapshot and committed atomically.
- Coordinates, entities, derived state, and events have explicit canonical
  ordering.
- Returned states are caller-owned values, never mutable views into an engine.
- Failures and rejected commands leave state and history unchanged.
- Level replacement makes all state from the previous level unreachable.

These constraints make a command sequence replayable across native,
WebAssembly, and future host adapters.

## Verification boundary

Native unit tests cover focused algorithms, while behavior tests exercise
complete public operations and compare the full ordered result. Native C and
Node/WebAssembly runners also execute the same authored, adapter-neutral
contract scenarios. Expected outputs are reviewed fixtures; neither adapter's
live result is used as the other's oracle.

Boundary-specific tests still cover C compilation and ownership, WebAssembly
memory and JavaScript representations, install-package consumption, and host
integration details. Test frameworks and fixtures are excluded from the
installed `GameRules::State` package.
