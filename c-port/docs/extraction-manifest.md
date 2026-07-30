# Exact extraction manifest

This manifest describes every file intended to exist at the root of the extracted repository.
Paths are relative to the standalone root. There are 47 files, no symlinks, and no generated
build artifact. Copy exactly these paths; do not copy the parent C++ implementation, parent
contracts/golden outputs, differential runners/scripts, generated differential data, build trees,
editor state, or local SDK paths.

## Production source (8)

```text
src/api_operations.c
src/c_api.c
src/c_api_internal.h
src/level.c
src/level_json.c
src/level_json_yyjson.c
src/level_json_yyjson.h
src/state.c
```

## Public headers (2)

```text
include/game_rules/c_allocator_api.h
include/game_rules/c_api.h
```

## Candidate-only tests (14)

```text
tests/abi_layout_test.c
tests/allocation_failure_test.c
tests/boundary_fuzz_test.c
tests/explosions_test.c
tests/falling_test.c
tests/fixtures_test.c
tests/flat_walking_test.c
tests/header_cpp_compatibility_test.cpp
tests/history_test.c
tests/level_loading_test.c
tests/lifecycle_test.c
tests/player_pushing_test.c
tests/ramps_test.c
tests/wasm_smoke_test.c
```

## Build and CI support (6)

```text
.github/workflows/ci.yml
.gitignore
CMakeLists.txt
CMakePresets.json
cmake/GameRulesStateCConfig.cmake.in
tests/library_only_build_test.cmake
```

## Documentation and licensing (12)

```text
LICENSE
README.md
THIRD_PARTY_NOTICES.md
docs/architecture.md
docs/embedding-api.md
docs/extraction-manifest.md
docs/level-format.md
docs/level-format.schema.json
docs/parity-transition.md
docs/release-readiness.md
docs/rules.md
docs/unreal-embedding.md
```

## Examples (1)

```text
examples/engine_smoke.c
```

## Vendored dependency (4)

```text
vendor/yyjson/LICENSE
vendor/yyjson/README.game-rules.md
vendor/yyjson/yyjson.c
vendor/yyjson/yyjson.h
```

The vendored set is pinned yyjson 0.12.0. Its provenance file records upstream and per-file
SHA-256 values. All four files are retained; none is generated.

## Audit disposition

Every pre-extraction file formerly under `c-port/` is retained and classified above:
8 production-source files, 2 public headers, 14 candidate-only executable test sources, 1
candidate build-regression script, 2 root documents, 1 root CMake file, and 4 yyjson files.
The extraction adds only standalone build/CI metadata, documentation, licensing, and one public
embedding example. No gameplay source, public ABI declaration, authored expected output, or
reference implementation was changed or removed.
