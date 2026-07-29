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
- `game_rules/c_api.h` exposes opaque engine handles and a versioned typed data
  ABI for native foreign-function consumers. Its fixed-width structs and
  explicitly owned immutable results are suitable for direct Odin bindings.
- The same header retains the version 1 caller-owned JSON API used by the
  existing WebAssembly adapter.
- `integrations/wasm` exports the C boundary and adds JavaScript ownership,
  parsing, and lifecycle checks.
- `integrations/unreal` is a plugin scaffold for staging a platform-compatible
  core build into Unreal Engine.

C++ standard-library types never cross the C ABI. Typed input arrays are
borrowed for one call and copied on accepted load. Typed result graphs are
caller-owned and do not alias engine storage, so later operations cannot
invalidate an in-flight presentation result. The C and WebAssembly contracts
are documented in the [embedding API](embedding-api.md).

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

## C17 rewrite boundary

The behavior-preserving C17 candidate lives under `c-port/` and builds as
`game_rules_state_c` / `GameRules::StateC`. It is intentionally self-contained:
standalone configuration, production sources, public headers, candidate tests,
and any future vendored dependencies cannot reach into the C++ checkout. The
parent build performs a byte-for-byte drift check against the frozen C header.

The frozen creation function uses the C runtime allocator. The C candidate also provides an
additive version-1 allocator header, leaving the frozen ABI untouched. Engine/session storage and
independent result graphs retain explicit allocator ownership. Each result graph is one contiguous
arena whose nested views cannot require child frees, and it carries enough private owner metadata
to be disposed after session replacement or engine destruction. Replacement
storage is fully allocated before the active session pointer is swapped, so failure cannot expose
a partially committed level. The allocator context is host-owned and must outlive its engine and
outstanding results; the candidate keeps no global allocator or mutable engine registry.

The candidate owns a private pinned yyjson 0.12.0 copy, strict C17 decoder, shared typed level
view, ordered validator, and canonical session representation. JSON and typed inputs converge before
the allocate-then-swap commit. The parent checkout checks the candidate yyjson source, header, and
license byte-for-byte against the reference pin, while standalone candidate builds use only files
under `c-port/`.

Differential execution uses separate reference and candidate runner processes,
so their identical public C symbols never collide. Both consume the existing
adapter-neutral contract transcript syntax and emit one canonical JSON result
per operation. `docs/c-port-status.md` is the parity inventory and records
intentional stage gaps; the C++ engine and reviewed contract outputs remain the
behavioral oracle during the rewrite.
