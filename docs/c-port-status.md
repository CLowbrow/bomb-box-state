# C17 rewrite status

## Stage-10 boundary

`c-port/` is the self-contained C17 production candidate. The C++ engine remains the frozen
behavioral reference, `docs/rules.md` remains normative, and `include/game_rules/c_api.h` remains
the ABI source of truth.

Stage 10 retains the complete stage-09 rules boundary and adds the final undo-only resolved-state
history and lifecycle behavior through both public APIs. It now covers:

- strict version-1 JSON and typed input decoding from stage 02;
- all 21 ordered world-schema validation errors;
- canonical C-owned static level data, resolved dynamic state, and cell-to-fixture indexing;
- complete fixture, gravity, ramp-slide, explosion-wave, loss, and win stabilization before the
  first command;
- immutable independently owned typed and JSON initial, tick, final, and current snapshots;
- atomic allocate-then-swap level replacement under allocation failure;
- north, east, south, and west decoding through both move boundaries;
- one-cell walking across flat terrain, including non-zero origins, reversed axes, and compatible
  stack support;
- one-cell box and barrel pushes at the player's bottom half-step, including pushes over lower
  terrain and onto lower stacks;
- endpoint, connected-chain, and matching-lane player traversal for every low direction under all
  declared x/y axis orientations;
- downhill box and barrel pushes onto ramps, pushes while leaving a ramp endpoint, and lateral
  pushes across three matching ramp lanes;
- simultaneous whole-stack slides with canonical source/stack event order, deterministic
  destination conflict cancellation, and fixture-aware blocked retries;
- bottom-up gravity compaction with complete half-step fall events, barrel arming, player-fall and
  crushing loss rules, and canonical simultaneous-column ordering;
- exact positive-fall and blast-hit arming, with armed barrels settling every triggered fall and
  available ramp slide before detonation;
- simultaneous explosion waves planned from one immutable pre-wave state, with spatial source and
  target event order, same-cell vertical chains, blast-height stack selection, source removal,
  support removal, and complete authoritative snapshots;
- deterministic impulse and destination conflict cancellation independent of IDs, input order,
  allocation order, and container traversal, while preserving independent nonconflicting moves;
- flat, endpoint, connected-center, and matching-lane ramp blast geometry, including blast-driven
  falls, whole-stack settling, slides, secondary arming, and later explosion waves;
- explosion-time switch and door recomputation, crushing and direct blast deaths, win precedence,
  terminal cancellation, and exact fixture/terminal event ordering;
- palette-ordered active switch derivation with AND behavior across multiple same-color switches;
- row-major effectively open doors, including active colors, occupied-door holding, closing after
  the final occupant leaves, and deterministic open/close changes around simultaneous effects;
- exact fixture event timing after movement, pushing, falling, sliding, and explosion ticks, with
  switch changes before coordinate-ordered door changes and fixture changes before terminal loss;
- closed-door and exit-column restrictions for player movement, pushes, ramp slides, gravity
  stabilization, and blast pops;
- legal floor-height exit activation, immediate and movement-triggered wins, win precedence, and
  terminal movement rejection; and
- exact pre-secondary-rule rejections for invalid direction, no level, world boundary, ledges,
  recursive or overlapping occupancy, non-top push targets, closed doors, ineligible teleporters,
  unsupported geometry, and terminal levels;
- exactly one canonical pre-command history snapshot for every accepted movement command and none
  for rejected commands, independent of the command's tick count;
- repeated rewind to the initialized boundary, `history_empty` behavior with and without a loaded
  level, semantic `StateRewound` events, and exact restoration of all dynamic state;
- one-way branching after rewind, with no redo storage or abandoned-future retention;
- rewind from movement-produced won and lost states, while initialization-terminal levels retain
  an empty history root; and
- valid replacement history discard, invalid or allocation-failed replacement preservation, and
  independent result/snapshot lifetime across moves, rewinds, replacement, and destruction.

## Internal state and ownership

An accepted session owns three explicit resolved-state buffers:

