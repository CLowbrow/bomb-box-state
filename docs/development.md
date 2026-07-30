# Development guide

## Native build

Prerequisites:

- CMake 3.25 or newer
- Ninja
- C99 and C++20 compilers

Configure, build, and run the full native debug suite:

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug
```

Build artifacts stay under `out/` and are ignored by Git. The first test build
downloads the pinned GoogleTest release and verifies its archive hash.
GoogleTest is only a build dependency; it is not linked into or installed with
the engine.

For an optimized build, use the matching release preset:

```sh
cmake --preset native-release
cmake --build --preset native-release
ctest --preset native-release
```

## Focused tests

CTest labels provide quick subsets while iterating:

```sh
ctest --preset native-debug -L unit
ctest --preset native-debug -L behavior
ctest --preset native-debug -L consumer
```

The test tree is organized by purpose:

- `tests/unit` contains focused checks of internal algorithms.
- `tests/behavior` exercises complete public engine operations and compares
  complete ordered results.
- `tests/contracts` contains authored operation scripts shared by the native C
  and Node/WebAssembly runners.
- `tests/consumer` verifies the installed-style headers, C boundary, and
  production no-exceptions/no-RTTI configuration.
- `tests/adapter` exercises both the typed native data ABI and the compatibility
  JSON ABI, including ownership and invalid foreign input.
- `tests/support` contains test-only builders, printers, and matchers.

When adding or changing behavior, prefer a public scenario under
`tests/behavior` and cover rejected input, boundary cases, rewind, and
determinism where they apply.

## Sanitizers

Clang and GCC builds can enable AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
cmake --preset native-sanitized
cmake --build --preset native-sanitized
ctest --preset native-sanitized
```

Do not run the final command on Apple Silicon/macOS. The Apple sanitizer
runtime stalls during GoogleTest discovery on this host. Configure and build
locally if useful, but execute the tests with the Ubuntu sanitizer CI job or
another known-working Linux environment.

## Building without tests

Consumers that only need the library can disable the test tree:

```sh
cmake --preset native-release -DBUILD_TESTING=OFF
cmake --build --preset native-release
```

To install the CMake package into a chosen prefix:

```sh
cmake --install out/build/native-release --prefix out/install
```

The installed target is `GameRules::State`. Public headers are installed under
`include/game_rules`, together with the package configuration and required
third-party notices.

## WebAssembly

Install and activate an Emscripten SDK, then configure through `emcmake`:

```sh
emcmake cmake --preset wasm-debug
cmake --build --preset wasm-debug
ctest --preset wasm-debug
```

The ES module and `.wasm` file are generated under
`out/build/wasm-debug/wasm/`. The test runs the same authored contract scenarios
used by the native C runner. The production module links the C17
`GameRules::StateC` archive directly; configuration fails if that target is
replaced by the C++ reference library. The generated JavaScript interface is
documented in the [embedding API](embedding-api.md).

`browser_smoke.html` is copied beside the generated module for a real-browser
load/move/rewind/state smoke check. Serve that output directory over HTTP; ES
module and wasm loading are not reliable from a `file:` URL.

The vendored yyjson object is compiled at `-O1` even in an Emscripten debug
build. At `-O0`, current Emscripten releases lower its reader beyond
WebAssembly's per-function local limit; the engine and adapter keep the normal
debug settings.

## Level JSON

The in-memory model is declared in `game_rules/world.hpp`; the portable codec
is in `game_rules/level_json.hpp`. Decode bytes and check the result before
loading the returned definition:

```cpp
const game_rules::DecodeLevelJsonResult decoded =
    game_rules::decode_level_json(bytes);

if (decoded.accepted()) {
    const game_rules::LoadResult loaded = engine.load_level(*decoded.level);
}
```

The [level format reference](level-format.md) describes the wire contract. Its
[JSON Schema](level-format.schema.json) is useful for editor feedback, but the
core decoder and validator remain authoritative for cross-entry rules.

Native foreign-language hosts do not need to use this codec at runtime. The
typed C data ABI accepts `game_rules_level_definition` arrays directly and
returns typed snapshots, ticks, and events. See the [embedding API](embedding-api.md)
for its ownership rules; adapter changes must retain the C99 header smoke test
and add coverage under `tests/adapter/`.

## Dependencies and integrations

yyjson is the only vendored source dependency in the portable core. Keep its
upstream files unmodified. Provenance, checksums, symbol-isolation details, and
the update procedure are in `vendor/yyjson/README.game-rules.md`; attribution
is in [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).

For Unreal staging and platform-library requirements, see the
[Unreal integration guide](../integrations/unreal/README.md).

## C17 candidate and differential harness

The stage-10 initialization, snapshot, flat/ramp movement, whole-stack sliding, gravity, fixture,
explosion-wave, terminal, causal-closure, and undo-history C candidate can be configured without
the parent checkout:

```sh
cmake -S c-port -B out/c-port-native -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build out/c-port-native
ctest --test-dir out/c-port-native --output-on-failure
```

The ordinary native preset also builds the candidate, checks its pinned public
header, and builds the two independent differential runners:

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug -L candidate
```

To compare the completed browser rewind slice directly:

```sh
python3 tools/c-port/compare_transcript.py \
  out/build/native-debug/tests/game_rules_reference_runner \
  out/build/native-debug/tests/game_rules_candidate_runner \
  tests/contracts/browser_vertical_slice/v1/contract.txt
```

Normal comparison exits nonzero at the first difference and now reports all five operations as
matched. See `c-port/README.md` for sanitizer-ready and Emscripten commands, and
`docs/c-port-status.md` for the feature matrix and stage-11 audit inputs.

## Future extracted-C dependency integration

This extraction stage does not change any consumer or remove the C++ reference. After the
standalone C repository exists locally and its reviewed commit is known, perform the dependency
migration in a separate change:

1. Add a cache path for transition testing and replace only the current
   `add_subdirectory(c-port)` source location:

   ```cmake
   set(GAME_RULES_C_SOURCE_DIR "" CACHE PATH
       "Path to a reviewed game-rules C17 source checkout")
   if(NOT GAME_RULES_C_SOURCE_DIR)
       message(FATAL_ERROR "Set GAME_RULES_C_SOURCE_DIR to the extracted C17 checkout")
   endif()
   add_subdirectory(
       "${GAME_RULES_C_SOURCE_DIR}"
       "${CMAKE_BINARY_DIR}/_deps/game-rules-state-c"
       EXCLUDE_FROM_ALL
   )
   ```

2. Replace parent drift-check paths rooted at `c-port/` with
   `${GAME_RULES_C_SOURCE_DIR}/include/game_rules/c_api.h` and
   `${GAME_RULES_C_SOURCE_DIR}/vendor/yyjson/`. Keep comparing those inputs to the frozen
   parent header and yyjson pin during the transition.
3. Leave all existing links to the stable `GameRules::StateC` target unchanged. In particular,
   keep the candidate differential runner and wasm link guard intact.
4. Configure with an explicit reviewed checkout, then run the full native, 100-repeat
   differential, sanitizer-CI, wasm, browser, and install-consumer verification recorded in
   `docs/c-port-status.md`.
5. After parity passes against the external checkout, replace the cache-path mechanism with the
   project's chosen pinned dependency mechanism (submodule, package manager, or CMake
   `FetchContent` using an immutable commit). That choice and remote creation require their own
   reviewed change.

Only after this dependency step and final transition verification may a later change consider
removing the in-tree extracted copy or changing production ownership. The C++ implementation,
reference runner, golden outputs, and differential tooling remain in this repository until that
separate decision.
