# C17 rewrite status

## Purpose and stage-00 boundary

`c-port/` is the self-contained C17 production candidate. The C++ engine under `src/` and
`include/` remains the frozen behavioral reference, `docs/rules.md` remains normative, and
`include/game_rules/c_api.h` remains the ABI source of truth. Stage 00 implements infrastructure
only: allocation, destruction, result disposal, and no-level responses. It contains no gameplay
rules, level parser, validator, canonicalizer, resolved state, or history implementation.

The candidate header is a byte-for-byte pinned copy at `c-port/include/game_rules/c_api.h`. The
parent build target and CTest named `game_rules_c_header_drift_check` and
`game_rules.candidate.header_drift` fail on any difference. A standalone `cmake -S c-port`
configure does not inspect the parent header or any other parent path.

The frozen typed ABI has no `not_implemented` call or operation status. Until stage 01, the legacy
JSON load operation returns the explicit non-conforming stage status `not_implemented`, while a
well-formed typed load call returns `GAME_RULES_CALL_INVALID_ARGUMENT` without changing the
engine. This is a tracked infrastructure limitation, not claimed parity and not a new normative
rule. Differential tests use the legacy JSON surface until typed level loading exists.

## Public contract inventory

### Operations and ownership

| Surface | Operation | Reference contract | Stage 00 |
| --- | --- | --- | --- |
| Shared | `game_rules_api_version`, `game_rules_data_api_version` | Independent version values, both currently 1 | matched |
| Shared | `game_rules_engine_create` | Allocates one independent mutable session; null only on allocation failure | matched |
| Shared | `game_rules_engine_destroy` | Accepts null; releases all engine-owned level and history state | matched for empty engines |
| Legacy JSON | `game_rules_engine_status` | Static storage, never freed | candidate reports `c17_skeleton` to expose incompleteness |
| Legacy JSON | `load_level` | Parse, validate, canonicalize, stabilize, and atomically replace; invalid input preserves state/history | not implemented |
| Legacy JSON | `get_state` | Complete current renderable snapshot or null | matched only with no level/null engine |
| Legacy JSON | `move` | Complete rejection or turn with ordered ticks/events and authoritative state | matched only for no-level and invalid-direction gating |
| Legacy JSON | `rewind` | Restore one resolved boundary, discard abandoned future, no tick | matched only for empty history |
| Legacy JSON | `game_rules_string_free` | Frees any non-null returned UTF-8 JSON allocation; accepts null | matched |
| Typed data | `load_level_data` | Borrow arrays for one call; copy accepted data; return owned immutable result graph | not implemented |
| Typed data | `get_state_data` | Owned immutable snapshot graph or `has_state == 0` | matched only with no level/null engine |
| Typed data | `move_data` | Owned complete move result graph | matched only for no-level and invalid-direction gating |
| Typed data | `rewind_data` | Owned complete rewind result graph | matched only for empty history |
| Typed data | four `*_result_dispose` functions | Accept null, free `owned_storage`, and zero the result | matched for stage-00 allocations |

Legacy non-null strings are caller-owned `malloc` allocations. Typed results own one allocation
graph through opaque `owned_storage`; callers initialize results to zero, do not copy an owning
result, dispose before reuse, and dispose exactly once. Nested pointers must remain valid after
later engine calls and engine destruction. Input arrays are borrowed only for the load call. The
frozen ABI provides no custom allocator hook, so the C candidate uses the C runtime
`malloc`/`free`; future internal allocator abstractions must preserve that public ownership model.

### Status inventory

- Boundary call statuses: `OK`, `INVALID_ENGINE`, `INVALID_ARGUMENT`, `ALLOCATION_FAILED`.
- Load statuses: `LOADED`, `INVALID_LEVEL`.
- Move statuses: `MOVED`, `NO_LEVEL`, `INVALID_DIRECTION`, `WORLD_BOUNDARY`, `LEDGE`, `OCCUPIED`,
  `STACKED_PUSH_TARGET`, `CLOSED_DOOR`, `TELEPORTER_RESTRICTION`, `UNSUPPORTED_GEOMETRY`,
  `LEVEL_TERMINAL`.
