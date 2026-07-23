# Development setup

## Native build

Prerequisites are CMake 3.25 or newer, Ninja, and a C++20 compiler.

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

Build artifacts stay under `out/` and are ignored by Git.

The phase-one loader performs structural validation and canonical storage only. It intentionally
accepts unsupported initial entities because initialization stabilization depends on the later
falling, ramp, fixture, and explosion phases. Until those phases land, `loaded_level()` is a loaded
definition snapshot rather than a claim that gameplay initialization has resolved.

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

## Unreal build

See `integrations/unreal/README.md`. Unreal integration is a separate consumer build; the standalone
CMake build intentionally does not require an Unreal installation.