- the canonical supplied initial state returned by load;
- the authoritative current stabilized state; and
- a scratch state used to calculate each tick or command from an immutable pre-state.

Each state uses bounded arrays for canonical entities, ascending armed-barrel IDs, palette-ordered
active switch colors, row-major open doors, and outcome. Static cells and fixtures are canonical
row-major arrays. A row-major cell index maps to its optional fixture without pointer or hash-order
dependence. Initialization scratch arrays are bounded by validated cell, fixture, and entity counts.
The initialization tick list grows with checked arithmetic; every retained tick owns an immutable
event/state arena.

History is an owned LIFO chain of canonical pre-command state arenas. An accepted command reserves
and copies its prospective history entry before derived resolution. The entry is linked and the
new current state is swapped only after the complete JSON or typed result has been allocated.
Rewind allocates its independent response graph from the immutable top entry before restoring and
freeing that entry. Thus every allocation failure leaves current state and history unchanged.

Movement planning first copies current state into scratch and describes the initiating tick with an
explicit transaction view. An accepted plan is then copied into an isolated command workspace with
two private resolved-state buffers. Gravity, ramp slides, explosions, fixtures, crushing, and
terminal effects run against those buffers, and every retained command tick owns an immutable
event/state arena.
JSON or typed result snapshots are fully allocated from this ordered tick list before the final
workspace state is copied into session scratch and swapped into current. Allocation or internal
resolution failure frees the workspace and leaves the complete pre-command state unchanged.

Typed inputs are borrowed only for their call. Accepted input is copied and canonicalized. Each
typed result is one independently owned contiguous arena, including nested tick, event, level, and
resolved-state arrays. Empty collections use a zero count and null pointer. Results survive later
calls, successful replacement, and engine destruction. Disposal frees the owner and zeros the
result.

Invalid JSON, invalid structural loads, rejected movement, and allocation failures leave the prior
level and current state unchanged. A replacement or move is committed only after its complete
return graph or JSON response has been allocated.

## Ordering and serialization parity

The candidate retains the frozen ordering rules: cells, fixtures, doors, physical work, and spatial
events use numeric `(y, x)` order; entities use `(y, x, bottomHalfSteps, id)`; armed IDs are
ascending; colors use palette order; and stack/source/target event orders follow the normative
rules. A successful walk emits one tick at index zero containing one `EntityMoved` event with cause
`player`; a successful push emits the player's event first and the pushed box or barrel second,
both with cause `player` and from the same pre-tick state. Successful slide events use source
coordinate then pre-tick bottom-to-top order, with the exact half-step translation in every event
and `state_after` snapshot.

JSON object fields and presence/null behavior match the reference API for the completed surface.
Entity IDs, including `18446744073709551615`, are quoted canonical decimals. Output depends on no
pointer address, hash order, allocator order, wall clock, platform API, or unspecified traversal.

## Public contract inventory

| Surface | Audit status | Executable evidence |
| --- | --- | --- |
| API versions, create/destroy, null handling | passed | lifecycle, ABI-layout, C-header, C++-header, and boundary-fuzz tests |
| Custom allocator extension | passed | allocation injection walks every successful allocation index for JSON and typed load, get-state, move/resolution, rewind, history growth, and replacement; every failure checks rollback and live allocations |
| `game_rules_engine_status` | passed: `schema_ready` | lifecycle and differential lifecycle tests |
| Legacy JSON load/get-state | passed | stage-03 differential, malformed-input boundary fuzz, and ownership tests |
| Typed load/get-state | passed | stage-03 differential, typed pointer/count/tag boundary fuzz, ABI-layout, and ownership tests |
| Move | passed | stage-04 through stage-09 authored differential corpora plus generated valid-level differential transcripts |
| Rewind | passed | stage-10 browser, hardening, stress, and 2,312-operation seeded history/lifecycle differential corpora |

The frozen candidate header and pinned private yyjson 0.12.0 source, header, and license remain
byte-for-byte identical to the reference copies. No yyjson symbol or type crosses the public ABI.

