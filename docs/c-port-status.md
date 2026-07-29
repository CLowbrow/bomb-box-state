# C17 rewrite status

## Purpose and stage-01 boundary

`c-port/` is the self-contained C17 production candidate. The C++ engine under `src/` and
`include/` remains the frozen behavioral reference, `docs/rules.md` remains normative, and
`include/game_rules/c_api.h` remains the ABI source of truth. Stage 01 completes the production
lifecycle slice: allocator-backed creation/destruction, explicit session ownership, atomic
replacement scaffolding, independent result ownership/disposal, and exact no-level responses. It
contains no gameplay rules, level parser, validator, canonicalizer, resolved state, or history
implementation.

The candidate header is a byte-for-byte pinned copy at `c-port/include/game_rules/c_api.h`. The
parent build target and CTest named `game_rules_c_header_drift_check` and
`game_rules.candidate.header_drift` fail on any difference. A standalone `cmake -S c-port`
configure does not inspect the parent header or any other parent path.

The frozen typed ABI has no `not_implemented` call or operation status. Until stage 02, the legacy
JSON load operation returns the explicit non-conforming stage status `not_implemented`, while a
well-formed typed load call returns `GAME_RULES_CALL_INVALID_ARGUMENT` without changing the
engine. This is a tracked infrastructure limitation, not claimed parity and not a new normative
rule. Differential tests use the legacy JSON surface until typed level loading exists.

## Public contract inventory

### Operations and ownership

| Surface | Operation | Reference contract | Stage 01 |
| --- | --- | --- | --- |
| Shared | `game_rules_api_version`, `game_rules_data_api_version` | Independent version values, both currently 1 | matched |
| Shared | `game_rules_engine_create` | Allocates one independent mutable session; null only on allocation failure | matched |
| Shared | `game_rules_engine_destroy` | Accepts null; releases all engine-owned level and history state | matched; empty engine and replacement scaffold covered |
| C17 extension | `game_rules_engine_create_with_allocator_v1` | Additive, versioned custom allocator entry point | implemented and failure-injected |
| Legacy JSON | `game_rules_engine_status` | Static storage, never freed | candidate reports `c17_lifecycle` to expose its current capability |
| Legacy JSON | `load_level` | Parse, validate, canonicalize, stabilize, and atomically replace; invalid input preserves state/history | not implemented |
| Legacy JSON | `get_state` | Complete current renderable snapshot or null | matched only with no level/null engine |
| Legacy JSON | `move` | Complete rejection or turn with ordered ticks/events and authoritative state | matched only for no-level and invalid-direction gating |
| Legacy JSON | `rewind` | Restore one resolved boundary, discard abandoned future, no tick | matched only for empty history |
| Legacy JSON | `game_rules_string_free` | Frees any non-null returned UTF-8 JSON allocation; accepts null | matched |
| Typed data | `load_level_data` | Borrow arrays for one call; copy accepted data; return owned immutable result graph | not implemented |
| Typed data | `get_state_data` | Owned immutable snapshot graph or `has_state == 0` | matched only with no level/null engine |
| Typed data | `move_data` | Owned complete move result graph | matched only for no-level and invalid-direction gating |
| Typed data | `rewind_data` | Owned complete rewind result graph | matched only for empty history |
| Typed data | four `*_result_dispose` functions | Accept null, free `owned_storage`, and zero the result | matched for stage-01 owners; repeated disposal after zeroing is safe |

Legacy non-null strings and typed `owned_storage` values are caller-owned allocations. A typed
result graph uses one contiguous arena, so future nested arrays must point inside its
`owned_storage` allocation and cannot require separate disposal. Each owner has a private aligned
prefix containing a copy of its deallocator and context; nested pointers
therefore remain valid after later calls, atomic session replacement, and engine destruction.
Callers initialize results to zero, do not copy an owning result, dispose before reuse, and use the
matching disposer. A disposer accepts null, frees a non-null owner, and zeros the result, so calling
it again on that same cleared struct is safe. Input arrays remain borrowed only for a future load
call.

