# Bomb Box state/rules engine

Bomb Box is a deterministic state and rules engine for a turn-based,
Sokoban-like puzzle game. The engine owns the authoritative world state. A UI
submits one cardinal movement command and receives the complete, tick-by-tick
result of that command, including every fall, slide, explosion, door change,
and terminal event caused by it.

This document is intended to become the normative behavior specification for
the engine. Rules marked **open design question** must be settled before the
affected behavior is implemented.

## Normative language

The words **must**, **must not**, **may**, and **should** describe engine
requirements. Examples illustrate the requirements but do not replace them.

## Terminology and world model

### Coordinates and directions

The world is a grid of cells. Player input is exactly one of the four cardinal
directions: north, east, south, or west. Diagonal movement and diagonal blast
effects do not exist.

The level format must eventually define its coordinate origin, axis directions,
board bounds, and behavior outside the board. See [Open design questions](#open-design-questions).

### Cells, fixtures, and entities

The engine distinguishes three kinds of world data:

- A **cell** supplies geometry: a coordinate and either a flat surface or a
  ramp.
- A **fixture** is attached to a cell: a switch, door, or exit. Fixtures are
  not members of entity stacks.
- An **entity** occupies physical space: the player, a box, or an explosive
  barrel. Every entity must have a stable unique ID.

This distinction permits an entity to occupy the same cell as a switch or an
open door without treating the fixture as part of the entity stack.

### Flat cells, height, and support

- A flat cell has an integer elevation `e` and supplies a floor surface at
  vertical coordinate `z = e`.
- Every entity has height `1` and occupies the half-open interval `[z, z + 1)`.
- Entities in the same cell may form a bottom-to-top stack. Their occupied
  intervals must not overlap.
- A stack can grow as high as the finite entity population permits.
- An entity is **supported** when its bottom is resting on the cell surface or
  on the top of the entity directly below it.
- A player may be the top entity of a stack but must never have another entity
  above it. If an involuntary effect would place or drop something onto the
  player, the level is lost.
- An entity is **unstacked** only when it is the sole entity in its cell.
  Switches, exits, and open doors do not affect whether an entity is unstacked
  because they are fixtures, not entities.

For example, a barrel starting on a flat cell of elevation `3` has bottom
height `3`. If it moves into a cell of elevation `1` containing one box, the
box supplies a landing surface at height `2`. The barrel therefore falls by
`1`, not `2`.

## Legal stacks

- Boxes and barrels may support boxes, barrels, or the player.
- The player must be the top entity in its cell.
- A voluntary player move that would put an entity above the player is
  illegal.
- An involuntary effect that causes an entity to land on the player causes a
  loss instead of creating a legal stack.
- Removing an entity from the middle of a stack makes every entity above it
  unsupported. Those entities fall as a stack during the following derived
  tick.

## Player commands and movement

### Input gating

- The engine accepts at most one player command at a time.
- A command starts a **turn**.
- After applying the player's action, the engine must resolve the complete
  causal chain to a stable state before accepting another command.
- While falls, slides, explosions, chain reactions, or other derived actions
  remain pending, additional player input must be rejected or left outside the
  engine by the wrapper. It must never be inserted into the active turn.
- Consequently, the player cannot race an explosion chain, move between two
  explosion waves, or act while another entity is still falling or sliding.

### Walking

- The player may move only one cell in a cardinal direction per command.
- The player cannot climb a ledge.
- The player cannot walk or fall down a ledge.
- The player changes elevation only by traversing a ramp.
- The player may walk onto the top of a box when that box's top is at the same
  height as the player's current supporting surface.
- A closed door and an ineligible exit cell block walking.

### Player pushing

- The player may push a box or barrel one cell in the command direction.
- The player may push an entity only if it is unstacked: it must be the sole
  entity in its cell. A box or barrel with anything above or below it cannot be
  pushed by the player.
- A player push may move only that one entity. It cannot recursively push an
  entity occupying the destination cell.
- Push legality is calculated from the state at the start of the movement
  tick. An occupied destination remains blocking even if its occupant could
  theoretically move later in the same tick.
- A push is atomic: either the pushed entity and player both move, or neither
  moves.
- A box or barrel may be pushed horizontally onto a support surface at its
  current height or over a lower surface, after which it falls.
- A box or barrel cannot be pushed into a higher solid surface.
- A player may push a box or barrel down a ramp but not up a ramp.
- A player cannot push an entity or stack already occupying a ramp.

Implementation note: a future version may permit pushing the top of a stack, a
stack suffix, or an entire stack. Movement APIs and events should therefore be
capable of describing more than one moved entity even though the initial
player-push rules permit only one.

## Falling

- Boxes and barrels may move or be pushed off ledges of any positive height.
- The player cannot voluntarily or safely fall.
- An unsupported entity or stack falls to the highest legal support surface in
  its current cell.
- A falling stack retains its bottom-to-top order and falls as one unit.
- A fall is resolved in a derived tick after the movement or removal that
  caused the loss of support.
- A box survives a fall of any distance.
- A barrel that falls any positive distance becomes armed and must explode
  after its forced movement has finished.
- If a falling entity or stack would land on the player, the level is lost.
- A fall event should record the complete start height, end height, and fall
  distance rather than emitting one tick per unit of height.

## Ramps

### Geometry

A ramp occupies a cell and connects exactly two cardinally adjacent flat cells
on opposite sides of it. It has:

- An orientation: north/south or east/west.
- A low direction.
- A low endpoint surface at integer height `X`.
- A high endpoint surface at height `X + 1`.
- A ramp-center support surface at height `X + 0.5`.

Entry or exit perpendicular to the ramp orientation is prohibited. A ramp does
not connect any of its perpendicular neighbors, even if their elevations happen
to match one of its endpoints.

### Player traversal

- The player may traverse a ramp in either direction.
- Moving between an endpoint and the ramp center changes player height by
  `0.5` and is ramp traversal, not climbing or falling.
- Traversing a one-cell ramp therefore normally takes two commands: endpoint
  to ramp, then ramp to the opposite endpoint.

### Boxes, barrels, and ramp stacks

- A box or barrel may be pushed onto a ramp only from its high endpoint and in
  the downhill direction.
- Moving from the high endpoint onto the ramp and sliding from the ramp to the
  low endpoint are ramp movements, not falls. These movements do not arm a
  barrel.
- A box or barrel on a ramp automatically tries to slide toward the low
  endpoint.
- If the low destination is blocked, the entity remains on the ramp.
- If the blockage later clears, the entity automatically tries again during
  the next derived tick.
- An entity may fall onto a ramp from somewhere other than the ramp's high
  endpoint. That movement is a fall, not a slide. A barrel falling any positive
  distance onto a ramp becomes armed.
- Additional entities may fall onto a blocked ramp occupant and form a stack.
- When a ramp stack is able to slide, the **entire stack moves together** to
  the low endpoint during one derived tick. Upper members do not fall one at a
  time.
- Moving a whole ramp stack is automatic ramp behavior, not a player pushing a
  stack.
- Because the world has no overhang geometry, there is no separate overhead
  obstruction that can block only an upper member of a ramp stack. The move is
  legal or blocked for the stack as a whole.
- If the bottom entity of a ramp stack disappears, the remaining upper stack
  falls onto the ramp surface before it can try to slide.

Switches, doors, and exits cannot be placed on ramp cells.

## Explosive barrels

This document uses **barrel** consistently for the explosive movable entity.

### Becoming armed

A barrel becomes **armed** when either:

- It falls a positive distance; or
- It is affected by another barrel's explosion.

An armed barrel must finish all movement caused by the triggering interaction
before it explodes. In particular:

- A blast-hit barrel first attempts its one-cell blast push.
- If that push sends it over a ledge, it falls.
- If it reaches a ramp, it completes any available automatic slide.
- It then explodes at its final settled position.
- If movement is blocked, it remains in place and still explodes.
- A barrel can be armed more than once but may explode only once.

### Blast area

- An explosion affects its source cell and the four cardinally adjacent cells.
- It never affects a diagonally adjacent cell.
- Cell elevation matters. In an adjacent cell, a target is affected only when
  its occupied vertical interval overlaps the exploding barrel's interval.
- Merely touching at a top/bottom boundary across two different cells does not
  count as overlap.
- The half-height of a ramp may allow a ramp occupant to overlap vertically
  with an occupant at either ramp endpoint.
- Doors, switches, exits, and cell geometry are not damaged or moved by a
  blast.
- The exploding barrel is removed from the world as part of its explosion.

### Adjacent-cell effects

- An affected box is pushed at most one cell directly away from the exploding
  barrel, if the movement is legal. It is not destroyed.
- An affected barrel is pushed by the same rule and becomes armed whether or
  not its movement succeeds.
- An affected player causes a loss condition.

How a blast acts on a target that is part of a stack is not yet fully specified;
see [Open design questions](#open-design-questions).

### Same-cell vertical effects

The source cell has no horizontal "away" direction. Its rules are therefore
different from adjacent-cell blast pushes:

- A barrel directly touching the exploding barrel immediately above or below
  becomes armed.
- The blast does not skip over another entity or empty vertical space.
- A contiguous vertical run of barrels consequently reacts as a visible chain
  over successive explosion waves.
- A barrel below the source remains supported and later explodes in place
  unless another interaction moves it.
- A barrel above the source becomes unsupported when the source disappears,
  falls with the stack above it during the following derived tick, and then
  explodes after settling.
- A directly touching player causes a loss condition.
- A box in the source cell receives no horizontal impulse because there is no
  unique away direction. A box above the source may nevertheless fall when the
  source disappears.

### Simultaneous blast impulses

All explosions in the same explosion wave inspect the same pre-wave state.

- One or more impulses from the same direction produce at most one one-cell
  movement.
- If an entity receives impulses from two or more different directions, all of
  its horizontal blast movement is canceled.
- Counts do not matter. For example, two eastward impulses and one westward
  impulse still conflict and result in no movement.
- A barrel hit by any blast becomes armed even when its movement is canceled.
- If two otherwise valid movements require overlapping destination space, the
  conflicting movements fail.
- Barrels already exploding in the current wave cannot be moved by that wave.
- Entity ID or collection iteration order must never decide a physics result.

## Switches and doors

### Switches

- Every switch has a color. The initial palette is red, green, blue, and yellow.
- A switch can be placed only on a flat cell.
- A switch cannot share a cell with a door.
- A switch is pressed when an entity is resting directly on the switch's floor
  surface. An entity higher in the stack does not itself press the switch.
- A box, barrel, or player may press a switch.
- All switches of a color use AND behavior: that color is active only while
  every switch of that color is pressed.
- All doors of an active color are open.

### Doors

- Every door has a switch color and cannot move.
- A closed door makes its entire cell impassable at every stack height.
- An open door does not obstruct movement.
- An entity may occupy a cell containing an open door. The door fixture does
  not make that entity stacked.
- If a door's color becomes inactive while its cell contains any entity at any
  height, the door remains effectively open.
- Once that cell becomes empty, the inactive door closes at the end of that
  movement tick. It does not matter whether the last entity walked, was pushed,
  fell, slid, or disappeared in an explosion.
- A door must never close onto, crush, or divide a stack.
- Door state is recomputed after each tick. An open/close event is emitted only
  when effective passability changes.

A level currently must not contain a door color without a corresponding switch
or a switch color without a corresponding door. Whether this should remain an
engine invariant or become only a level-editor warning is an open question.

## Exits and terminal outcomes

- An exit can be placed only on a flat cell and cannot share a cell with a door.
- Only the player may enter an exit cell. Boxes and barrels treat it as
  impassable.
- The player wins only when directly touching the exit floor.
- A player in the exit cell while standing on another entity has not reached
  the exit.
- If win and loss conditions occur during the same tick, **win takes
  precedence**.
- After the level becomes terminal, the engine accepts no more player input.

Whether a win immediately cancels already-scheduled derived ticks or whether
the active causal chain finishes before the final outcome is emitted remains an
open design question.

## Turns, ticks, and causal resolution

### Definitions

- A **turn** begins with one player command and includes every consequence of
  that command until the world becomes stable or terminal handling stops it.
- A **tick** contains effects that happen simultaneously from one pre-tick
  state.
- Sequentially dependent effects must occur in different ticks.
- Multiple independent effects may occur in the same tick.
- The world is **stable** when no fall, slide, explosion, chain reaction, door
  transition, or other derived action remains pending.

### Required resolution order

The engine should resolve a normal turn as follows:

1. Validate the command against a stable, nonterminal state.
2. Compute the complete player walk or atomic push from the pre-tick state.
3. If it is illegal, emit a blocked-action event and leave the world unchanged.
4. Otherwise, apply the player movement tick.
5. After every tick, recompute switches and effective door states, detect
   crushing, and identify newly unsupported entities and newly armed barrels.
6. Resolve all currently possible falls and ramp slides in derived ticks until
   the affected entities are settled.
7. Detonate all settled barrels ready for the same explosion wave from one
   shared pre-wave snapshot.
8. Apply that wave's blast movements simultaneously.
9. Return to step 5 and continue until no derived work remains.
10. Only then may the wrapper submit another player command.

An armed barrel does not explode while it still has a fall or available ramp
slide caused by the triggering interaction. This is why a blast can push a
barrel off a ledge and the barrel can explode only after landing.

### Example chain

If the player pushes a barrel off a ledge and its explosion reaches two more
barrels, the approximate ticks are:

1. The player and first barrel move horizontally.
2. The first barrel falls and becomes armed.
3. The first barrel explodes; the two neighboring barrels receive blast pushes
   and become armed.
4. The two barrels complete any resulting falls or ramp slides.
5. The two settled barrels explode simultaneously in the next wave.
6. Resolution continues until the state is stable. Only then can another
   player command be accepted.

## Engine output

The engine returns the state after every tick plus the events that occurred in
that tick. A useful logical result shape is:

```text
TurnResult
  command
  accepted
  initialState
  ticks[]
    index
    events[]
    stateAfter
  finalState
  outcome: ongoing | won | lost
```

Useful event kinds include:

- `MoveBlocked`
- `EntityMoved`
- `EntityPushed`
- `EntityFell`
- `EntitySlid`
- `BarrelArmed`
- `BarrelExploded`
- `SwitchChanged`
- `DoorOpened`
- `DoorClosed`
- `PlayerCrushed`
- `LevelWon`
- `LevelLost`

Movement events should include the entity ID, old and new coordinates, old and
new heights, and the cause: player, blast, fall, or slide.

### Determinism requirements

- Identical state plus identical command must always produce the same logical
  result.
- No rule depends on randomness.
- Every tick is computed from a pre-tick snapshot.
- Results must not depend on map, set, or array iteration order.
- Concurrent conflicts must be resolved by stated rules, never arbitrary
  entity ordering.
- State snapshots returned to callers should be immutable values.
- Events report what happened; callers should not need to replay events to
  discover the authoritative state.

## Level validation

At minimum, level validation should eventually check:

- Exactly one player exists.
- Every entity has a unique ID.
- No entity volumes overlap in the initial state.
- The player is not below another entity.
- Every stack is supported and contiguous.
- Fixtures obey their cell-placement restrictions.
- Every ramp has one valid low endpoint and one valid high endpoint on opposite
  sides.
- No ramp connects through a perpendicular edge.
- Door/switch color requirements are satisfied.
- The initial door state agrees with switch occupancy.

The level loader must either require an initially stable world or explicitly
stabilize it and return the resulting setup ticks. Requiring stable initial
states is the simpler initial implementation.

## Open design questions

These questions must be answered before implementing the affected feature.

1. **Terminal timing:** When the player wins, are already-scheduled falls and
   explosion waves canceled immediately, or does the active chain finish before
   the final outcome is emitted? In either case, no new player input is allowed.
2. **Blast effects on stacks:** If an adjacent-cell blast hits an entity that
   has something above or below it, does the whole stack move, does only a
   portion move, or is movement blocked? This is separate from player pushing
   and automatic ramp-stack sliding.
3. **Player support destroyed:** If a barrel supporting the player explodes,
   is that immediately a loss, or may the player remain at that coordinate if a
   new support surface is exactly at the same height? The recommended simple
   rule is immediate loss whenever the player involuntarily loses support.
4. **Board boundary:** Is the board rectangular or irregular, and is an absent
   neighboring cell a solid wall or a void that objects can fall into?
5. **Exit count:** Must a level contain exactly one exit or may it contain
   several equivalent exits?
6. **Exit occupancy:** Should boxes and barrels remain prohibited from exit
   cells, or should an exit behave like ordinary goal floor that only the player
   can activate?
7. **Color validation:** Must every used switch color have a door and vice
   versa, or is that merely a level-authoring warning?
8. **Initial instability:** Must every loaded level already be stable, or does
   loading resolve initial falls, slides, switches, and explosions?
9. **Invalid commands:** Does a blocked command count as a turn with no ticks,
   or does the engine return a dedicated rejected-command result outside the
   turn history?
10. **Multiple simultaneous falls:** What happens if independently falling
    stacks would require overlapping destination space during the same tick?

## Suggested implementation order

1. World schema, entity IDs, fixtures, stacks, and validation.
2. Flat-cell walking and single-entity player pushes.
3. Falling and crushing.
4. Switches, doors, exits, and terminal events.
5. Ramp traversal, automatic sliding, and whole-stack ramp movement.
6. Single explosions and height-aware blast targeting.
7. Simultaneous explosion waves and chain reactions.
8. Full tick snapshots, event output, and conflict tests.

Every phase should add behavior-level tests before implementation proceeds to
the next phase.