## Differential and structural coverage

Separate reference and candidate processes avoid identical C-symbol collisions. The comparator
treats object order as non-semantic while preserving every array order.

Current proofs:

- `game_rules.differential.lifecycle_parity`: all 16 no-level and lifecycle operations match.
- `game_rules.differential.level_load_parity`: all 18 stage-02 decode, validation, canonical-state,
  replacement, and preservation operations match.
- `game_rules.differential.load_state_parity`: all 20 load/get-state operations match for
  stable, unstable, reordered, fixture-bearing, coordinate-variant, terminal, and empty worlds.
- `game_rules.differential.flat_walking_parity`: all 34 load/move operations match for all
  cardinal directions, non-zero origins, both axis orientations, repeated commands, board edges,
  both ledge directions, compatible stack support, occupied push contact, closed doors, ineligible
  teleporters, unsupported ramp entry, malformed direction, terminal state, and reordered
  equivalent input.
- `game_rules.differential.player_pushing_parity`: all 40 load/move operations match for
  boxes and barrels in every cardinal direction, repeated pushes, reversed axes and non-zero
  origins, maximum entity IDs, top-of-stack selection, exact-height destination support, reordered
  source arrays and entity IDs, recursive occupancy, world edges, higher terrain, closed, active,
  and held-open doors, teleporters, unsupported ramp entry, and the frozen browser push.
- `game_rules.differential.gravity_parity`: all 23 load/move operations match for lower
  terrain, landing on stacks, top-of-stack removal, tall drops, gravity-triggered fixtures, barrel
  arming and explosion closure, chain loss, repeated terminal commands, simultaneous post-blast
  falls, teleporter win and terminal gating, maximum entity IDs, and reordered cells and entities.
- `game_rules.differential.ramps_parity`: all 52 load/move operations match for every low
  direction and coordinate orientation, endpoint and chain traversal, lateral lanes, downhill and
  ramp-exit pushes, stack heights, blocked retries, doors, exits, conflicts, ledges,
  fall-then-slide, slide-then-fall, maximum/reassigned IDs, and reordered input arrays.
- `game_rules.differential.fixtures_parity`: all 43 load/get-state/move operations match for
  every color, multiple same-color switches and doors, simultaneous activation/deactivation,
  canonical palette and door-coordinate order, held doors, multi-tick push/fall activation,
  explosion-driven fixture changes, fixture-aware walking/pushing/sliding, exit restrictions,
  immediate and movement-triggered wins, terminal commands, and reordered fixture/entity input.
- `game_rules.differential.explosions_parity`: all 47 generated load/move operations match
  for every authored explosion behavior, simultaneous source and destination conflicts, same-cell
  and adjacent chains, blast-height stack pops, fixture changes, crushing, ramp connectivity,
  blast-to-fall-to-slide closure, terminal outcomes, eight fixed stress seeds, reassigned IDs,
  reordered arrays, and identical repeated-seed execution.
- `game_rules.candidate_runner.browser_contract`: all five browser vertical-slice
  operations, including rewind and the following state snapshot, match their authored outputs.
- `game_rules.candidate_runner.hardening_contract`: lifecycle, conflict, replacement,
  terminal, and 33-operation rewind-stress scripts match every authored output.
- `game_rules.differential.seeded_history_lifecycle`: 2,312 operations across eight fixed
  seeds mix accepted and blocked moves, invalid directions, repeated rewinds, branches, valid and
  malformed replacement, snapshots, terminal states, engine destruction, and recreation. Both
  engines match, and two identical candidate executions are byte-identical.
- `game_rules.candidate_runner.browser_push_contract`: the candidate matches the existing
  authored load and move golden outputs without modifying them.
- `game_rules.candidate_runner.explosion_conflicts_contract`: canonical and reordered
  explosion conflicts, plus the existing whole-stack slide conflicts, match the authored golden
  outputs without modification.