- Rewind statuses: `REWOUND`, `HISTORY_EMPTY`.
- Outcomes: `ONGOING`, `WON`, `LOST`.
- Legacy JSON also distinguishes boundary `invalid_engine`/`invalid_argument`, level-format
  `invalid_json`, structural `invalid_level`, state `ok`/`no_level`, and operation statuses using
  the lowercase names above.

Boundary failure and gameplay rejection are separate. Invalid pointer/tag/enum input is a call
failure where specified. A recognized direction blocked by rules is `GAME_RULES_CALL_OK` with an
unaccepted move status and presentation event. An out-of-range movement value is
`GAME_RULES_CALL_OK` with `INVALID_DIRECTION`, no direction, and no presentation event.

### Validation-error inventory

The typed structural validation codes, in frozen numeric order, are:

1. `INVALID_DIMENSIONS`
2. `INVALID_COORDINATE_SYSTEM`
3. `CELL_COUNT_MISMATCH`
4. `CELL_OUT_OF_BOUNDS`
5. `DUPLICATE_CELL`
6. `INVALID_CELL_HEIGHT`
7. `INVALID_RAMP_DIRECTION`
8. `INVALID_RAMP_ENDPOINTS`
9. `FIXTURE_OUT_OF_BOUNDS`
10. `FIXTURE_ON_RAMP`
11. `DUPLICATE_FIXTURE`
12. `INVALID_FIXTURE_COLOR`
13. `ENTITY_OUT_OF_BOUNDS`
14. `DUPLICATE_ENTITY_ID`
15. `INVALID_ENTITY_KIND`
16. `ENTITY_BELOW_SURFACE`
17. `OVERLAPPING_ENTITIES`
18. `PLAYER_NOT_TOP_OF_STACK`
19. `PLAYER_COUNT_NOT_ONE`
20. `INVALID_TELEPORTER_OCCUPANCY`
21. `INVALID_ENTITY_ID`

The JSON decoder additionally has stable format errors for invalid JSON, document size, nesting
depth, root type, missing/unknown/duplicate members, member type, integer range, enum value, format
discriminator, version, and entity ID. Syntax errors carry a byte offset; decoded-shape errors
carry a JSON Pointer. Structural errors are ordered values containing code, coordinate, and entity
ID. Rejected loads preserve the prior current state and complete rewind history.

### Events, ticks, and snapshots

The frozen event kinds are `MOVE_BLOCKED`, `STATE_REWOUND`, `ENTITY_MOVED`, `BARREL_ARMED`,
`BARREL_EXPLODED`, `PLAYER_CRUSHED`, `SWITCH_CHANGED`, `DOOR_OPENED`, `DOOR_CLOSED`, `LEVEL_WON`,
and `LEVEL_LOST`. `ENTITY_MOVED` has cause `PLAYER`, `BLAST`, `FALL`, or `SLIDE`. The fixed event
struct is intentionally not a union; only the fields documented in `docs/embedding-api.md` are
meaningful for each tag.

A snapshot combines the canonical static level with authoritative resolved state. Resolved state
contains entities, armed barrel IDs, active switch colors, effectively open doors, and outcome.
Load and accepted move results contain initial state, zero-based contiguous ticks, every tick's
ordered events and complete `state_after`, final state, current snapshot, and outcome. A rejected
move has presentation events outside the tick list. Rewind has no tick and returns its restored
state plus current snapshot.

### Deterministic ordering inventory

- Cells are numeric row-major `(y, x)`.
- Fixtures are canonical spatial order.
- Entities are `(y, x, bottomHalfSteps, id)`; within a column this is bottom-to-top, with ID used
  only as a final stable representation tie-break after valid non-overlapping geometry.
