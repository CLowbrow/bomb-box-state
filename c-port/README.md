# C17 candidate engine

This directory is the self-contained production candidate for the C17 rewrite. Stage 10 preserves
the frozen public ABI and complete stage-09 rules boundary, then completes undo-only resolved-state
history, repeated and terminal rewind, branching, replacement isolation, independent result
lifetimes, and allocation-failure rollback across native and WebAssembly builds.

The frozen creation API uses the C runtime allocator. The additive, versioned
`game_rules/c_allocator_api.h` extension permits an embedding host or deterministic test to supply
allocate/deallocate callbacks without changing `c_api.h`. An engine explicitly owns its active
session scaffold. Replacement builds all new storage before swapping it into the engine, and
allocation failure leaves the prior session untouched. Movement similarly plans into scratch,
allocates the complete caller-owned response, and swaps authoritative state only on success.

Every typed result owns a separate, contiguous allocation arena through `owned_storage`, and legacy
JSON results are independent caller-owned allocations. All nested arrays and views point inside
that arena rather than introduce separately freed child allocations. Each
allocation remembers its deallocator, so
destroying or replacing an engine never invalidates an already returned result. A custom allocator
context must remain usable until both the engine and all outstanding results have been released.
Accepted commands resolve inside private working state buffers and retain immutable per-tick
snapshots before either serializer allocates its result. Only a complete result commits the final
state to the session. No renderer, filesystem, environment, clock, thread, randomness, or platform
API is used.

Every accepted move owns one canonical pre-command history arena regardless of tick count. A
rejected move owns none. Move history and complete results are allocated before current state is
committed; rewind results are allocated before the top entry is restored and consumed. Valid
replacement destroys the old chain only after the new session and response are complete, while an
invalid or allocation-failed replacement retains the old level, current state, and chain.

Standalone native verification:

```sh
cmake -S c-port -B out/c-port-native -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build out/c-port-native
ctest --test-dir out/c-port-native --output-on-failure
```

The standalone tree includes ABI size/alignment/offset and C/C++ header probes,
typed/JSON boundary fuzz cases, and exhaustive allocation-index injection in
addition to the gameplay suites. A C++ compiler is optional and is used only
for the consumer-header probe; the library and all production sources remain
C17. A nested build regression also proves that `BUILD_TESTING=OFF` exposes
only the library and install targets, not candidate test executables.

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

The parent WebAssembly build additionally produces
`out/build/wasm-debug/wasm/browser_smoke.html`, which imports the generated ES
module in a real browser. The parent link target has a configure-time guard
requiring `GameRules::StateC` directly.
