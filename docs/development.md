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

On the current Apple Silicon/macOS development host, configure and build work,
but the Apple sanitizer runtime hangs while CTest performs GoogleTest discovery
before any case runs. This occurs in a normal terminal as well as in the Codex
sandbox, so do not run the final `ctest` command on this host. Use the Ubuntu
`native sanitizers` GitHub Actions job, or another known-working Linux
environment, to execute the sanitized suite. A local sanitized configure/build
remains useful for proving that all targets compile and link with the
instrumentation enabled.

Build artifacts stay under `out/` and are ignored by Git.

When native tests are enabled, the first configure downloads GoogleTest 1.17.0 from its exact
release commit and verifies its archive hash through CMake `FetchContent`. GoogleTest is a
build-only dependency: it is not linked into `GameRules::State`, exported, installed, or required by
an embedding host. Configure with `-DBUILD_TESTING=OFF` when only the library or an install package
is needed.

See the [implementation status](implementation-status.md) for current feature coverage, verification
results, and known toolchain limitations.

## Test organization

CTest remains the suite runner. GoogleTest supplies native test registration, assertions, and
failure diagnostics; `gtest_discover_tests()` registers each named GoogleTest case separately with
CTest. Useful filtered runs include:

```sh
ctest --preset native-debug -L unit
ctest --preset native-debug -L behavior
ctest --preset native-debug -L consumer
```

Native tests are divided by purpose:

- `tests/unit` contains focused white-box checks of internal algorithms where the public engine
  boundary would make failures unnecessarily difficult to localize.
- `tests/behavior` contains specification-level scenarios expressed through public operations. A
  scenario should construct complete input state, perform one public operation when practical, and
  compare the complete ordered result and resulting lifecycle state.
- `tests/support` contains test-only builders, printers, and matchers. Helpers should improve
  diagnostics without hiding the rule-relevant state in a scenario.
- `tests/consumer` contains framework-free consumer checks compiled with the same no-exceptions and
  no-RTTI policy as the core. The GoogleTest runners use their normal runtime while linking the
  already production-compiled core; sanitizer instrumentation is applied to both.

The adapter-neutral contract corpus for the stateful C ABI and WebAssembly surface currently runs
one authored browser vertical-slice scenario through both native C and Node/WebAssembly runners.
Native C++ unit tests do not need to be recompiled under Emscripten. Adapter-neutral scenarios
compare their normalized complete results against the same authored expectations; neither adapter's
live output is an oracle for the other. The test runner may own fixture files and platform
facilities; the engine remains headless and filesystem-independent. Expand this corpus as later
cross-adapter lifecycle and conflict coverage is implemented.

## Native gameplay loop

`game_rules::Engine` accepts cardinal movement after a valid level load. An
accepted `MoveResult` contains the complete initial state, ordered tick events
and states, final state, and outcome. A rejected result contains a stable
`MoveStatus`, a presentation-only `MoveBlockedEvent` for gameplay rejections,
and the unchanged authoritative state when one exists.

```cpp
const game_rules::MoveResult moved = engine.move(game_rules::Direction::east);
if (moved.accepted()) {
    for (const game_rules::TickResult& tick : moved.ticks) {
        // Render tick.events, then use tick.state_after as authoritative.
    }
}

const game_rules::RewindResult rewound = engine.rewind();
if (rewound.accepted()) {
    // rewound.state is the restored authoritative command-boundary state.
}
```

The current movement scope is flat and oriented ramp walking, atomic
single-entity pushes including downhill ramp entry, derived falling and
whole-stack sliding, single-source explosions, switches, doors, and exit
teleporters. A push emits the
player's movement event followed by the box or barrel movement event in the
same tick; fixture changes follow physical events. A push over a lower flat
support is completed by a derived fall tick, while a downhill ramp push is
completed by a later slide tick. Stacked targets, recursive pushes, closed
doors, teleporter restrictions, perpendicular ramp traversal, and other
blocked destinations reject without entering history.

`Engine::load_level()` runs initial fixture derivation and the same gravity and
ramp-slide planners before establishing the new history boundary. Its
`LoadResult` exposes the canonical supplied dynamic state as `initial_state`,
every initialization tick, the final authoritative state, and outcome.
Initialization derives active switch colors and effectively open doors,
recognizes an immediate teleporter win, and stabilizes gravity and ramp
movement. One settled armed barrel detonates only after those movements finish;
its blast tick may be followed by additional falls and slides. Barrels newly
armed by that blast remain in the resolved state's canonical `armed_barrels`
IDs until the later wave-and-chain phase is implemented.

## Level JSON

The public in-memory representation is in `game_rules/world.hpp`; the portable JSON codec is
in `game_rules/level_json.hpp`. Generic JSON syntax parsing is provided by the pinned yyjson 0.12.0
source under `vendor/yyjson`; the public game-rules API and rule-specific decoding do not expose
yyjson. Decode untrusted bytes, check `accepted()`, and then pass the returned definition through the
normal engine load boundary:

```cpp
const game_rules::DecodeLevelJsonResult decoded = game_rules::decode_level_json(bytes);
if (decoded.accepted()) {
    const game_rules::LoadResult loaded = engine.load_level(*decoded.level);
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
in `vendor/yyjson/README.game-rules.md`; the repository-level attribution is in
`THIRD_PARTY_NOTICES.md`. The installed package includes the upstream MIT license.

GoogleTest is fetched only while building native tests and is intentionally not vendored into or
installed with the portable core.

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

The generated module exposes the browser-facing `module.gameRules` interface documented in the
[embedding API](embedding-api.md). The Wasm CTest runs the same authored vertical-slice contract as
the native C ABI runner. The vendored yyjson object is compiled at `-O1` in Emscripten builds even
for the debug preset: at `-O0`, current Emscripten releases lower `yyjson_read_opts` beyond
WebAssembly's per-function local limit. The game-rules core and adapter otherwise retain the debug
preset's settings.

## Unreal build

See `integrations/unreal/README.md`. Unreal integration is a separate consumer build; the standalone
CMake build intentionally does not require an Unreal installation.
