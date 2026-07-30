# Release readiness checklist

This checklist is a release gate, not a claim that every downstream platform has already shipped.

## Versioning and ABI

- [ ] Choose and tag a semantic package version. The extraction baseline is CMake project version
  `0.1.0`; changing it requires an explicit release decision.
- [ ] Confirm `game_rules_api_version() == 1` and `game_rules_data_api_version() == 1`.
- [ ] Diff `include/game_rules/c_api.h` against the audited frozen header. Do not reorder fields,
  change widths, change function signatures, or rename symbols under the same ABI version.
- [ ] Treat `c_allocator_api.h` as the additive allocator-extension contract, independently
  versioned at version 1.
- [ ] Compile and link public-header consumers as strict C17 and C++17.
- [ ] Inspect the archive exports and confirm that all public names use the `game_rules_` prefix
  and no C++ ABI/runtime symbol is present.

## Supported toolchains and platforms

- [ ] Pass strict warnings-as-errors builds on genuine GCC and Clang.
- [ ] Pass the Windows MSVC CI build.
- [ ] Pass macOS and Ubuntu native CI builds.
- [ ] Pass Ubuntu ASan/UBSan runtime tests. Do not use the known-stalling Apple Silicon/macOS
  sanitizer runtime as release evidence.
- [ ] Pass Emscripten wasm32 build, ABI-layout tests, and Node smoke tests.
- [ ] Add a native ILP32 job if a non-wasm 32-bit target becomes supported.

The baseline support claim is portable C17 on current CI versions of Ubuntu/GCC or Clang,
macOS/AppleClang, Windows/MSVC, and Emscripten wasm32. Pin exact release toolchain versions in the
release notes.

## Ownership and allocator contract

- [ ] Document whether the host uses `game_rules_engine_create()` or
  `game_rules_engine_create_with_allocator_v1()`.
- [ ] Ensure a custom allocator context outlives its engine and every outstanding result.
- [ ] Dispose each typed owning result exactly once with its matching dispose function.
- [ ] Free each legacy JSON result exactly once with `game_rules_string_free()`.
- [ ] Verify allocation-failure rollback and engine/result lifetime tests on the release build.

## Artifacts and licensing

- [ ] Name the static archive `libgame_rules_state_c.a` on Unix-like hosts and
  `game_rules_state_c.lib` on MSVC.
- [ ] Ship both public headers under `include/game_rules/`.
- [ ] Ship CMake package files providing `GameRules::StateC`.
- [ ] Ship `LICENSE`, `THIRD_PARTY_NOTICES.md`, `vendor/yyjson/LICENSE`, and
  `vendor/yyjson/README.game-rules.md`.
- [ ] Verify yyjson source/header/license checksums against the pinned provenance document.
- [ ] Record compiler, platform, architecture, build type, package version, and ABI versions for
  every produced binary.

## Host handoffs

- [ ] Odin: translate fixed-width fields directly, represent the engine as an opaque pointer,
  preserve C calling convention, validate both API versions, and mirror each result-dispose call.
- [ ] WebAssembly: export only the required `game_rules_` C functions, retain the version-1 JSON
  ownership wrapper if used, run wasm ABI/layout tests, and verify no C++ runtime symbols.
- [ ] Unreal: stage one platform-compatible static archive plus public headers, link it from a thin
  C++ module with `extern "C"` declarations supplied by the header, and keep Unreal objects and
  allocators outside the core unless using the explicit allocator extension.
- [ ] Run a packaged Unreal integration test before claiming Unreal platform support.

See [embedding-api.md](embedding-api.md) and [unreal-embedding.md](unreal-embedding.md) for concrete
consumer guidance.
