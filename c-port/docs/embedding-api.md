# C and WebAssembly embedding API

The C boundary has two contracts over the same engine:

- the typed data ABI is the preferred native foreign-function interface, including for Odin;
- the version 1 JSON ABI remains available for the existing JavaScript/WebAssembly adapter.

Both use an opaque `game_rules_engine*` for one independent session. Rendering, input mapping,
animation timing, persistence, and scheduling remain host concerns.

## Typed data ABI

Include `game_rules/c_api.h` and check `game_rules_data_api_version()`. Version 1 uses only
fixed-width integers, pointers, and plain C structs. C enums are not stored in ABI structs: every
tag, status, Boolean, and count is explicitly `uint32_t`. Entity IDs are `uint64_t`, and heights
are signed integer half-steps. This maps directly to Odin's `u32`, `u64`, `i32`, `rawptr`, and
pointer types without JSON parsing or numeric precision loss.

The basic lifecycle is:

```c
game_rules_engine* engine = game_rules_engine_create();

game_rules_level_definition level = { /* caller-owned arrays and counts */ };
game_rules_load_result loaded = {0};
uint32_t call = game_rules_engine_load_level_data(engine, &level, &loaded);
if (call == GAME_RULES_CALL_OK && loaded.accepted) {
    /* Read loaded.state, loaded.ticks, and their typed arrays. */
}
game_rules_load_result_dispose(&loaded);

game_rules_move_result moved = {0};
call = game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &moved);
if (call == GAME_RULES_CALL_OK) {
    /* Read moved.status, moved.ticks, moved.events, and moved.state. */
}
game_rules_move_result_dispose(&moved);

game_rules_engine_destroy(engine);
```

`game_rules_engine_load_level_data()` borrows the input arrays only for that call. The engine
copies and canonicalizes an accepted level. A zero count permits a null pointer; a nonzero count
with a null pointer, an unknown tagged kind, or an invalid enum value returns
`GAME_RULES_CALL_INVALID_ARGUMENT` without changing the engine. A structurally well-formed level
that violates the world schema returns `GAME_RULES_CALL_OK` with
`GAME_RULES_LOAD_INVALID_LEVEL` and ordered `game_rules_validation_error` values. Rejected loads
preserve the previous level and rewind history.

Each successful ABI call writes a result that owns one immutable allocation graph through its
opaque `owned_storage` field. All pointers nested under the result remain valid across later engine
calls and even engine destruction. Call the matching `*_result_dispose()` exactly once when
finished; it releases the graph and zeros the result. Do not copy an owning result struct.
Initialize a result to zero before its first call, and dispose it before reuse. Dispose functions
accept null.

The frozen shared ABI has no allocator parameter. The C17 library keeps it unchanged and adds
the separately included, versioned `game_rules/c_allocator_api.h` extension. Engines created with
`game_rules_engine_create_with_allocator_v1()` use the supplied callbacks for engine/session
storage and every result returned by that engine. The allocator context must remain usable until
the engine and all outstanding legacy and typed results have been released. The ordinary
`game_rules_engine_create()` continues to use the C runtime allocator.

The call return value reports boundary failures:

- `GAME_RULES_CALL_OK`: the operation ran; inspect its operation-specific status or `has_state`;
- `GAME_RULES_CALL_INVALID_ENGINE`: the engine pointer was null;
- `GAME_RULES_CALL_INVALID_ARGUMENT`: an output/input pointer, tag, or enum value was invalid;
- `GAME_RULES_CALL_ALLOCATION_FAILED`: the result owner itself could not be allocated.

Normal gameplay rejection is not a call failure. For example, an out-of-range movement value
returns `GAME_RULES_CALL_OK`, `GAME_RULES_MOVE_INVALID_DIRECTION`, `accepted == 0`,
`has_direction == 0`, and no presentation event. A valid direction blocked by gameplay returns
its specific move status and a typed move-blocked event.

### Odin binding outline

Link the platform archive named `game_rules_state_c` and import the functions with Odin's C
calling convention. Represent `game_rules_engine` as an opaque/incomplete type used only through
a pointer. Mirror `uint32_t`, `uint64_t`, and `int32_t` as `u32`, `u64`, and `i32`;
mirror C pointers as typed pointers or `rawptr` as appropriate. Preserve field order and native
alignment exactly—do not pack the structs.