Candidate C tests verify complete initial, tick, event, final, snapshot, acceptance, status, and
outcome fields as well as result ownership and pointer/count invariants. The dedicated explosion
suite covers six-tick blast/fall chains, exact arming and wave timing, complete per-tick snapshots,
simultaneous conflict cancellation, reordered input, reassigned IDs, vertical chains, fixtures,
loss ordering, matching-ramp geometry, blast-to-fall-to-slide closure, and terminal state. The
dedicated fixture suite covers all palette colors, multiple same-color switches and doors,
canonical fixture and event order, independent immutable typed views, simultaneous switch/door
changes, AND behavior, held-door falls and closing, multi-tick gravity activation, loss ordering,
exit-height rejection, wins, and terminal commands. The falling corpus covers
initial multi-column compaction, ramp-center landing before the deferred slide phase, direct crush
planning, box stacks, tall drops, barrel chains, and terminal repetition. Allocation injection
walks every allocation in multi-tick JSON and typed falling-barrel commands; every failure checks
the complete pre-command entity and armed-barrel state before a successful retry. The dedicated
ramp matrix covers all sixteen low-direction/coordinate-orientation combinations, exact half-step
endpoint events and snapshots, connected chains, and whole-stack retries. Ramp allocation
injection walks every JSON and typed allocation in a three-tick push/slide/slide command.
Fixture allocation injection walks every JSON and typed allocation in a two-tick
push/fall/switch/door command, checks the complete pre-command entity and fixture state at every
failure, and verifies deterministic successful retry.
Explosion allocation injection walks every JSON and typed allocation in a six-tick command with
two explosion waves and blast-driven falls, checks the complete pre-command entity, armed-barrel,
fixture, and outcome state at every failure, and verifies deterministic successful retry.
Stage-10 allocation injection additionally walks history-entry growth, JSON and typed rewind result
construction, and JSON and typed valid replacement while old history is nonempty. Every injected
failure checks current state, history depth, the live-allocation baseline, safe retry, and
invalid-free count. Dedicated typed tests retain load, move, state, rewind, replacement, and
terminal results across later operations and engine destruction.

## Stage-11 independent audit verification

### Passed

- Standalone strict C17 native build with warnings as errors: 15/15 tests passed.
- Parent native debug suite: 148/148 tests passed.
- The complete 47-operation stage-09 explosion differential corpus passed 100 consecutive
  executions; each execution includes eight fixed seeds, reassigned IDs, reordered arrays, and
  an identical repeated load for every seed.
- The 2,312-operation seeded history/lifecycle differential passed 100 consecutive executions;
  every execution also performs an internal byte-for-byte candidate repeat.
- Eight additional generated valid-level seeds matched for 2,096 complete reference/candidate
  operations. Canonical and reordered inputs, `C` and `en_US.UTF-8` locales, and repeated candidate
  serialization were identical.
- Emscripten 6.0.3 plus Node 26.5.0: parent 15/15 tests passed.
  The WebAssembly smoke executable covers canonical fixture derivation, door traversal, a legal
  exit win, terminal rejection, the existing three-tick barrel push/fall/explosion command, and a
  separate three-tick multi-barrel chain reaction. The suite includes six-tick allocation failure
  injection plus the dedicated falling, ramp, fixture, explosion, history/lifecycle, ABI-layout,
  and parser/boundary-fuzz matrices.
- The production WebAssembly link now consumes `c-port/libgame_rules_state_c.a` directly. Its wasm
  symbol table contains no C++ runtime, standard-library, RTTI, exception, or operator-new/delete
  symbols. The real in-app browser smoke passed load, move, rewind, and state retrieval with no
  console errors or warnings.
- Parent and standalone ASan/UBSan configurations build successfully.
- Clang static analysis reported one infeasible null-dereference path in `ticks_json`: its only
  caller has already returned on the allocation failure needed to make the session null. No warning
  was suppressed. Apple `leaks --atExit` reported zero leaks for allocation-failure, boundary-fuzz,
  explosion, and history executables.
