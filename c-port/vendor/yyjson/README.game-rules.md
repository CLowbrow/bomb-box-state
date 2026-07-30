# Vendored yyjson

`yyjson.c`, `yyjson.h`, and `LICENSE` are unmodified upstream files from
[ibireme/yyjson](https://github.com/ibireme/yyjson), release tag `0.12.0`.
They were copied from the audited reference repository's pinned vendor directory on
2026-07-28 so this standalone C17 package remains self-contained.

Upstream archive SHA-256:

```text
b16246f617b2a136c78d73e5e2647c6f1de1313e46678062985bdcf1f40bb75d  yyjson-0.12.0.tar.gz
```

Pinned file SHA-256 values:

```text
ac2e9bbb2e2d9149d90878d40506a1d624fa0b33c979a11b61075c54782c6d6a  yyjson.c
175867c5493a5df648cec566717fa1c29aa2f6096f5f0cf1efad0b65e1f6d7b3  yyjson.h
45e384d3d52c73cba3a64d6e6c25d47cd738cd8a55c30629e3201046eda62947  LICENSE
```

The implementation is included only through `src/level_json_yyjson.c`, which
gives upstream functions internal linkage. No yyjson type or symbol enters a
public header. To update, replace all three upstream files together, review the
release and license, update these hashes, and run the native and WebAssembly
parser/load suites. During transition, the originating repository additionally compares these
files byte-for-byte with its reference pin.