At startup, check both version functions before constructing typed input. Treat every result with
`owned_storage` as a move-only owner in host code: do not value-copy it, and defer its matching
dispose function on every successful or partially successful call path. The input arrays may be
temporary for the duration of a load call because accepted data is copied. A custom allocator
context must outlive the engine and all result owners. Run `tests/abi_layout_test.c` for each new
Odin target architecture and compare a small load/move/rewind transcript before shipping.

### Views and events

`game_rules_snapshot` contains the canonical static level and authoritative resolved state.
Static cells are row-major; fixtures and entities use the engine's canonical spatial order.
Resolved-state arrays include entities, ascending armed-barrel IDs, palette-ordered active switch
colors, row-major open doors, and outcome.

Load and move results include initial and final dynamic states plus every ordered
`game_rules_tick`. Each tick contains its ordered semantic events and complete authoritative
`state_after`. The top-level snapshot is the complete current renderable state. Rewind returns its
restored dynamic state, semantic event, and complete current snapshot.

`game_rules_event` deliberately uses a fixed struct instead of a C union, which keeps hand-written
foreign bindings simple. Read fields according to `kind`:

| Kind | Meaningful fields |
| --- | --- |
| `GAME_RULES_EVENT_MOVE_BLOCKED` | `direction`, `move_status` |
| `GAME_RULES_EVENT_STATE_REWOUND` | none |
| `GAME_RULES_EVENT_ENTITY_MOVED` | `entity_id`, `from`, `to`, old/new bottom half-steps, `movement_cause` |
| `GAME_RULES_EVENT_BARREL_ARMED` | `entity_id` |
| `GAME_RULES_EVENT_BARREL_EXPLODED` | `entity_id`, `coordinate`, `bottom_half_steps` |
| `GAME_RULES_EVENT_PLAYER_CRUSHED` | `entity_id` (player), `other_entity_id` (crusher) |
| `GAME_RULES_EVENT_SWITCH_CHANGED` | `color`, `active` |
| `GAME_RULES_EVENT_DOOR_OPENED`, `GAME_RULES_EVENT_DOOR_CLOSED` | `coordinate`, `color` |
| `GAME_RULES_EVENT_LEVEL_WON`, `GAME_RULES_EVENT_LEVEL_LOST` | none |

## Legacy JSON ABI, version 1

`game_rules_api_version()` describes the existing JSON response contract, independently of the
typed data ABI version. The JSON functions are retained for WebAssembly compatibility:

```c
char* loaded = game_rules_engine_load_level(engine, level_json, level_json_length);
char* moved = game_rules_engine_move(engine, GAME_RULES_DIRECTION_EAST);

game_rules_string_free(moved);
game_rules_string_free(loaded);
```

Every non-null result is an allocated, null-terminated UTF-8 document. Release it exactly once
with `game_rules_string_free()`. A null result means response allocation failed. Level input uses
the [version 1 level format](level-format.md), and entity IDs are decimal strings so their full
unsigned 64-bit range remains safe in JavaScript.

Every response contains `apiVersion`, `operation`, `status`, and the complete current `state` (or
null without a level). Load and move responses include initial state, ordered ticks and events,
final state, and outcome. Rewind includes its semantic event, restored renderable state, and
outcome. Candidate-only tests verify the same responses, ownership, and ordering through native
and WebAssembly builds.

## JavaScript/WebAssembly adapter pattern

A host may generate an Emscripten module and wrap the JSON ABI with parsed JavaScript objects:

```js
import createGameRulesModule from "./game_rules.mjs";

const module = await createGameRulesModule();
const engine = module.gameRules.createEngine();
try {
  const loaded = engine.loadLevel(levelObject);
  const moved = engine.move("east");
  const rewound = engine.rewind();
} finally {
  engine.destroy();
}
```

The adapter should make `destroy()` idempotent, reject later stateful calls locally, copy input
JSON into wasm memory for the call, parse each returned UTF-8 document, and always invoke
`game_rules_string_free()`. The standalone CTest wasm smoke uses the public C boundary directly;
the illustrative JavaScript ownership layer is a host responsibility.
