# C17 candidate engine

This directory is the self-contained production candidate for the C17 rewrite. Stage 00 preserves
the frozen public ABI and implements only allocation, destruction, result disposal, and no-level
lifecycle responses. Level loading deliberately returns `not_implemented` through the legacy JSON
surface; the typed load entry point returns `GAME_RULES_CALL_INVALID_ARGUMENT` because version 1 of
the frozen typed ABI has no not-implemented operation status.

The candidate uses the C runtime allocator (`malloc`/`free`). An engine owns all future mutable
session state. Every typed result will own a separate allocation graph through `owned_storage`, and
legacy JSON results are independent caller-owned allocations. Destroying an engine never
invalidates an already returned result. No renderer, filesystem, environment, clock, thread,
randomness, or platform API is used by the library.

Standalone native verification:

```sh
cmake -S c-port -B out/c-port-native -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build out/c-port-native
ctest --test-dir out/c-port-native --output-on-failure
```

Sanitizer-ready configuration (execute tests on a supported Linux host, not this Apple Silicon
macOS host):

```sh
cmake -S c-port -B out/c-port-sanitized -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON -DGAME_RULES_C_ENABLE_SANITIZERS=ON
cmake --build out/c-port-sanitized
```

WebAssembly smoke build:

```sh
emcmake cmake -S c-port -B out/c-port-wasm -G Ninja -DBUILD_TESTING=ON
cmake --build out/c-port-wasm --target game_rules_candidate_wasm_smoke
ctest --test-dir out/c-port-wasm --output-on-failure -R game_rules.candidate.wasm_smoke
```