The frozen `c_api.h` remains byte-for-byte unchanged and its creation function uses the C runtime
allocator. The additive `c_allocator_api.h` version-1 extension accepts allocate/deallocate
callbacks. All engine, replacement-session, legacy-result, and typed-result allocations for such
an engine use that allocator. The allocator context must outlive the engine and all outstanding
results. No process-global allocator or mutable registry exists.

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

Status meanings: **matched** is covered against the reference; **scaffolded** is production
ownership infrastructure without level behavior; **not started** has no production implementation
in `c-port/`.

| Subsystem | Required behavior | Reference executable coverage | Candidate status and mapped tests |
| --- | --- | --- | --- |
| Frozen C ABI | Exact constants, layouts, prototypes, C compilation | `tests/consumer/c_api_header_smoke.c`; `tests/adapter/c_*` | **matched header**; build/CTest drift check; `c-port/tests/lifecycle_smoke.c` |
| Engine lifetime | Independent instances, null-safe destroy, no global mutable state | `adapter/c_api_test.cpp`; `behavior/hardening_test.cpp`; lifecycle contract | **matched for lifecycle slice**; `lifecycle_smoke.c`, `allocation_failure_test.c`, `lifecycle-match.txt` |
| Allocator/failure transaction | Every owned allocation fails cleanly; replacement is allocate-then-swap | reference boundary allocation statuses and replacement atomicity tests | **matched/scaffolded**; failure at every stage-01 allocation index, overflow, leak/double-free counters |
| Legacy result ownership | Allocated null-terminated JSON, null on allocation failure, explicit free | `adapter/c_api_test.cpp` | **matched for lifecycle responses**; allocation failure and post-destroy lifetime covered |
| Typed result ownership | Immutable independent graphs, disposal zeros, survival after engine destruction | `adapter/c_data_api_test.cpp` | **matched for lifecycle results**; all four disposal paths, double-safe cleared disposal, post-replacement/destruction event view |
| Level JSON codec | Strict v1 parse, resource limits, precise errors, canonical encoding | `behavior/level_json_test.cpp`; malformed contract load | **not started**; candidate runner proves byte input transport only |
| Typed input decoding | Borrowed arrays; pointer/tag/enum checks; no partial mutation | `adapter/c_data_api_test.cpp` | **not started**; only top-level null checks exist |
| World schema validation | All 21 errors and stable ordered reporting | `behavior/world_schema_test.cpp`; `behavior/level_json_test.cpp` | **not started** |
| Canonicalization | Input-order independence and canonical static/dynamic arrays | `world_schema_test.cpp`; `player_push_test.cpp`; `falling_test.cpp`; conflict contracts | **not started** |
| Load transaction | Validate before replace; initialization; invalid-load preservation; replacement isolation | `resolved_state_history_test.cpp`; `hardening_test.cpp`; lifecycle contract | **ownership scaffolded** with allocation-complete swap and rollback; public load behavior not started; explicit differential mismatch |
| Initialization/stabilization | Initial fixtures, win, gravity, slides, explosions until stable/terminal | `falling_test.cpp`; `fixtures_test.cpp`; `ramps_test.cpp`; `explosions_test.cpp` | **not started** |
| Flat walking/axes | Four directions, axis mapping, legal support, rejection atomics | `behavior/flat_walking_test.cpp` | **not started** |
| Player push | One eligible top entity, atomic push, heights, order independence | `behavior/player_push_test.cpp`; `falling_test.cpp` | **not started** |
| Gravity/crushing | Bottom-up column compaction, barrel arming, player thresholds/crush | `behavior/falling_test.cpp`; `unit/gravity_test.cpp` | **not started** |
| Ramp traversal/slides | Endpoint and lane connectivity, stack slide, retries/conflicts | `behavior/ramps_test.cpp` | **not started** |
| Explosions/chains | Waves, height targeting, conflicts, settlement before next wave | `behavior/explosions_test.cpp`; `unit/explosions_test.cpp`; conflict contract | **not started** |
| Switches/doors | AND colors, held-open doors, event ordering, rewind | `behavior/fixtures_test.cpp`; `unit/fixtures_test.cpp` | **not started** |
| Teleporters/outcome | Eligibility, immediate terminal win, win precedence, terminal gating | `behavior/fixtures_test.cpp`; `ramps_test.cpp`; terminal contract | **not started** |
| Resolved-state history | Root, repeated rewind, boundary, branch/no redo, terminal rewind | `behavior/resolved_state_history_test.cpp`; `hardening_test.cpp`; rewind-stress contract | **matched only for no-level empty history**; real history not started |
| Simultaneous conflicts | Shared snapshots; no ID/container-order physics | `behavior/hardening_test.cpp`; explosion/slide conflict contracts | **not started** |
| Differential transcript | Same operations/bytes through separate symbol spaces | existing contracts plus `tests/c-port/transcripts/lifecycle-match.txt` | **matched infrastructure**; reference/candidate runner and comparator CTests |
| Native/sanitizer build | Strict C17, warnings, ASan/UBSan-ready | native presets and CI sanitizer guidance | **native green**; ASan/UBSan build green; runtime intentionally not executed on Apple Silicon/macOS |
| WebAssembly lifecycle | C-only candidate lifecycle and ownership under Emscripten | existing JS/Wasm contract suite for reference | **green**; lifecycle, failure-injection, and post-destroy result smoke tests (3/3) |

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