- Armed barrel IDs are ascending; active colors are palette order; open doors are row-major.
- Player-push movement events are player first, pushed entity second.
- Falls are source cell row-major then pre-tick bottom-to-top; terminal loss is last.
- Ramp slides are source ramp row-major then pre-tick bottom-to-top.
- Explosion sources are row-major then pre-wave bottom-to-top; target effects are affected-cell
  row-major then pre-wave bottom-to-top. Explosion events precede target effects.
- Fixture events follow physical/arming events: switch changes in palette order, then door events
  in row-major order, before crushing/terminal events.
- Every simultaneous tick reads one immutable pre-tick snapshot. Entity IDs, input container
  order, implementation iteration order, platform, wall clock, randomness, and asynchronous
  completion never decide physics.
- Canonical JSON preserves specified array ordering and field layout. The differential comparator
  treats object member order as non-semantic but always preserves and compares array order.

## Feature-parity matrix

Status meanings: **matched** is covered against the reference; **skeleton** is intentionally
limited lifecycle infrastructure; **not started** has no production implementation in `c-port/`.

| Subsystem | Required behavior | Reference executable coverage | Candidate status and mapped tests |
| --- | --- | --- | --- |
| Frozen C ABI | Exact constants, layouts, prototypes, C compilation | `tests/consumer/c_api_header_smoke.c`; `tests/adapter/c_*` | **matched header**; build/CTest drift check; `c-port/tests/lifecycle_smoke.c` |
| Engine lifetime | Independent instances, null-safe destroy, no global mutable state | `adapter/c_api_test.cpp`; `behavior/hardening_test.cpp`; lifecycle contract | **skeleton**; lifecycle smoke and `lifecycle-match.txt` |
| Legacy result ownership | Allocated null-terminated JSON, null on allocation failure, explicit free | `adapter/c_api_test.cpp` | **skeleton matched** for stage-00 responses; lifecycle smoke |
| Typed result ownership | Immutable independent graphs, disposal zeros, survival after engine destruction | `adapter/c_data_api_test.cpp` | **skeleton matched** only for empty results; lifecycle smoke |
| Level JSON codec | Strict v1 parse, resource limits, precise errors, canonical encoding | `behavior/level_json_test.cpp`; malformed contract load | **not started**; candidate runner proves byte input transport only |
| Typed input decoding | Borrowed arrays; pointer/tag/enum checks; no partial mutation | `adapter/c_data_api_test.cpp` | **not started**; only top-level null checks exist |
| World schema validation | All 21 errors and stable ordered reporting | `behavior/world_schema_test.cpp`; `behavior/level_json_test.cpp` | **not started** |
| Canonicalization | Input-order independence and canonical static/dynamic arrays | `world_schema_test.cpp`; `player_push_test.cpp`; `falling_test.cpp`; conflict contracts | **not started** |
| Load transaction | Validate before replace; initialization; invalid-load preservation; replacement isolation | `resolved_state_history_test.cpp`; `hardening_test.cpp`; lifecycle contract | **not started**; explicit differential mismatch |
| Initialization/stabilization | Initial fixtures, win, gravity, slides, explosions until stable/terminal | `falling_test.cpp`; `fixtures_test.cpp`; `ramps_test.cpp`; `explosions_test.cpp` | **not started** |
| Flat walking/axes | Four directions, axis mapping, legal support, rejection atomics | `behavior/flat_walking_test.cpp` | **not started** |
| Player push | One eligible top entity, atomic push, heights, order independence | `behavior/player_push_test.cpp`; `falling_test.cpp` | **not started** |
| Gravity/crushing | Bottom-up column compaction, barrel arming, player thresholds/crush | `behavior/falling_test.cpp`; `unit/gravity_test.cpp` | **not started** |
| Ramp traversal/slides | Endpoint and lane connectivity, stack slide, retries/conflicts | `behavior/ramps_test.cpp` | **not started** |
| Explosions/chains | Waves, height targeting, conflicts, settlement before next wave | `behavior/explosions_test.cpp`; `unit/explosions_test.cpp`; conflict contract | **not started** |
| Switches/doors | AND colors, held-open doors, event ordering, rewind | `behavior/fixtures_test.cpp`; `unit/fixtures_test.cpp` | **not started** |
| Teleporters/outcome | Eligibility, immediate terminal win, win precedence, terminal gating | `behavior/fixtures_test.cpp`; `ramps_test.cpp`; terminal contract | **not started** |
| Resolved-state history | Root, repeated rewind, boundary, branch/no redo, terminal rewind | `behavior/resolved_state_history_test.cpp`; `hardening_test.cpp`; rewind-stress contract | **empty-history skeleton**; lifecycle smoke/match |
| Simultaneous conflicts | Shared snapshots; no ID/container-order physics | `behavior/hardening_test.cpp`; explosion/slide conflict contracts | **not started** |
| Differential transcript | Same operations/bytes through separate symbol spaces | existing contracts plus `tests/c-port/transcripts/lifecycle-match.txt` | **matched infrastructure**; reference/candidate runner and comparator CTests |
| Native/sanitizer build | Strict C17, warnings, ASan/UBSan-ready | native presets and CI sanitizer guidance | **configured**; standalone/native builds; do not run sanitizer tests on Apple Silicon/macOS |
| WebAssembly skeleton | C-only candidate can instantiate under Emscripten | existing JS/Wasm contract suite for reference | **smoke target added**; `game_rules_candidate_wasm_smoke` |

