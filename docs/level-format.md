# Bomb Box level format, version 1

This document is the normative serialization specification for a Bomb Box level. `README.md`
remains normative for gameplay and state transitions; this document defines how a level's supplied
initial data is represented and exchanged.

## Goals and boundary

The format is UTF-8 JSON. JSON is deliberately used instead of a custom binary or text format
because levels are expected to be created in a browser, inspected by people, stored by ordinary web
services, and shared independently of a particular engine build.

The portable C++ core owns the authoritative decoder, encoder, and structural validator. Generic
JSON syntax parsing uses a pinned, privately compiled copy of the MIT-licensed yyjson 0.12.0; its
types do not enter the public API and its upstream symbols have internal linkage. The codec does not
use C++ exceptions, a filesystem, browser APIs, or Unreal. JSON is passed as an in-memory byte
string. Because the codec is in separate objects in the static library, an embedded host that only
constructs `LevelDefinition` values need not link it into its final binary.

Version 1 contains only rule-relevant initial data. Titles, authors, descriptions, thumbnails,
sharing permissions, server IDs, and creation timestamps are distribution metadata and must be
stored beside the level document rather than added to it. This keeps identical playable content
identical across hosts and avoids making the rules engine an owner of presentation or service data.

## Complete example

```json
{
  "format": "bomb-box-level",
  "version": 1,
  "coordinateSystem": {"origin":{"x":10,"y":-4},"positiveX":"west","positiveY":"south"},
  "width": 3,
  "height": 1,
  "cells": [
    {"coordinate":{"x":10,"y":-4},"type":"flat","elevation":0},
    {"coordinate":{"x":11,"y":-4},"type":"ramp","lowDirection":"east","lowElevation":0},
    {"coordinate":{"x":12,"y":-4},"type":"flat","elevation":1}
  ],
  "fixtures": [
    {"coordinate":{"x":10,"y":-4},"type":"switch","color":"blue"},
    {"coordinate":{"x":12,"y":-4},"type":"door","color":"blue"}
  ],
  "entities": [
    {"id":"40","type":"barrel","coordinate":{"x":10,"y":-4},"bottomHalfSteps":6},
    {"id":"20","type":"box","coordinate":{"x":12,"y":-4},"bottomHalfSteps":2},
    {"id":"30","type":"player","coordinate":{"x":12,"y":-4},"bottomHalfSteps":4}
  ]
}
```

The accompanying [JSON Schema](level-format.schema.json) describes the syntactic shape. The schema
cannot express all cross-entry rules, so passing it is not a substitute for `validate_level()` or
`decode_level_json()`.

## Document members

Every member is required, including arrays that are empty. Unknown and duplicate members are
errors. JSON object member order and input array order have no gameplay meaning.
All fields described as integers must use a JSON number token containing an optional minus sign and
decimal digits only; fractional and exponent notation are rejected even when they denote a whole
number.

| Member | Type | Meaning |
| --- | --- | --- |
| `format` | string | Must be exactly `bomb-box-level`. |
| `version` | integer | Must be exactly `1`. |
| `coordinateSystem` | object | Declares the numeric origin and how increasing axes map to cardinal directions. |
| `width` | integer | Positive `uint32` board width. |
| `height` | integer | Positive `uint32` board height. |
| `cells` | array | One cell for every coordinate in the declared rectangle. |
| `fixtures` | array | Zero or one fixture for any flat cell. |
| `entities` | array | The supplied initial entity population and bottom heights. |

`coordinateSystem.origin.x` and `.y` are signed 32-bit integers. `positiveX` is `east` or `west`;
`positiveY` is `north` or `south`. Coordinates in the rectangle are the numeric range from `origin`
through `origin + (width - 1, height - 1)`. Axis declarations affect cardinal movement, not which
numeric coordinates are included.

All occurrences of a coordinate use an object with signed 32-bit integer `x` and `y` members:

```json
{"x": 0, "y": 0}
```

## Cells

A flat cell has this shape:

```json
{"coordinate":{"x":0,"y":0},"type":"flat","elevation":0}
```

Its `elevation` is an integer from `-1073741824` through `1073741823`, inclusive, so its doubled
half-step value fits in a signed 32-bit integer. It supplies a floor at that integer height.

A ramp cell has this shape:

```json
{"coordinate":{"x":1,"y":0},"type":"ramp","lowDirection":"west","lowElevation":0}
```

