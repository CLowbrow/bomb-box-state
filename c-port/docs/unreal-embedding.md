# Thin Unreal C++ embedding

The library is not an Unreal module and must remain free of Unreal headers. Build
`game_rules_state_c` separately for each Unreal target platform and configuration, then stage:

```text
Source/ThirdParty/GameRulesCore/include/game_rules/c_api.h
Source/ThirdParty/GameRulesCore/include/game_rules/c_allocator_api.h
Source/ThirdParty/GameRulesCore/lib/<Platform>/<archive>
```

In the Unreal `.Build.cs`, add the include directory and the platform-specific static library to
`PublicIncludePaths` and `PublicAdditionalLibraries`. The public C header already supplies
`extern "C"` guards, so a C++ wrapper can include it directly.

The wrapper should own one `game_rules_engine*` per play session, translate Unreal input into
cardinal commands, copy or consume snapshots/events before disposing their result owner, and
destroy the engine deterministically. Keep `UObject` pointers, `FString`, `TArray`, delegates,
rendering, and scheduling outside the C boundary.

If routing allocations through Unreal, use only
`game_rules_engine_create_with_allocator_v1()`. Its allocator context must remain alive until
the engine and every outstanding result have been disposed. Never free a library result directly
with an Unreal or C runtime free function.

Release verification must package and launch an Unreal target for each claimed platform. A C++17
header/link smoke test proves language linkage but is not a substitute for packaged Unreal
staging, architecture, runtime-library, and configuration checks.
