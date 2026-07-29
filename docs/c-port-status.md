# C17 rewrite status

## Stage-02 boundary

`c-port/` is the self-contained C17 production candidate. The C++ engine remains the frozen
behavioral reference, `docs/rules.md` remains normative, and `include/game_rules/c_api.h` remains
the ABI source of truth.

Stage 02 completes the rule-neutral level boundary:

- strict version-1 JSON decoding through privately linked yyjson 0.12.0;
- typed input pointer/count/tag decoding;
- all 21 ordered world-schema validation errors;
- canonical C-owned immutable level and supplied entity data;
- initial switch/door derivation and immediate teleporter win;
- atomic allocate-then-swap replacement and independent result ownership.

Walking, pushing, gravity, sliding, explosions, physical stabilization of unstable loads, and real
rewind history are deliberately not implemented. The browser vertical-slice load and get-state now
match; its movement operation is the first pinned difference.

The candidate C header remains byte-for-byte identical to the reference header. Parent-only build
and CTest checks enforce that pin without making a standalone `cmake -S c-port` configure reach
outside the candidate tree. The parent also compares the candidate yyjson source, header, and
license byte-for-byte against the reference pin.

## Public contract inventory

| Surface | Stage-02 status |
| --- | --- |
| API versions, create/destroy, null handling | matched |
| Custom allocator extension | implemented; all accepted-load allocation sites failure-injected |
| `game_rules_engine_status` | matched: `schema_ready` |
| Legacy JSON load | strict decoding, validation, canonical state, initial fixtures/win, and atomic replacement matched; physical stabilization deferred |
| Legacy get-state | matched for no level and stage-02 loaded states |
| Typed load | pointer/count/tag checks, structural errors, canonical owned graph, fixtures/win, and atomic replacement matched |
| Typed get-state | matched for no level and stage-02 loaded states |
| Move | only no-level and invalid-direction scaffolding; real movement not started |
| Rewind | empty-history scaffolding only |
| Legacy string and typed result disposal | matched; owners survive replacement and engine destruction |

Typed input arrays are borrowed only for the load call. Accepted input is copied and canonicalized;
the engine never retains caller arrays or JSON bytes. Each typed result is one independently owned
contiguous arena. Nested views remain valid after later calls, replacement, or engine destruction.
Disposal frees the owner and zeros the result.

Invalid JSON and invalid structural loads leave the previous level, current state, history
scaffold, and outstanding caller-owned results unchanged. Accepted replacement is committed only
after the complete candidate session and return graph or JSON response have been allocated.

## Decoder and validation parity

The JSON reader matches the reference's first-error behavior for:

- syntax and UTF-8 failures, including embedded NUL bytes;
- the 16 MiB document limit and nesting depth 32;
- root, missing, unknown, and duplicate members with JSON Pointer paths;
- member types, integer token form and fixed-width ranges, enums, discriminator, version, and
  canonical decimal `uint64` entity IDs;
- empty arrays and structural-validation handoff.

The shared validator covers, in frozen numeric and output order:

1. invalid dimensions;
2. invalid coordinate system;
3. cell count mismatch;
4. cell out of bounds;
5. duplicate cell;
6. invalid cell height;
7. invalid ramp direction;
8. invalid ramp endpoints;
9. fixture out of bounds;
10. fixture on ramp;
11. duplicate fixture;
12. invalid fixture color;
13. entity out of bounds;
14. duplicate entity ID;
15. invalid entity kind;
16. entity below surface;
17. overlapping entities;
18. player not top of stack;
19. player count not one;
20. invalid teleporter occupancy;
21. invalid entity ID.

Cells and fixtures canonicalize by numeric `(y, x)`. Entities canonicalize by
`(y, x, bottomHalfSteps, id)`. Canonicalization is independent of JSON object-member and input-array
ordering. Initial active colors follow palette order and open doors follow canonical fixture order.

## Dependency and symbol isolation

The candidate contains unmodified upstream `yyjson.c`, `yyjson.h`, and `LICENSE` under
`c-port/vendor/yyjson`. `README.game-rules.md` records release 0.12.0, archive and file hashes,
license, import source, and update procedure. `src/level_json_yyjson.c` gives upstream functions
internal linkage and exports only private `game_rules_c_*` bridge names; no yyjson type enters the
public ABI.

All yyjson DOM allocations, decoder temporaries, validation indexes/errors, sessions, legacy JSON
responses, and typed result graphs use the engine allocator. The custom-allocator context therefore
continues to obey the same lifetime rule as stage 01.

## Differential harness

Reference and candidate runners remain separate processes so identical public C symbols never
collide. The comparator treats object order as non-semantic and preserves array order.

Current proofs:

- `game_rules.differential.lifecycle_match`: 16 no-level and lifecycle operations match.
- `game_rules.differential.level_load_parity`: 18 strict decode, validation, canonical state,
  initial fixture/win, replacement, and preservation operations match.
- `game_rules.differential.gameplay_expected_incomplete`: the first remaining browser difference
  is pinned at operation 3, `$.status` (`moved` versus candidate `no_level`).
- The reference runner still checks every authored browser contract oracle unchanged.

## Stage-02 verification

- Standalone strict C17 native build with warnings-as-errors: 4/4 tests passed.
- Parent native debug suite: 124/124 tests passed.
- Emscripten 6.0.3 plus Node: 4/4 candidate tests passed, including parser/load and allocation
  failure coverage.
- Parent and standalone ASan/UBSan configurations built successfully.
- Per repository policy, sanitizer tests were not executed on this Apple Silicon/macOS host because
  the Apple sanitizer runtime stalls during test discovery. Runtime sanitizer evidence remains an
  Ubuntu CI or other known-working Linux handoff.

Boundary coverage added in this stage includes integer extremes, malformed UTF-8, embedded NUL,
empty arrays, count-size overflow/failure, every explicit-width tag, all validation codes, result
and caller-buffer independence, input-order canonicalization, and failure at every allocation in
accepted JSON and typed loads.

## Remaining gaps and stage-03 handoff

Stage 03 can start from one canonical immutable session shared by JSON and typed loads. It should:

1. port physical initialization for unstable accepted levels (gravity, slides, barrel settlement
   and explosions) without changing stage-02 decoding or validation;
2. implement movement gating and the first real movement transition, making browser operation 3
   the next differential milestone;
3. establish real resolved-state history once accepted transitions exist;
4. extend allocation-failure and immutable-result tests to every new tick, event, and history arena.

Do not weaken the stage-02 differential corpus or replace the pinned movement mismatch with a new
earlier difference.
