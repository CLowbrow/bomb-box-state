# Unreal Engine integration

This directory is a plugin scaffold, not a staged binary plugin yet. Keep the rules implementation
in the portable core and keep Unreal-specific types, reflection, Blueprint exposure, and animation
translation in the `BombBoxState` runtime module.

## Why the core is external

Unreal Build Tool needs libraries built for the exact target platform and a compatible compiler and
runtime. A macOS archive built on this machine cannot be reused for Win64, Linux, consoles, or a
different architecture. The `BombBoxCore` external module therefore expects one archive per Unreal
target rather than committing a host-only binary.

The core defaults to no RTTI and no exceptions, matching the plugin module settings and avoiding the
most common Unreal third-party-library flag mismatch. Its public C ABI is the safest initial wrapper
boundary. The C++ API is available too, but prebuilt C++ ABI compatibility must be verified for every
Unreal toolchain you ship.

## Staging a platform build

1. Configure a release build using the same architecture and supported compiler toolchain as your
   Unreal target.
2. Install it to a temporary directory:

   ```sh
   cmake --preset native-release
   cmake --build --preset native-release
   cmake --install out/build/native-release --prefix out/install/unreal
   ```

3. Copy `out/install/unreal/include/bomb_box` into
   `BombBoxState/Source/ThirdParty/BombBoxCore/include/bomb_box`.
4. Copy the installed library into the matching `lib/Mac`, `lib/Win64`, or `lib/Linux` directory.
5. Copy or link the `BombBoxState` plugin directory into an Unreal project's `Plugins` directory,
   regenerate project files, and build the project.

Before shipping, automate step 3 and 4 in a per-platform packaging job and test a packaged Unreal
build. Do not assume a library that links in the editor is valid for every packaged target.