- A copy containing only `c-port/` configured and built outside the repository, passed 15/15 tests,
  installed its archive, headers, notices, and yyjson license, and linked standalone strict C and
  C++ consumers. No symlink, parent-relative include, absolute source path, reference-source
  dependency, or missing vendored license was found. Its nested library-only regression also
  proved that `BUILD_TESTING=OFF` does not expose candidate test executables.

### Unavailable or blocked on this host

- ASan/UBSan runtime execution is blocked by repository policy on Apple Silicon/macOS because the
  Apple sanitizer runtime stalls during test discovery. The existing Ubuntu CI job is the required
  runtime handoff; only both sanitizer builds and non-sanitizer leak checks passed locally.
- No second native compiler family or native 32-bit runner is installed. `/usr/bin/gcc` is an
  AppleClang alias. Emscripten supplied an independent wasm32 build/layout/test result, but it is
  not evidence for native GCC or native ILP32 execution.
- A packaged Unreal build was not available. Static C and C++ consumers prove the intended thin
  wrapper boundary, but the checked-in Unreal scaffold still stages the C++ reference archive and
  is not a candidate integration test.
- The deterministic parser/ABI fuzz corpus passed, but no long-running coverage-guided fuzzing
  campaign was available in this checkout.

### Disputed checks

- None. No normative-rule, frozen-ABI, reference-implementation, or authored-golden conflict was
  found, so no golden output or normative rule was changed.

The frozen rules, ABI, reference implementation, and authored expected outputs were not changed.
See `docs/c-port-audit.md` for findings, exact commands, remaining risks, and recommendations.

## Extraction packaging and isolated-boundary verification

The extraction preconditions were rechecked on 2026-07-30. The checkout was on the dedicated
`c-rewrite` branch; stages 00 through 10 retained complete differential parity; the stage-11
audit explicitly recommended **Separate-repository extraction: GO**; and this status contained no
incomplete or disputed behavior. The frozen C header and three yyjson pin files still matched
their parent reference copies byte-for-byte before packaging.

`c-port/` is now the extraction-ready standalone root. The packaging change adds root licensing,
strict native/wasm presets, standalone CI, an installed CMake package exporting
`GameRules::StateC`, a native/wasm embedding smoke example, architecture/embedding/level/rules
documentation, a release checklist, Unreal guidance, and transition-period differential
instructions. No gameplay source, frozen ABI declaration, normative rule, golden output, C++
reference source, differential input, or current production consumer was changed or removed.

### Isolated evidence

The final intended 47-file tree was copied without symlinks to
`/private/tmp/game-rules-c17-extraction.B2gpNp`. That path is temporary evidence and is not a
source or build input.

- `cmake --preset native-debug`, `cmake --build --preset native-debug`, and
  `ctest --preset native-debug`: strict AppleClang 17 C17 build passed; 16/16 candidate-only,
  ABI, library-only, C++-header, and smoke tests passed.
- `cmake --install out/build/native-debug --prefix out/install`: installed the static archive,
  two public headers, `GameRulesStateC` package config/targets, project license, third-party
  notices, yyjson license/provenance, README, and all standalone docs.
- Direct `/usr/bin/cc -std=c17 -Wall -Wextra -Wpedantic -Werror` and
  `/usr/bin/c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror` consumers compiled, linked against
  the installed archive, and ran successfully.
- A clean consumer using `find_package(GameRulesStateC CONFIG REQUIRED)` linked
  `GameRules::StateC` from the install prefix and ran successfully.
- `nm -gU` found the stable `game_rules_` exports; `nm -u`, `strings`, and `otool -L`
  found no C++ standard-library, RTTI, exception, operator-new/delete, or C++ runtime dependency.
- `cmake --preset native-sanitized` and `cmake --build --preset native-sanitized` passed.
  Runtime execution remains intentionally unavailable on this Apple Silicon/macOS host under
  repository policy; Ubuntu CI remains the required sanitizer runtime gate.
- Emscripten 6.0.3 and Node 26.5.0: `emcmake cmake --preset wasm-debug`,
  `cmake --build --preset wasm-debug`, and `ctest --preset wasm-debug` passed 15/15 wasm32
  tests, including ABI layout and the embedding smoke. The first sandboxed link attempt could not
  write the Homebrew Emscripten SDK cache; the authorized retry changed no source and passed.
