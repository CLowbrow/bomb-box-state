# C17 port independent adversarial audit

Date: 2026-07-29

## Recommendation

- **Behavioral parity: GO.** All authored differential corpora and 2,096 additional generated
  operations match the C++ reference, including complete responses and byte serialization. No
  normative or golden-output dispute was found.
- **Separate-repository extraction: GO.** An outside-repository copy containing only `c-port/`
  configured, built, passed 15/15 tests, installed, and linked strict standalone C and C++
  consumers. The extracted repository should retain Linux sanitizer execution, native GCC/MSVC,
  and eventual packaged-Unreal checks as release gates.
- **Unconditional production/memory-safety sign-off: NO-GO on local evidence alone.** Repository
  policy forbids running Apple ASan/UBSan tests on this host. The already-configured Ubuntu
  sanitizer CI job must pass the audited tree before release.

## Findings, ordered by severity

### High — production WebAssembly tested the C++ reference, not the C17 candidate (fixed)

Before this audit, `game_rules_wasm` linked `GameRules::State`; consequently the Node wasm corpus
could pass while exercising the reference implementation. This invalidated the prior claim that
the final wasm delivery path proved the C engine.

The minimized regression was a configure-time assertion requiring the exact direct link target.
Added before the implementation change, it failed with:

```text
game_rules_wasm must link GameRules::StateC directly; got: GameRules::State
```

The smallest fix changes the empty link translation unit to C17 and links `GameRules::StateC`
directly. The guard is retained at `integrations/wasm/CMakeLists.txt:1-13`, and the real-browser
regression is `integrations/wasm/browser_smoke.html:1`. Verbose Emscripten output now links
`c-port/libgame_rules_state_c.a`; the wasm symbol table contains no C++ standard-library, RTTI,
exception, operator-new/delete, or `__cxa` symbol.

### Medium — frozen ABI lacked executable layout and language/linkage proofs (fixed)

The candidate header drift check proved file identity but did not prove target layouts, enum
representation, callable prototypes, or C++ inclusion. The audit added C17 compile-time size,
alignment, field-offset, fixed-width, enum-value/width, and function-pointer checks for LP64 and
wasm32 at `c-port/tests/abi_layout_test.c:7`. It added exact C++ function-type and `extern "C"`
compatibility checks at `c-port/tests/header_cpp_compatibility_test.cpp:1`.

The pinned reference and candidate `c_api.h` remain byte-identical (SHA-256
`8225762905e716c60686eb98547a4612707c69400367d8a5ef82dddbc2e5e2c7`). The candidate and reference
yyjson source, header, and license also remain byte-identical. No public header was changed.

### Medium — malformed boundary and generated-transcript coverage was incomplete (fixed)

The prior suite had strong authored scenarios but no one test that systematically combined
truncated JSON, malformed UTF-8, embedded NULs, depth, numeric limits, arbitrary bytes, typed
pointer/count/tag combinations, and result-backed input aliasing. The deterministic boundary
corpus is now at `c-port/tests/boundary_fuzz_test.c:26`; it includes every truncation of a valid
document, 4,096 seeded byte buffers, and state-preservation assertions.

The new generated-level differential test at `tests/c-port/randomized_valid_levels_test.py:17`
uses eight fixed seeds, canonical and reordered equivalent arrays, two locales, maximum `uint64_t`
IDs, mixed moves/rewinds/snapshots, and byte-identical candidate repetition. It matched 2,096
complete operations. No behavioral mismatch was found, so no golden output needed changing and
there was no behavioral defect requiring a saved level regression.

### Low — status documentation overstated completed verification (fixed)

The previous status counted 11 standalone, 143 parent-native, and 12 parent-wasm tests and said
sanitizer runtime was the only incomplete check. It also failed to disclose that the production
wasm module linked the reference engine. `docs/c-port-status.md` now records current counts and
separates passed, unavailable/blocked, and disputed checks. Architecture and development guidance
now state which engine the final wasm path links.

### Low — `BUILD_TESTING=OFF` still built all candidate tests (fixed)

The standalone production tree registered tests conditionally but created every C test executable
unconditionally, so the documented library-only setting still compiled audit programs. The new
self-test at `c-port/tests/library_only_build_test.cmake:1` first failed by successfully building
`game_rules_candidate_lifecycle_smoke` from an `OFF` configuration. Candidate test targets are now
scoped under `BUILD_TESTING` in `c-port/CMakeLists.txt:76-260`; the same regression passes and the
library-only build exposes no candidate test target.

## Ownership, rollback, and resource review

No additional ownership defect was found.

- Each allocation carries its allocator context and deallocator, and disposal clears result
  structs (`c-port/src/c_api.c:52-79`, `c-port/src/c_api.c:345-371`). Results therefore remain
  independently disposable after replacement or engine destruction.
- Session construction frees every partial allocation and publishes only the completed replacement
  (`c-port/src/c_api.c:180-214`). JSON and typed replacement commit only after the complete result
  exists (`c-port/src/api_operations.c:506-558`, `c-port/src/api_operations.c:956-983`).
- A move prepares history before resolution and links history plus swaps state only at final commit
  (`c-port/src/state.c:1497-1502`, `c-port/src/state.c:1593-1601`). Rewind serializes its result before
  consuming history (`c-port/src/api_operations.c:1132-1154`).
