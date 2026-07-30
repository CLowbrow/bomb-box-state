# Game rules state — C17 library

This directory is an extraction-ready standalone distribution of the deterministic, headless
game-rules engine. Production code is portable C17. The stable public boundary is
`include/game_rules/c_api.h`; the additive allocator extension is
`include/game_rules/c_allocator_api.h`. Repository or game-title changes must not rename the
public symbols, the `game_rules_state_c` archive, or the `GameRules::StateC` CMake target.

The host owns rendering, animation, audio, input mapping, persistence, and scheduling. Each engine
instance owns one independent loaded level, resolved state, and undo-only history. The core uses no
filesystem, environment, clock, thread, randomness, renderer, Unreal, or Emscripten API.

## Build and test

Requirements are CMake 3.25 or newer, Ninja, a C17 compiler, and an optional C++17 compiler for the
public-header compatibility probe.

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug
```

The preset enables strict warnings as errors, all candidate-only behavior/ABI/ownership tests, and
the embedding smoke example. A library-only build is:

```sh
cmake -S . -B out/build/library -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF -DGAME_RULES_C_BUILD_EXAMPLES=OFF \
  -DGAME_RULES_C_WARNINGS_AS_ERRORS=ON
cmake --build out/build/library
cmake --install out/build/library --prefix out/install
```

On a supported Linux host, run the sanitizer preset:

```sh
cmake --preset native-sanitized
cmake --build --preset native-sanitized
ctest --preset native-sanitized
```

Repository policy for the source checkout from which this package was extracted forbids executing
the Apple sanitizer runtime on Apple Silicon/macOS because test discovery stalls there. The
standalone CI therefore executes ASan/UBSan on Ubuntu.

## WebAssembly

Install Emscripten and Node, then run the same production source and candidate-only suite as wasm32:

```sh
emcmake cmake --preset wasm-debug
cmake --build --preset wasm-debug
ctest --preset wasm-debug
```

The wasm CTest suite runs the generated C executables through Node. It covers the full C test
corpus, ABI layout, allocation failures, and the native/wasm smoke example without a C++ runtime.
A browser or JavaScript adapter may wrap the retained version-1 JSON ABI; adapter ownership and
presentation behavior remain outside this library.

## Consume with CMake

Use `add_subdirectory()` or install the package and call `find_package(GameRulesStateC CONFIG)`.
Both provide the stable `GameRules::StateC` target:

```cmake
find_package(GameRulesStateC CONFIG REQUIRED)
target_link_libraries(host PRIVATE GameRules::StateC)
```

The installed package contains `libgame_rules_state_c.a`, both public headers, CMake package
metadata, project and third-party licenses, provenance, and documentation.

## Contracts and documentation

- [Architecture](docs/architecture.md)
- [C, Odin, wasm, and ownership API](docs/embedding-api.md)
- [Level format](docs/level-format.md) and [JSON Schema](docs/level-format.schema.json)
- [Normative rules](docs/rules.md)
- [Unreal embedding](docs/unreal-embedding.md)
- [Release readiness](docs/release-readiness.md)
- [Transition-period differential parity](docs/parity-transition.md)
- [Exact extraction manifest](docs/extraction-manifest.md)

yyjson 0.12.0 is the sole vendored source dependency. Its unmodified files, license, pinned
checksums, private-symbol configuration, and update procedure are under `vendor/yyjson/` and in
`THIRD_PARTY_NOTICES.md`.