- Source scans found no symlink, parent-relative include, C++ source/header/runtime reference,
  old-checkout absolute path, untracked SDK path, or reference-runner/golden-data dependency in a
  production, test, example, or build file.
- The machine comparison between the manifest and isolated tree reported 47 paths on each side
  and no difference. A recursive diff, excluding generated `out/`, found the isolated source
  byte-identical to `c-port/`.
- The required parent `cmake --preset native-debug`, build, and CTest verification passed 148/148,
  including all retained C++ reference, authored contract, and differential tests. The standalone
  example defaults off when `c-port/` is embedded as a subdirectory, so current parent consumers
  and test inventory remain unchanged.

An initial isolated copy at `/private/tmp/game-rules-c17-extraction.p9hUzj` correctly rejected
the newly added smoke example because it called the frozen status function with an engine
argument. The example, not the ABI, was corrected; the final clean copy above was created afresh
and is the evidence of record.

### Exact extraction file manifest

Every path below is relative to the future standalone root. The classifications and exclusions
are documented in `c-port/docs/extraction-manifest.md`.

```text
.github/workflows/ci.yml
.gitignore
CMakeLists.txt
CMakePresets.json
LICENSE
README.md
THIRD_PARTY_NOTICES.md
cmake/GameRulesStateCConfig.cmake.in
docs/architecture.md
docs/embedding-api.md
docs/extraction-manifest.md
docs/level-format.md
docs/level-format.schema.json
docs/parity-transition.md
docs/release-readiness.md
docs/rules.md
docs/unreal-embedding.md
examples/engine_smoke.c
include/game_rules/c_allocator_api.h
include/game_rules/c_api.h
src/api_operations.c
src/c_api.c
src/c_api_internal.h
src/level.c
src/level_json.c
src/level_json_yyjson.c
src/level_json_yyjson.h
src/state.c
tests/abi_layout_test.c
tests/allocation_failure_test.c
tests/boundary_fuzz_test.c
tests/explosions_test.c
tests/falling_test.c
tests/fixtures_test.c
tests/flat_walking_test.c
tests/header_cpp_compatibility_test.cpp
tests/history_test.c
tests/level_loading_test.c
tests/library_only_build_test.cmake
tests/lifecycle_test.c
tests/player_pushing_test.c
tests/ramps_test.c
tests/wasm_smoke_test.c
vendor/yyjson/LICENSE
vendor/yyjson/README.game-rules.md
vendor/yyjson/yyjson.c
vendor/yyjson/yyjson.h
```

Reference-vs-candidate runners, comparator scripts, generated differential levels/transcripts,
reviewed golden outputs, C++ implementation files, and build products are intentionally excluded.
They remain in the C++ checkout. `c-port/docs/parity-transition.md` records how to rerun parity
with externally supplied runner paths, and `docs/development.md` records the exact future
dependency integration step. This extraction stage does not initialize a repository, create a
remote, commit, push, switch consumers, or delete the reference.

## Repeat-audit commands

To reproduce the longest differential checks without changing the frozen API, rules, C++
reference, or authored expected outputs, run the exact commands in `c-port/README.md`, plus:

```sh
ctest --preset native-debug -R game_rules.differential.seeded_history_lifecycle \
  --repeat until-fail:100 --output-on-failure
python3 tests/c-port/history_differential_test.py \
  out/build/native-debug/tests/game_rules_reference_runner \
  out/build/native-debug/tests/game_rules_candidate_runner \
  tools/c-port/compare_transcript.py
```

The fixed seeds are `0x10A11CE`, `0x10B4A2C`, `0x10C0FFEE`, `0x10D37E2`,
`0x10E501D`, `0x10F17E5`, `0x1012345`, and `0x1065432`, with 256 randomized
operations per seed after the deterministic lifecycle/history prefix. On Linux, also execute both
sanitizer test trees rather than only building them.
