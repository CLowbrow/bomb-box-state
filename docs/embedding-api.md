# C and WebAssembly embedding API

The embedding boundary is a versioned, JSON-based view of the same engine state and command
results exposed by the C++ API. It is intended for browser and foreign-function hosts; rendering,
input mapping, animation timing, persistence, and scheduling remain host concerns.

## Primitive C ABI

Include `game_rules/c_api.h`. An opaque `game_rules_engine*` owns one independent engine session,
including the current level and rewind history:

```c
game_rules_engine* engine = game_rules_engine_create();
char* loaded = game_rules_engine_load_level(engine, level_json, level_json_length);
char* moved = game_rules_engine_move(engine, GAME_RULES_DIRECTION_EAST);

game_rules_string_free(moved);
game_rules_string_free(loaded);
game_rules_engine_destroy(engine);
```

The stable direction values are north `0`, east `1`, south `2`, and west `3`. `create` returns
`NULL` only on allocation failure. Every stateful operation otherwise returns an allocated,
null-terminated UTF-8 JSON document; a `NULL` result means response allocation failed. Release
each result exactly once with `game_rules_string_free()`. Destroying or freeing `NULL` is allowed.
A destroyed engine handle must not be reused.

Level bytes use the existing [version 1 level format](level-format.md). The explicit byte length
means the input need not be null-terminated. A rejected decode or load leaves an already loaded
level and its history unchanged.

## Response contract, version 1

Every response contains `apiVersion`, `operation`, `status`, and `state`. `state` is `null` when no
authoritative level is available. A renderable state contains:

- the coordinate system, width, and height;
- canonical cells and fixtures using the level-format field names;
- current authoritative entities using decimal-string IDs and integer `bottomHalfSteps`;
- canonical `armedBarrelIds`, also encoded as decimal strings; and
- the current `outcome` (`ongoing`, `won`, or `lost`).

Entity IDs remain strings at this boundary so the full unsigned 64-bit range is safe in
JavaScript. Heights remain integer half-steps. Hosts should treat response objects as snapshots;
they do not alias engine-owned storage.

`loadLevel` returns `loaded`, `invalid_json`, `invalid_level`, `invalid_argument`, or
`invalid_engine`. A successful response contains the supplied dynamic `initialState`, ordered
initialization `ticks` with semantic events and authoritative `stateAfter` snapshots, the complete
final renderable `state`, and `outcome`. JSON errors include stable `code`, `byteOffset`, and `path`
fields. Structural errors include an ordered `errors` array with stable code and context fields.

`getState` returns `ok`, `no_level`, or `invalid_engine`. `move` additionally contains:

- `accepted` and the decoded cardinal `direction` (or `null` for malformed input);
- presentation-only rejection `events`;
- the dynamic `initialState` for an accepted turn;
- ordered `ticks`, each with its semantic events and authoritative dynamic `stateAfter`;
- the complete final renderable `state`; and
- the final `outcome`.

Move status strings are the same stable strings as `MoveStatus`, including `moved`, `no_level`,
`invalid_direction`, `world_boundary`, `stacked_push_target`, and current phase-boundary rejections
for unsupported geometry or fixtures. An accepted player push places the player's `entityMoved`
event before the pushed box or barrel's event in the same tick. A derived fall uses `entityMoved`
with cause `fall`; newly armed barrels emit `barrelArmed`. Crushing and terminal fall ticks may emit
`playerCrushed` and `levelLost`. `rewind` returns `accepted`, semantic events, the complete restored
state, and outcome with status `rewound` or `history_empty`.

The authored examples under `tests/contracts/browser_vertical_slice/v1/` are executable version-1
response examples shared by the native C ABI and WebAssembly tests.

## JavaScript/WebAssembly interface

The generated `game_rules.mjs` factory resolves to an Emscripten module with a thin `gameRules`
object. It converts allocated C strings to parsed objects and always frees the module memory:

```js
import createGameRulesModule from "./game_rules.mjs";

const module = await createGameRulesModule();
const api = module.gameRules;
const engine = api.createEngine();

try {
  const loaded = engine.loadLevel(levelObject); // a JSON string also works
  if (loaded.status !== "loaded") throw new Error(loaded.status);

  const before = engine.getState();
  const result = engine.move("east");
  const rewound = engine.rewind();
} finally {
  engine.destroy();
}
```

`gameRules.createEngine()` returns an engine object with `loadLevel`, `getState`, `move`, `rewind`,
and `destroy` methods. The numeric WebAssembly handle remains private. `move` accepts `north`,
`east`, `south`, or `west`; the same strings are available from the frozen `gameRules.directions`
object. `destroy` is idempotent, and any later stateful call throws a local lifecycle error instead
of passing a stale handle into WebAssembly.