- Allocation injection walks every successful allocation index for JSON and typed load, get-state,
  move/resolution, rewind, history growth, and replacement. Every injected failure verifies state,
  history, live-allocation baseline, retry, and invalid-free count
  (`c-port/tests/allocation_failure_test.c:1214-1308`).
- Apple `leaks --atExit` reported zero leaks for the allocation-failure, boundary-fuzz, explosion,
  and history executables.

## ABI, portability, wasm, and Unreal boundary

- Public fields use fixed-width integers; counts are `uint32_t`; no C++ type crosses the ABI.
  Exact default-C calling prototypes compile in C17 and C++17. LP64 and wasm32 layouts pass.
- Strict warnings-as-errors builds pass with AppleClang 17 and Emscripten Clang 6.0.3. No second
  native compiler family is installed: `/usr/bin/gcc` is an AppleClang alias. Existing CI covers
  Ubuntu, macOS, Windows, Ubuntu sanitizers, and wasm (`.github/workflows/ci.yml:10-47`), but those
  remote jobs were not invoked during this local audit.
- There is no native ILP32 runner. The wasm32 ABI/layout/build/test result exercises 32-bit pointer
  and `size_t` arithmetic, but is not a substitute for native ILP32.
- The production wasm module passed 15/15 tests in Node 26.5.0. A real in-app browser loaded the ES
  module over HTTP and passed load/move/rewind/state with no console warning or error.
- The C archive is usable by a thin C++ wrapper without RTTI, exceptions, global platform state, or
  C++ ABI exposure. The current Unreal scaffold intentionally still names the C++ reference
  archive (`integrations/unreal/GameRulesState/Source/ThirdParty/GameRulesCore/GameRulesCore.Build.cs:16-24`);
  it is not evidence that the candidate was packaged for Unreal. The extracted C port will require
  per-platform staging of `game_rules_state_c` and a packaged Unreal integration test.

## Static analysis, fuzzing, determinism, and performance

- Parent and standalone ASan/UBSan builds complete. Their runtime tests were not run because
  `AGENTS.md` explicitly forbids `ctest --preset native-sanitized` on this Apple Silicon/macOS host.
  This is an unavailable check, not a pass.
- Clang static analysis emitted one null-dereference warning at `c-port/src/api_operations.c:452`.
  Investigation showed the only caller at line 543 is reached only after replacement construction
  at lines 515-520 returned non-null; allocation failure returns first. The warning is infeasible
  and was not suppressed.
- Deterministic parser/typed-boundary fuzzing passed. No coverage-guided, long-duration fuzzer was
  configured, so that broader campaign remains unavailable rather than implicitly passed.
- Both the 47-operation explosion differential and the 2,312-operation seeded history/lifecycle
  differential passed 100 consecutive executions. Generated tests additionally varied allocation
  history, input order, locale, IDs, and repeat execution.
- Structural review found checked arithmetic and the documented 16 MiB JSON bound. Per-command
  physics may revisit entities across stabilization ticks, and rewind history grows linearly with
  accepted commands by design. No accidental unbounded recursion, hash-order dependency, result
  aliasing, or release-significant quadratic duplication was identified. No speculative
  performance rewrite was made.

## Extraction proof

The audit copied only `c-port/` to `/private/tmp/game-rules-c17-audit-final.y8EGcv` and performed a clean
Ninja debug configuration with tests and warnings as errors. Results:

- build succeeded; candidate-only CTest: 15/15 passed;
- install emitted `libgame_rules_state_c.a`, both public headers, third-party notices, yyjson
  license, and provenance README;
- installed strict C17 and C++17 consumers both linked and ran;
- no symlinks, parent-relative includes, absolute source/build paths, reference source dependency,
  missing license, or reliance on the parent C++ checkout was found.

The path above is temporary audit evidence, not a repository input.

## Commands and results

```sh
cmake --preset native-debug
cmake --build --preset native-debug
ctest --preset native-debug --output-on-failure
# 148/148 passed

ctest --preset native-debug \
  -R 'game_rules.differential.explosions_parity|game_rules.differential.seeded_history_lifecycle' \
  --repeat until-fail:100 --output-on-failure --quiet
# exit 0

cmake --preset native-sanitized
cmake --build --preset native-sanitized
# build passed; tests intentionally not run on this host

emcmake cmake --preset wasm-debug
cmake --build --preset wasm-debug
ctest --preset wasm-debug --output-on-failure
# 15/15 passed

cmake -S c-port -B out/c-port-audit-sanitized -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
  -DGAME_RULES_C_WARNINGS_AS_ERRORS=ON -DGAME_RULES_C_ENABLE_SANITIZERS=ON
cmake --build out/c-port-audit-sanitized
# build passed; tests intentionally not run on this host
```

The clean outside-copy build/install commands and real-browser HTTP smoke were also run as
described above. Full raw CTest output is not committed as source documentation.

## Remaining risks and required handoffs

1. Run both sanitizer test trees on the existing Ubuntu CI job or another known-working Linux
   host; any sanitizer finding reopens the recommendation.
2. Run the strict standalone tree with a genuine native GCC family compiler and retain the
   existing MSVC CI result before release.
3. If native 32-bit support is a shipping requirement, add a real ILP32 build/test job; wasm32 is
   useful but insufficient proof.
4. Add a packaged per-platform Unreal wrapper test when that separate integration stage begins.
5. Consider a time-bounded coverage-guided JSON/typed ABI fuzz campaign for release hardening.

No C++ target, frozen API, normative rule, reference implementation, or authored golden output was
removed or modified. No commit or push was made.
