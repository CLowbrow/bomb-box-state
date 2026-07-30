# C17 rewrite status

## Stage-04 boundary

`c-port/` is the self-contained C17 production candidate. The C++ engine remains the frozen
behavioral reference, `docs/rules.md` remains normative, and `include/game_rules/c_api.h` remains
the ABI source of truth.

Stage 04 retains the complete stage-03 initialization and snapshot boundary and adds the first
player-command slice:

- strict version-1 JSON and typed input decoding from stage 02;
- all 21 ordered world-schema validation errors;
- canonical C-owned static level data, resolved dynamic state, and cell-to-fixture indexing;
- complete fixture, gravity, ramp-slide, explosion-wave, loss, and win stabilization before the
  first command;
- immutable independently owned typed and JSON initial, tick, final, and current snapshots;
- atomic allocate-then-swap level replacement under allocation failure;
- north, east, south, and west decoding through both move boundaries;
- one-cell walking across flat terrain, including non-zero origins, reversed axes, and compatible
  stack support; and
- exact pre-secondary-rule rejections for invalid direction, no level, world boundary, ledges,
  occupied push contact, closed doors, ineligible teleporters, unsupported geometry, and terminal
  levels.

Pushing, ramp traversal, movement-triggered fixture effects, derived command physics, explosions,
and real rewind behavior are deliberately not implemented for commands. A command that requires
one of those later stages is rejected without mutation; push contact currently returns `occupied`.
The browser vertical-slice push therefore remains the first pinned difference.

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
explicit transaction view. JSON or typed result snapshots are fully allocated from that view before
the session swaps current and scratch. Allocation failure therefore leaves authoritative state
unchanged and cannot expose a partially committed tick. This transaction seam is reusable for
multi-entity and derived ticks in later stages.

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
`player`.

JSON object fields and presence/null behavior match the reference API for the completed surface.
Entity IDs, including `18446744073709551615`, are quoted canonical decimals. Output depends on no
pointer address, hash order, allocator order, wall clock, platform API, or unspecified traversal.

## Public contract inventory

| Surface | Stage-04 status |
| --- | --- |
| API versions, create/destroy, null handling | matched |
| Custom allocator extension | implemented; accepted load, snapshot, and move allocation sites are failure-injected |
| `game_rules_engine_status` | matched: `schema_ready` |
| Legacy JSON load/get-state | complete stage-03 behavior matched |
| Typed load/get-state | complete stage-03 owned graphs matched |
| Move | cardinal decoding, flat walking, and pre-secondary-rule rejections matched; push/ramp/fixture/derived work pending |
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
- `game_rules.differential.gameplay_expected_incomplete`: the first browser difference remains
  operation 3 at `$.status` (`moved` versus candidate `occupied`) because pushing is stage-05 work.

Candidate C tests verify complete initial, tick, event, final, snapshot, acceptance, status, and
outcome fields as well as result ownership and pointer/count invariants. Allocation injection walks
every allocation in stable and multi-tick JSON/typed loads and successful JSON/typed moves. Each
failed move is retried to prove no hidden commit occurred.

## Stage-04 verification

- Standalone strict C17 native build with warnings as errors: 5/5 tests passed.
- Parent native debug suite: 127/127 tests passed.
- Emscripten 6.0.3 plus Node: 5/5 candidate tests passed. The WebAssembly smoke executable performs
  successful JSON and typed flat moves in addition to stage-03 load/snapshot checks, and the suite
  includes allocation-failure coverage.
- Parent and standalone ASan/UBSan configurations build successfully.
- Per repository policy, sanitizer tests are not executed on this Apple Silicon/macOS host because
  the Apple sanitizer runtime stalls during test discovery. Runtime sanitizer execution remains an
  Ubuntu CI or other known-working Linux handoff.

No normative-rule, architecture, public-ABI, reference-implementation, or golden-output conflict
was found. Those frozen files and outputs were not changed.

## Stage-05 handoff

Stage 05 should extend player commands without reopening stage-03 initialization or stage-04 flat
walking semantics:

1. Port atomic single-entity box/barrel pushes on flat terrain, including top-of-stack selection,
   recursive/overlapping destination rejection, event order, and remaining push statuses.
2. Generalize the stage-04 transaction from one walk tick to the complete ordered movement tick;
   allocate result and history storage before committing either moved entity.
3. Add differential coverage for all directions, reversed axes, stack heights, entity-order
   permutations, boundary/door/teleporter interactions, and browser operation 3.
4. Leave falling, ramp traversal/sliding, switch/door transitions, teleporter wins, explosions, and
   rewind for explicit later stages unless stage 05 deliberately expands its documented boundary.
5. Extend allocation injection and WebAssembly command smoke coverage for pushes before removing
   the pinned browser difference.

Keep the stage-02 through stage-04 differential corpora intact, keep the C++ engine and public ABI
frozen, and continue to stop on unresolved specification conflicts.
