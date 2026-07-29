# C17 candidate engine

This directory is the self-contained production candidate for the C17 rewrite. Stage 03 preserves
the frozen public ABI and implements strict version-1 JSON decoding, typed loading, complete world
schema validation, canonical immutable level and resolved-state ownership, full physical level
initialization, complete typed and JSON snapshots, and atomic replacement. Player commands and
rewind history are later stages.

The frozen creation API uses the C runtime allocator. The additive, versioned
`game_rules/c_allocator_api.h` extension permits an embedding host or deterministic test to supply
allocate/deallocate callbacks without changing `c_api.h`. An engine explicitly owns its active
session scaffold. Replacement builds all new storage before swapping it into the engine, and
allocation failure leaves the prior session untouched.

Every typed result owns a separate, contiguous allocation arena through `owned_storage`, and legacy
JSON results are independent caller-owned allocations. All nested arrays and views point inside
that arena rather than introduce separately freed child allocations. Each
allocation remembers its deallocator, so
destroying or replacing an engine never invalidates an already returned result. A custom allocator
context must remain usable until both the engine and all outstanding results have been released.
No renderer, filesystem, environment, clock, thread, randomness, or platform API is used.

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
cmake --build out/c-port-wasm
ctest --test-dir out/c-port-wasm --output-on-failure
```