## Differential harness

`game_rules_reference_runner` links only `GameRules::State` (the C++ reference). The separately
linked `game_rules_candidate_runner` links only `GameRules::StateC`; identical public symbols never
share a process. Both interpret the existing pipe-separated contract format. The extension adds
`destroy-engine` and `create-engine`; `-` in the expected-output field denotes a runner-lifecycle
operation without an authored engine oracle. Existing `load`, `get-state`, `move`, and `rewind`
syntax and all reviewed expected files are unchanged. Repeated `load` covers level replacement and
an input path may contain malformed bytes.

Each runner writes exactly one complete compact JSON response per operation. The Python comparator
runs both processes, parses every response, retains array order, and reports the first difference
with one-based operation index, transcript line, and JSON field path. Normal mode exits nonzero on
any difference. `--expect-difference` is reserved for a pinned, explicitly incomplete port case and
fails if the expected difference disappears, moves, or is preceded by another difference.

Current proofs:

- `game_rules.differential.lifecycle_match` compares nine no-level and destroy/recreate operations.
- `game_rules.differential.gameplay_expected_incomplete` requires the browser vertical-slice load
  to differ at operation 1, `$.status`: reference `loaded`, candidate `not_implemented`.
- `game_rules.reference_runner.browser_contract` separately checks reference output against every
  existing authored oracle in that transcript.
- `game_rules.candidate_runner.consumes_browser_contract` and
  `game_rules.candidate_runner.consumes_hardening_lifecycle` prove the candidate consumes the same
  transcript and level bytes, including repeated replacement and malformed JSON, without claiming
  parity.

## Stage 01 starting point

Start stage 01 with the rule-neutral data boundary: add C value/storage types, strict level-format
v1 decoding, all structural validation codes, and canonicalization under `c-port/`, with direct C
tests ported from `behavior/level_json_test.cpp`, `behavior/world_schema_test.cpp`, and the typed
adapter malformed-input cases. Vendor yyjson 0.12.0 and its license/provenance inside `c-port/`
before using it. Do not implement stabilization, movement, or history in that slice. Once JSON and
typed inputs produce the same canonical validated level, change the first differential milestone
to a structurally invalid load (which requires no gameplay) before attempting a stable flat-level
load and initialization in the following stage.
