# Architecture

## Boundary

`game_rules_state_c` is a headless C17 static library embedded by a host application. The host
loads a level and sends cardinal movement or rewind commands; the library returns complete,
authoritative snapshots and ordered semantic events. Rendering, animation, audio, controls,
persistence, networking, and scheduling remain host responsibilities.

The public ABI is declared by `include/game_rules/c_api.h`. It uses fixed-width integers, plain C
structs, opaque engine handles, explicit status values, and caller-disposed result ownership.
`include/game_rules/c_allocator_api.h` is an additive versioned extension for host allocators.
No standard-library, yyjson, Emscripten, or Unreal type crosses the boundary. The public names are
title-independent and remain stable if the repository or game title changes.

## State and transitions

Each `game_rules_engine` owns an independent session containing the canonical static level,
current resolved state, scratch state, and undo-only pre-command history. There is no global
mutable engine state.

Loads, moves, and rewinds allocate and construct their complete result before committing. A failed
or rejected operation cannot expose partial state. Accepted commands resolve gravity, ramp slides,
fixtures, explosion waves, crushing, and terminal outcomes against immutable pre-tick snapshots.
Each returned tick owns ordered events and a complete state-after snapshot. Rewind restores an
owned resolved-state snapshot rather than replaying simulation.

The [normative rules](rules.md) define behavior and ordering. The [level format](level-format.md)
defines serialization. This document describes implementation boundaries only.

## Portability and dependencies

Production translation units are C17 with extensions disabled. The library has no dependency on a
filesystem, environment variables, wall-clock time, threads, random sources, graphics APIs, input
devices, a C++ runtime, Unreal, or Emscripten. The same sources build natively and for wasm32.

yyjson 0.12.0 is the sole vendored source dependency. It is compiled privately with non-required
features disabled; no yyjson symbol or type is public. Provenance and checksums are in
`vendor/yyjson/README.game-rules.md`.

## Determinism and ownership

- Heights are signed integer half-steps.
- Entity IDs identify entities but never decide simultaneous physics.
- Spatial, entity, fixture, and event collections have explicit canonical ordering.
- Each logical tick reads one immutable pre-state and commits atomically.
- Typed input arrays are borrowed for one call and copied on accepted load.
- Typed result graphs and JSON strings are independently caller-owned.
- Valid level replacement discards old history only after the new session is complete.

These properties make command sequences replayable across native, WebAssembly, Odin, and thin
Unreal adapters.

## Build and verification

CMake exports `GameRules::StateC` both from the build tree and installed package. Candidate-only
tests cover lifecycle, validation, walking, pushing, falling, ramps, fixtures, explosions,
history, allocation failures, ABI layout, malformed input, C++ header inclusion, library-only
configuration, and native/wasm smoke execution. Differential runners and the C++ reference remain
in the originating repository during transition; see [parity-transition.md](parity-transition.md).
