# Vendored yyjson

The files `yyjson.c`, `yyjson.h`, and `LICENSE` in this directory are unmodified upstream files from
[ibireme/yyjson](https://github.com/ibireme/yyjson), release tag `0.12.0`.

- Upstream release: <https://github.com/ibireme/yyjson/releases/tag/0.12.0>
- Archive used: <https://github.com/ibireme/yyjson/archive/refs/tags/0.12.0.tar.gz>
- License: MIT, copyright YaoYuan; the complete upstream text is in `LICENSE`.
- Imported: 2026-07-23.

SHA-256 checksums at import:

```text
b16246f617b2a136c78d73e5e2647c6f1de1313e46678062985bdcf1f40bb75d  yyjson-0.12.0.tar.gz
ac2e9bbb2e2d9149d90878d40506a1d624fa0b33c979a11b61075c54782c6d6a  yyjson.c
175867c5493a5df648cec566717fa1c29aa2f6096f5f0cf1efad0b65e1f6d7b3  yyjson.h
45e384d3d52c73cba3a64d6e6c25d47cd738cd8a55c30629e3201046eda62947  LICENSE
```

The engine includes the source through `src/level_json_yyjson.c` as an isolated C99 object. That
private bridge gives upstream functions internal linkage and exports only `game_rules_*` bridge
symbols, preventing collisions if an embedding host links another yyjson version. No yyjson symbol
or type appears in public engine headers. Compile-time options are set in the root
`CMakeLists.txt`; the generic parser is used only behind `src/level_json.cpp`.

To update yyjson, choose a released tag, review its release notes and license, replace all three
upstream files together without local edits, update the version and checksums above, and run the
native and WebAssembly verification described in `docs/development.md`.
