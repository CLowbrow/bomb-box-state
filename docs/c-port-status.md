# C17 rewrite status

## Stage-03 boundary

`c-port/` is the self-contained C17 production candidate. The C++ engine remains the frozen
behavioral reference, `docs/rules.md` remains normative, and `include/game_rules/c_api.h` remains
the ABI source of truth.

Stage 03 completes authoritative level initialization and snapshot output:

- strict version-1 JSON and typed input decoding from stage 02;
- all 21 ordered world-schema validation errors;
- canonical C-owned static level data, resolved dynamic state, and cell-to-fixture indexing;
- initial fixture derivation and immediate teleporter wins;
- complete gravity, ramp-slide, explosion-wave, fixture, loss, and win stabilization before the
  first command;
- immutable, independently owned typed snapshots for initial, per-tick, final, and current state;
- deterministic legacy JSON serialization for the same complete graphs; and
- atomic allocate-then-swap replacement under allocation failure.

Player movement and real rewind behavior are deliberately not implemented. The browser
vertical-slice load and get-state operations match; its movement operation remains the first pinned
difference.

## Internal state and ownership

An accepted session owns three explicit resolved-state buffers:

- the canonical supplied initial state returned by load;
- the authoritative current stabilized state; and
- a scratch state used to calculate each tick from an immutable pre-tick state.

Each state uses bounded arrays for canonical entities, ascending armed-barrel IDs, palette-ordered
active switch colors, row-major open doors, and outcome. Static cells and fixtures are canonical
row-major arrays. A row-major cell index maps to its optional fixture without pointer or hash-order
dependence. Initialization scratch arrays are bounded by validated cell, fixture, and entity counts.
The tick list grows with checked arithmetic; every retained tick owns one immutable event/state
arena.

Typed input arrays remain borrowed only for the load call. Accepted input is copied and
canonicalized. Each public typed result is one independently owned contiguous arena, including all
nested tick, event, level, and resolved-state arrays. Empty collections use a zero count and null
pointer. Results survive later calls, successful replacement, and engine destruction. Disposal
frees the owner and zeros the result.

Invalid JSON, invalid structural loads, and allocation failures leave the prior level and current
state unchanged. A successful replacement is committed only after the complete candidate session
and return graph or JSON response have been allocated.

## Ordering and serialization parity

The candidate matches the frozen observable ordering rules:

- cells, fixtures, open doors, physical tick work, and spatial events use numeric `(y, x)` order;
- entities use `(y, x, bottomHalfSteps, id)` order;
- armed barrel IDs are ascending;
- active colors and switch-change events use red, green, blue, yellow palette order;
- door changes follow canonical fixture order;
- stack events preserve pre-tick bottom-to-top order; and
- explosion sources and targets follow their specified spatial and stack ordering.

JSON object fields and presence/null behavior match the reference API. Entity IDs, including
`18446744073709551615`, serialize as canonical quoted decimal strings. Integer formatting is local,
base-10 conversion and does not depend on locale. Output depends on no pointer address, hash table,
allocator order, wall clock, platform API, or unspecified container traversal.

## Public contract inventory

| Surface | Stage-03 status |
| --- | --- |
| API versions, create/destroy, null handling | matched |
| Custom allocator extension | implemented; every accepted load and snapshot allocation site is failure-injected |
| `game_rules_engine_status` | matched: `schema_ready` |
| Legacy JSON load | complete decode, validation, physical initialization, ticks, events, state, and atomic replacement matched |
| Legacy get-state | complete no-level and loaded snapshots matched |
| Typed load | complete owned initial/tick/final/current graphs matched |
| Typed get-state | complete independently owned snapshot matched |
| Move | no-level and invalid-direction scaffolding only; player commands not started |
| Rewind | empty-history scaffolding only; resolved history not started |

The frozen candidate header remains byte-for-byte identical to the reference header. The pinned,
private yyjson 0.12.0 source, header, and license also remain byte-for-byte identical to the
reference pin and expose no yyjson symbol or type through the public boundary.

## Differential and structural coverage

Separate reference and candidate processes avoid identical C-symbol collisions. The comparator
treats object order as non-semantic while preserving every array order.

Current proofs:

- `game_rules.differential.lifecycle_match`: all 16 no-level and lifecycle operations match.
- `game_rules.differential.level_load_parity`: all 18 stage-02 decoding, validation, canonical
  state, replacement, and preservation operations continue to match.
- `game_rules.differential.stage03_load_state_parity`: all 20 load/get-state operations match for
  stable and unstable worlds, canonical and reordered equivalents, multiple and maximum entity
  IDs, east/north and west/south coordinate systems, initial fixtures, fixture changes during a
  terminal tick, gravity, slides, explosions, terminal initialization, and empty collections.
- `game_rules.differential.gameplay_expected_incomplete`: the first remaining browser difference
  is still operation 3 at `$.status` (`moved` versus candidate `no_level`).

Candidate C tests additionally verify every snapshot graph structurally: pointer/count invariants,
canonical ordering, typed/engine non-aliasing, cross-result non-aliasing, owner survival after
engine destruction, exact disposal clearing, intermediate armed IDs, and null pointers for empty
collections. Allocation injection walks every allocation in stable and multi-tick unstable JSON
and typed loads, including tick/state arenas and top-level return graphs.

## Stage-03 verification

- Standalone strict C17 native build with warnings as errors: 4/4 tests passed.
- Parent native debug suite: 125/125 tests passed.
- Emscripten 6.0.3 plus Node: 4/4 candidate tests passed, including typed multi-tick snapshot,
  parser/load, and allocation-failure coverage.
- Parent and standalone ASan/UBSan configurations built successfully.
- Per repository policy, sanitizer tests were not executed on this Apple Silicon/macOS host because
  the Apple sanitizer runtime stalls during test discovery. Runtime sanitizer execution remains an
  Ubuntu CI or other known-working Linux handoff.

No normative-rule, architecture, public-ABI, reference-implementation, or golden-output conflict
was found. Those frozen files and outputs were not changed.

## Stage-04 handoff

Stage 04 should add player-command behavior without reopening stage-03 load/state semantics:

1. Port command gating and player movement planning, including walking, ramp traversal, atomic
   single-entity pushes, rejection statuses, and move-blocked events.
2. Reuse the stage-03 immutable pre-tick/current-scratch model and physical resolution functions
   for all derived move ticks; do not create a second physics implementation.
3. Establish resolved-state history only at command boundaries, then implement exact rewind,
   repeated rewind, branching, terminal rewind, and replacement isolation.
4. Complete loaded-level JSON and typed move/rewind result graphs, preserving independent ownership
   and extending allocation injection to every new command/history allocation.
5. Replace the pinned browser movement mismatch only when operation 3 reaches exact parity, then
   expand differential coverage through the remaining browser and hardening command transcripts.

Keep the stage-02 and stage-03 differential corpora intact, keep the C++ engine and public ABI
frozen, and continue to stop on any unresolved specification conflict.