`lowDirection` is `north`, `east`, `south`, or `west`. It identifies the ramp's low endpoint from
the ramp cell under the declared coordinate axes. `lowElevation` is the integer elevation of that
endpoint; the opposite endpoint must be a flat cell at `lowElevation + 1`. The ramp-center support
height is therefore `lowElevation + 0.5`. `lowElevation` ranges from `-1073741824` through
`1073741822`, inclusive, so the low endpoint, center, and high endpoint all fit the height type.

Every in-bounds coordinate must have exactly one cell. Sparse maps and implicit default cells are
not part of version 1. Explicit cells make accidental holes detectable and leave room for a future
explicit void-cell type without changing the meaning of an omitted entry.

## Fixtures

Fixtures can occur only on flat cells and use one of these shapes:

```json
{"coordinate":{"x":0,"y":0},"type":"switch","color":"red"}
{"coordinate":{"x":1,"y":0},"type":"door","color":"red"}
{"coordinate":{"x":2,"y":0},"type":"exit"}
```

Switch and door colors are `red`, `green`, `blue`, or `yellow`. Exit teleporters have no color.
There may be at most one fixture at a coordinate. Initial effective switch and door states are not
serialized because they are derived during level initialization.

## Entities and heights

An entity has this shape:

```json
{"id":"42","type":"box","coordinate":{"x":0,"y":0},"bottomHalfSteps":2}
```

`type` is `player`, `box`, or `barrel`. A valid level has exactly one player. `id` is a unique
unsigned 64-bit integer encoded as its canonical decimal string: the least value is `1`, leading
zeroes are not allowed, and the greatest value is `18446744073709551615`. ID `0` is reserved to mean
"no entity" at API boundaries. IDs are strings because JavaScript JSON
numbers cannot exactly represent every `uint64` value. Editors must keep IDs as strings or `bigint`
values and must not round-trip them through `number`.

`bottomHalfSteps` is a signed 32-bit integer and gives the bottom height in units of one half-step:

- `0` means height `0`;
- `1` means height `0.5`;
- `2` means height `1`;
- `-3` means height `-1.5`.

This representation is exact across native, WebAssembly, and Unreal builds. It also represents
entities in initially unstable levels and gaps within a supplied stack; initialization later settles
them according to `README.md`. Each entity is one height unit, or two half-steps, tall. Initial
volumes must not overlap, no entity may intersect its cell surface, and the player must be the top
entity in its column.

## Validation and error model

Loading has three distinct stages:

1. Parse JSON syntax and enforce this version's field names, types, ranges, and enum spellings.
2. Decode an in-memory `LevelDefinition` and run the shared structural rules in
   `validate_level()`.
3. Canonicalize the valid definition before it is returned or committed to an engine.

`decode_level_json()` reports at most one format error as a stable `LevelJsonErrorCode`. JSON syntax
errors include yyjson's byte offset. Shape errors include a precise JSON Pointer path and use byte
offset zero because yyjson's successfully parsed DOM does not retain source positions. If the
document shape is valid but its level is structurally invalid, it
instead returns the ordered `ValidationError` list used by `Engine::load_level()`. No partially
decoded level is returned on either kind of failure. A host therefore decodes first and calls
`Engine::load_level(*result.level)` only after `result.accepted()` is true; a failed decode cannot
replace the currently loaded level.

The decoder defaults to a 16 MiB input limit and nesting depth 32 to bound common untrusted-input
costs. Those are caller-configurable resource limits, not restrictions on the version 1 data model.
Services accepting shared levels should also impose appropriate request, storage, and board-size
limits before simulation.

## Canonical encoding

`encode_level_json()` first validates the definition. Invalid definitions do not produce JSON.
Valid output is deterministic:

- object fields use the order shown in the complete example;
- cells and fixtures are sorted by numeric `(y, x)`;
- entities are sorted by `(y, x, bottomHalfSteps, id)`;
- enum values use the lowercase spellings in this document;
- integers use base-10 with no leading zeroes or positive sign;
- entity IDs are quoted decimal strings;
- output uses UTF-8, LF line endings, and exactly one final newline.

Whitespace is not significant when reading. Producers other than the core encoder do not have to
copy its whitespace, field order, or array order. Content hashes and caches should use output from
the canonical encoder rather than hashing arbitrary accepted input bytes.

## Compatibility policy

Version 1 readers must reject unknown members, unknown enum values, a different `format`, and any
unsupported `version`. This strictness is intentional: silently ignoring a misspelled or newer rule
field could make a shared level play differently from its author's intent.

Additive or behavioral changes to rule-relevant level data require a new integer format version.
A future reader may explicitly migrate an older document to its current in-memory representation,
but must never guess how to interpret a newer version. Distribution metadata may evolve outside the
gameplay document without incrementing this version.
