# Development setup

## Native build

Prerequisites are CMake 3.25 or newer, Ninja, and a toolchain with C99 and C++20 compilers.

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug
```

For memory and undefined-behavior checks on Clang or GCC:

```sh
cmake --preset native-sanitized
cmake --build --preset native-sanitized
ctest --preset native-sanitized
```

Build artifacts stay under `out/` and are ignored by Git.

See the [implementation status](implementation-status.md) for current feature coverage, verification
results, and known toolchain limitations.

## Native gameplay loop

`bomb_box::Engine` accepts cardinal movement after a valid level load. An
accepted `MoveResult` contains the complete initial state, ordered tick events
and states, final state, and outcome. A rejected result contains a stable
`MoveStatus`, a presentation-only `MoveBlockedEvent` for gameplay rejections,
and the unchanged authoritative state when one exists.

```cpp
const bomb_box::MoveResult moved = engine.move(bomb_box::Direction::east);
if (moved.accepted()) {
    for (const bomb_box::TickResult& tick : moved.ticks) {
        // Render tick.events, then use tick.state_after as authoritative.
    }
}

const bomb_box::RewindResult rewound = engine.rewind();
if (rewound.accepted()) {
    // rewound.state is the restored authoritative command-boundary state.
}
```

The current movement scope is flat walking. Pushes, initialization gravity,
ramp traversal, and fixture effects remain later phases; their boundaries are
reported explicitly instead of partially resolving a turn.

## Level JSON

The public in-memory representation is in `bomb_box/world.hpp`; the portable JSON codec is
in `bomb_box/level_json.hpp`. Generic JSON syntax parsing is provided by the pinned yyjson 0.12.0
source under `vendor/yyjson`; the public Bomb Box API and rule-specific decoding do not expose
yyjson. Decode untrusted bytes, check `accepted()`, and then pass the returned definition through the
normal engine load boundary:

```cpp
const bomb_box::DecodeLevelJsonResult decoded = bomb_box::decode_level_json(bytes);
if (decoded.accepted()) {
    const bomb_box::LoadResult loaded = engine.load_level(*decoded.level);
}
```

Format errors have a `LevelJsonErrorCode`. Syntax failures have a byte offset; shape failures have a
JSON Pointer path and use offset zero because the parsed DOM does not retain source positions.
Structurally invalid levels instead return the shared `ValidationError` list. Encoding also
validates and emits no bytes for an invalid definition.

See the [version 1 format specification](level-format.md) and its
[JSON Schema](level-format.schema.json). The schema can be used by a web editor for immediate shape
feedback, but authoritative acceptance must use the core decoder/validator because ramp endpoints,
complete cell coverage, stacks, and other cross-entry rules are semantic checks.

## Vendored dependencies

yyjson is the only approved source dependency in the portable core. Its upstream files must remain
unmodified. Provenance, checksums, license, isolation details, and the update procedure are recorded
in `vendor/yyjson/README.bomb-box.md`; the repository-level attribution is in
`THIRD_PARTY_NOTICES.md`. The installed package includes the upstream MIT license.

## WebAssembly build

Install the Emscripten SDK, activate it, and load its environment in each new shell. The SDK supplies
Emscripten's Clang, Node.js, and related tools.

```sh
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

Then, from this repository:

```sh
emcmake cmake --preset wasm-debug
cmake --build --preset wasm-debug
ctest --preset wasm-debug
```

The generated ES module and `.wasm` binary are placed in `out/build/wasm-debug/wasm/`. Pin an SDK
version in CI and release builds once the engine moves beyond initial development.

## Unreal build

See `integrations/unreal/README.md`. Unreal integration is a separate consumer build; the standalone
CMake build intentionally does not require an Unreal installation.
