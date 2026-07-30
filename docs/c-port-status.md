# C17 rewrite status

## Stage-08 boundary

`c-port/` is the self-contained C17 production candidate. The C++ engine remains the frozen
behavioral reference, `docs/rules.md` remains normative, and `include/game_rules/c_api.h` remains
the ABI source of truth.

Stage 08 retains the complete stage-07 ramp and causal-closure boundary and establishes switches,
doors, exit teleporters, and terminal wins as a separately proven production boundary through both
public APIs. The fixture algorithms were already present in the candidate because earlier movement
stages needed fixture-aware blocking and stabilization; stage 08 removes the staging drift by
exercising their complete contract rather than treating that incidental coverage as completion:

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
- command-time explosion waves needed to close falling-barrel turns, including blast-driven falls,
  chain arming, support removal, fixture recomputation, and terminal loss;
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
  unsupported geometry, and terminal levels.

Real rewind behavior remains deliberately unimplemented. Ramp movement now enters the same atomic
command workspace as flat movement: fall, otherwise slide, otherwise explosion phases run to
closure with one immutable snapshot per tick. The frozen browser vertical-slice move still
matches; its rewind remains the first pinned difference.

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

| Surface | Stage-08 status |
| --- | --- |
| API versions, create/destroy, null handling | matched |
| Custom allocator extension | implemented; accepted load, snapshot, and move allocation sites are failure-injected |
| `game_rules_engine_status` | matched: `schema_ready` |
| Legacy JSON load/get-state | complete stage-03 behavior matched |
| Typed load/get-state | complete stage-03 owned graphs matched |
| Move | flat and ramp walking/pushing plus whole-stack slides, gravity, explosion closure, fixture effects, crushing, win, and loss matched |
| Rewind | empty-history scaffolding only; resolved history not started |

The frozen candidate header and pinned private yyjson 0.12.0 source, header, and license remain
byte-for-byte identical to the reference copies. No yyjson symbol or type crosses the public ABI.

## Differential and structural coverage

Separate reference and candidate processes avoid identical C-symbol collisions. The comparator
treats object order as non-semantic while preserving every array order.

Current proofs:

- `game_rules.differential.lifecycle_match`: all 16 no-level and lifecycle operations match.
- `game_rules.differential.level_load_parity`: all 18 stage-02 decode, validation, canonical-state,
  replacement, and preservation operations match.
- `game_rules.differential.stage03_load_state_parity`: all 20 load/get-state operations match for
  stable, unstable, reordered, fixture-bearing, coordinate-variant, terminal, and empty worlds.
- `game_rules.differential.stage04_flat_walking_parity`: all 34 load/move operations match for all
  cardinal directions, non-zero origins, both axis orientations, repeated commands, board edges,
  both ledge directions, compatible stack support, occupied push contact, closed doors, ineligible
  teleporters, unsupported ramp entry, malformed direction, terminal state, and reordered
  equivalent input.
- `game_rules.differential.stage05_player_pushing_parity`: all 40 load/move operations match for
  boxes and barrels in every cardinal direction, repeated pushes, reversed axes and non-zero
  origins, maximum entity IDs, top-of-stack selection, exact-height destination support, reordered
  source arrays and entity IDs, recursive occupancy, world edges, higher terrain, closed, active,
  and held-open doors, teleporters, unsupported ramp entry, and the frozen browser push.
- `game_rules.differential.stage06_gravity_parity`: all 23 load/move operations match for lower
  terrain, landing on stacks, top-of-stack removal, tall drops, gravity-triggered fixtures, barrel
  arming and explosion closure, chain loss, repeated terminal commands, simultaneous post-blast
  falls, teleporter win and terminal gating, maximum entity IDs, and reordered cells and entities.
- `game_rules.differential.stage07_ramps_parity`: all 52 load/move operations match for every low
  direction and coordinate orientation, endpoint and chain traversal, lateral lanes, downhill and
  ramp-exit pushes, stack heights, blocked retries, doors, exits, conflicts, ledges,
  fall-then-slide, slide-then-fall, maximum/reassigned IDs, and reordered input arrays.
- `game_rules.differential.stage08_fixtures_parity`: all 43 load/get-state/move operations match for
  every color, multiple same-color switches and doors, simultaneous activation/deactivation,
  canonical palette and door-coordinate order, held doors, multi-tick push/fall activation,
  explosion-driven fixture changes, fixture-aware walking/pushing/sliding, exit restrictions,
  immediate and movement-triggered wins, terminal commands, and reordered fixture/entity input.
- `game_rules.candidate_runner.stage05_browser_push_contract`: the candidate matches the existing
  authored load and move golden outputs without modifying them.
- `game_rules.candidate_runner.stage07_hardening_conflicts_contract`: canonical and reordered
  whole-stack slide conflicts match the existing authored golden output without modification.
- `game_rules.differential.gameplay_expected_incomplete`: the first browser difference is now
  operation 4 at `$.status` (`rewound` versus candidate `history_empty`) because resolved-state
  history remains later-stage work.

Candidate C tests verify complete initial, tick, event, final, snapshot, acceptance, status, and
outcome fields as well as result ownership and pointer/count invariants. The dedicated fixture
suite covers all palette colors, multiple same-color switches and doors, canonical fixture and
event order, independent immutable typed views, simultaneous switch/door changes, AND behavior,
held-door falls and closing, multi-tick gravity activation, loss ordering, exit-height rejection,
wins, and terminal commands. The falling corpus covers
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

## Stage-08 verification

- Standalone strict C17 native build with warnings as errors: 9/9 tests passed.
- Parent native debug suite: 137/137 tests passed.
- The complete 43-operation stage-08 fixture differential corpus passed 100 consecutive
  executions.
- Emscripten 6.0.3 plus Node 26.5.0: standalone 9/9 and parent 10/10 tests passed.
  The WebAssembly smoke executable covers canonical fixture derivation, door traversal, a legal
  exit win, and terminal rejection in addition to the existing three-tick barrel
  push/fall/explosion command. The suite includes multi-tick allocation failure plus the dedicated
  falling, ramp, and fixture matrices.
- Parent and standalone ASan/UBSan configurations build successfully.
- Per repository policy, sanitizer tests are not executed on this Apple Silicon/macOS host because
  the Apple sanitizer runtime stalls during test discovery. Runtime sanitizer execution remains an
  Ubuntu CI or other known-working Linux handoff.

No normative-rule, architecture, public-ABI, reference-implementation, or golden-output conflict
was found. Stage numbering in this document had drifted by prematurely grouping fixture behavior
into stage 07 and naming rewind as stage 08; the explicit stage-08 fixture command is now the
documented boundary. The frozen rules, ABI, reference files, and golden outputs were not changed.

## Stage-09 starting point

Stage 09 should add real resolved-state history and rewind without reopening the completed
stage-03 through stage-08 behavior:

1. Replace the one-byte history scaffold with owned canonical resolved-state entries.
2. Reserve the next history entry before planning a command, commit it only with the completed
   command result, and preserve both state and history at every injected allocation failure.
3. Implement repeated rewind, beginning-of-history rejection, deterministic replay, branching
   after rewind, terminal rewind, and replacement-level isolation through both public boundaries.
4. Port the rewind-dependent suffixes of the falling corpus and the existing browser and hardening
   contract sequences without changing their expected outputs.
5. Preserve the completed ramp traversal, pushing, whole-stack sliding, and causal-order behavior
   while history storage is added.

Keep the stage-02 through stage-08 differential corpora intact, keep the C++ engine and public ABI
frozen, and continue to stop on unresolved specification conflicts.
