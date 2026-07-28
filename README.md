# Game rules engine

This repository contains the headless rules engine for a turn-based,
Sokoban-like puzzle game. **Bomb Box is a working title**, so code and public
interfaces use the title-independent `game_rules` name.

The engine turns a loaded level plus a player command into an authoritative,
deterministic result. A host game is free to animate that result however it
likes; rendering, audio, controls, persistence, and frame timing stay outside
the library.

## What it supports

- Cardinal walking and pushing on multi-level boards
- Stackable boxes, explosive barrels, falling, and crushing
- Oriented ramps and automatic whole-stack sliding
- Simultaneous explosions and deterministic chain reactions
- Color-linked switches and doors
- Exit teleporters, wins, and losses
- Complete undo-only rewind history
- Stable tick-by-tick state and semantic events for animation
- Native C++20, a primitive C ABI, and a JavaScript/WebAssembly adapter

Every turn resolves fully before the next command. For example, one push can
send a barrel over a ledge, trigger a fall, slide it down a ramp, detonate it,
move other entities, update doors, and end the level. The caller receives each
tick in order as well as the final state; it never has to reconstruct gameplay
state from animation events.

## Build and test

You need CMake 3.25 or newer, Ninja, and C99/C++20 compilers.

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug
```

Build output stays under `out/`. See the [development guide](docs/development.md)
for release, sanitizer, WebAssembly, installation, and focused test commands.

## Embedding the engine

The primary C++ interface is `game_rules::Engine` in
[`game_rules/engine.hpp`](include/game_rules/engine.hpp). An engine instance
owns one level, its current resolved state, and its rewind history. Loading a
new valid level replaces all three atomically.

Hosts that cannot use the C++ interface can use the opaque C API in
[`game_rules/c_api.h`](include/game_rules/c_api.h). Native foreign-language
frontends can use its fixed-width typed data ABI without serializing commands,
snapshots, or events as JSON. The WebAssembly build wraps the retained JSON ABI
with a small JavaScript interface. See the [embedding API](docs/embedding-api.md)
for both contracts.

Levels can be built as C++ values or loaded from the strict, versioned JSON
format described in the [level format reference](docs/level-format.md). A
[JSON Schema](docs/level-format.schema.json) is included for editor-side
validation.

## Design principles

The core is portable C++20 with no dependency on a renderer, filesystem,
platform runtime, wall clock, threads, or randomness. It computes each tick
from an immutable snapshot, commits transitions atomically, and uses explicit
ordering rules wherever effects happen simultaneously. The same authored
contract scenarios run through the native C and WebAssembly boundaries.

## Documentation

- [Gameplay rules](docs/rules.md) — the complete normative behavior specification
- [Level format](docs/level-format.md) — version 1 JSON wire format
- [Embedding API](docs/embedding-api.md) — C and JavaScript/WebAssembly interfaces
- [Architecture](docs/architecture.md) — ownership, portability, and determinism
- [Development guide](docs/development.md) — builds, tests, and dependencies
- [Unreal integration](integrations/unreal/README.md) — plugin scaffold and staging

Third-party licensing and provenance are recorded in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

