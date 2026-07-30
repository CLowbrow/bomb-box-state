# C17 rewrite status

## Stage-05 boundary

`c-port/` is the self-contained C17 production candidate. The C++ engine remains the frozen
behavioral reference, `docs/rules.md` remains normative, and `include/game_rules/c_api.h` remains
the ABI source of truth.

Stage 05 retains the complete stage-04 flat-walking boundary and adds atomic player pushes on
already-supported flat terrain:

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
- one-cell box and barrel pushes at the player's bottom half-step, including top-of-stack removal,
  exact-height destination support, canonical two-event output, and reordered entity IDs; and
- exact pre-secondary-rule rejections for invalid direction, no level, world boundary, ledges,
  recursive or overlapping occupancy, non-top push targets, closed doors, ineligible teleporters,
  unsupported geometry, and terminal levels.

Gravity after a command, ramp traversal or pushing, movement-triggered fixture effects, crushing,
command-time explosions, and real rewind behavior are deliberately not implemented. A command
that would otherwise be legal but requires one of those later stages is rejected as
`unsupported_geometry` without mutation. The frozen browser vertical-slice push now matches; its
rewind is the first pinned difference.

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

Movement planning copies current state into scratch and describes the complete command with an
explicit transaction view. A push selects its target from the immutable current state, moves both
entities in scratch, canonicalizes the complete entity array, and exposes the player event before
the pushed-entity event. JSON or typed result snapshots are fully allocated from that view before
the session swaps current and scratch. Allocation failure therefore leaves both entities and the
history scaffold unchanged and cannot expose a partially committed tick. This transaction seam is
reusable for derived ticks in later stages.

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
both with cause `player` and from the same pre-tick state.

JSON object fields and presence/null behavior match the reference API for the completed surface.
Entity IDs, including `18446744073709551615`, are quoted canonical decimals. Output depends on no
pointer address, hash order, allocator order, wall clock, platform API, or unspecified traversal.

## Public contract inventory

| Surface | Stage-05 status |
| --- | --- |
| API versions, create/destroy, null handling | matched |
| Custom allocator extension | implemented; accepted load, snapshot, and move allocation sites are failure-injected |
| `game_rules_engine_status` | matched: `schema_ready` |
| Legacy JSON load/get-state | complete stage-03 behavior matched |
| Typed load/get-state | complete stage-03 owned graphs matched |
| Move | cardinal flat walking and supported flat box/barrel pushes matched; ramp/fixture/derived work pending |
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
- `game_rules.candidate_runner.stage05_browser_push_contract`: the candidate matches the existing
  authored load and move golden outputs without modifying them.
- `game_rules.differential.gameplay_expected_incomplete`: the first browser difference is now
  operation 4 at `$.status` (`rewound` versus candidate `history_empty`) because resolved-state
  history remains later-stage work.

Candidate C tests verify complete initial, tick, event, final, snapshot, acceptance, status, and
outcome fields as well as result ownership and pointer/count invariants. Allocation injection walks
every allocation in stable and multi-tick JSON/typed loads and successful JSON/typed walks and
pushes. Each failed push checks both entities and allocation ownership before retrying to prove no
hidden commit occurred.

## Stage-05 verification

- Standalone strict C17 native build with warnings as errors: 6/6 tests passed.
- Parent native debug suite: 130/130 tests passed.
- Emscripten 6.0.3 plus Node 26.5.0: 7/7 tests passed. The WebAssembly smoke executable performs
  repeated successful JSON and typed pushes in addition to stage-04 load/snapshot/walk checks, and
  the suite includes allocation-failure and the dedicated pushing matrix.
- Parent and standalone ASan/UBSan configurations build successfully.
- Per repository policy, sanitizer tests are not executed on this Apple Silicon/macOS host because
  the Apple sanitizer runtime stalls during test discovery. Runtime sanitizer execution remains an
  Ubuntu CI or other known-working Linux handoff.

No normative-rule, architecture, public-ABI, reference-implementation, or golden-output conflict
was found. Those frozen files and outputs were not changed.

## Stage-06 starting point

Stage 06 starts at post-movement stabilization; it must not reopen stage-03 initialization,
stage-04 walking, or stage-05 supported-push semantics:

1. Replace the single-tick command view with an owned ordered command-tick list while retaining the
   accepted movement tick as tick zero.
2. Begin with a box pushed over lower flat terrain: run the existing gravity machinery after tick
   zero, append the complete fall tick and snapshot, and commit only after the final result graph
   and the next resolved-state history entry are allocated.
3. Complete the flat-terrain causal closure before claiming the command accepted: crushing,
   barrel arming/detonation, switch/door derivation, and teleporter terminal handling must either be
   included in that stage or continue to reject before tick zero. Do not expose an intermediate
   unresolved command state.
4. Add the falling push behavior corpus and hardening contract sequences incrementally; move the
   pinned browser difference only after real rewind history is implemented.
5. Keep ramp traversal and ramp sliding rejected until their own movement stage unless stage 06
   explicitly expands and verifies that boundary.

Keep the stage-02 through stage-05 differential corpora intact, keep the C++ engine and public ABI
frozen, and continue to stop on unresolved specification conflicts.