- `game_rules.differential.lifecycle_match` compares 16 operations: no-level state, all four valid
  directions, invalid direction, rewind, destroyed-engine calls, load gating, and repeated creation.
- `game_rules.differential.gameplay_expected_incomplete` requires the browser vertical-slice load
  to differ at operation 1, `$.status`: reference `loaded`, candidate `not_implemented`.
- `game_rules.reference_runner.browser_contract` separately checks reference output against every
  existing authored oracle in that transcript.
- `game_rules.candidate_runner.consumes_browser_contract` and
  `game_rules.candidate_runner.consumes_hardening_lifecycle` prove the candidate consumes the same
  transcript and level bytes, including repeated replacement and malformed JSON, without claiming
  parity.

## Stage 01 verification and consciously deferred behavior

- Standalone native C17 build with warnings-as-errors: 3/3 candidate tests passed.
- Parent native debug suite: 119/119 tests passed; lifecycle differential parity matched all 16
  operations.
- Emscripten 6.0.3 build and Node execution: lifecycle, allocation-failure, and wasm smoke tests
  passed (3/3).
- The standalone ASan/UBSan configuration built successfully. Per repository policy, sanitizer
  tests were not executed on this Apple Silicon/macOS host because the Apple sanitizer runtime is
  not a supported verification environment; Ubuntu sanitizer CI remains required for a runtime
  clean claim.
- Legacy load with non-null bytes still returns `not_implemented`; typed load with non-null level
  still returns `GAME_RULES_CALL_INVALID_ARGUMENT`. These deliberate non-parity statuses do not
  mutate the replacement scaffold. No level, initialization, movement, or rewind-history behavior
  has been ported.
- `game_rules_engine_status()` deliberately reports `c17_lifecycle`, not the reference
  `schema_ready`, until the schema boundary exists.

## Precise stage 02 prerequisites

Stage 02 must add the rule-neutral data boundary before any gameplay: C-owned value/storage types,
strict level-format v1 decoding, all structural validation codes, and canonicalization under
`c-port/`. It must vendor yyjson 0.12.0 and its license/provenance inside `c-port/`, port the
relevant `behavior/level_json_test.cpp`, `behavior/world_schema_test.cpp`, and typed adapter
malformed-input cases, and construct a complete replacement session before calling the existing
allocate-then-swap commit seam. JSON and typed inputs must converge on the same canonical validated
level. The next differential milestone is a structurally invalid load, which requires no gameplay;
stabilization, real movement, and resolved-state history remain later stages.
