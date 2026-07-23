# Bomb Box state/rules engine

Bomb Box is a deterministic state and rules engine for a turn-based,
Sokoban-like puzzle game. The engine owns the authoritative world state and an
undo-only history of resolved states for the currently loaded level. A UI may
submit a cardinal movement command or rewind to the preceding resolved state.
For a movement command, it receives the complete, tick-by-tick result,
including every fall, slide, explosion, door change, and terminal event caused
by it.

This document is the normative behavior specification for the engine. Any
future unresolved behavior must be called out explicitly rather than left for
an implementation to decide implicitly.

## Normative language

The words **must**, **must not**, **may**, and **should** describe engine
requirements. Examples illustrate the requirements but do not replace them.

## Terminology and world model

### Coordinates and directions

By default, the world is a finite rectangular grid with a cell at every
in-bounds coordinate. A movement command contains exactly one of the four
cardinal directions: north, east, south, or west. Diagonal movement and
diagonal blast effects do not exist.

The edge of the rectangle is a solid world boundary. Players, boxes, barrels,
stacks, falls, slides, and blast pushes cannot enter a coordinate outside it.
There are no void or abyss cells in the initial rules. A future version may add
explicit void cells and rules for falling out of the world; implementations
should avoid making that extension unnecessarily difficult.

The level format must define its coordinate origin, axis directions, width, and
height. Coordinates cover the rectangle from the declared origin through
`origin + (width - 1, height - 1)`. The positive x-axis must be declared as east
or west, and the positive y-axis as north or south. Width and height must both
be positive, and the complete coordinate range must be representable by the
fixed-width coordinate type.

### Cells, fixtures, and entities

The engine distinguishes three kinds of world data:

- A **cell** supplies geometry: a coordinate and either a flat surface or a
  ramp.
- A **fixture** is attached to a cell: a switch, door, or exit teleporter.
  Fixtures are not members of entity stacks.
- An **entity** occupies physical space: the player, a box, or an explosive
  barrel. Every entity must have a stable unique ID.

This distinction permits an entity to occupy the same cell as a switch or an
open door without treating the fixture as part of the entity stack.

A cell may have at most one fixture. This keeps fixture occupancy unambiguous;
multiple switches or multiple doors cannot be layered in one cell.

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
  Switches and open doors do not affect whether an entity is unstacked because
  they are fixtures, not entities. Teleporter occupancy matters only for the
  player and immediately ends the level.

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

## Player controls and resolved-state history

The gameplay command set is exactly:

- `Move(direction)`, where `direction` is north, east, south, or west; or
- `Rewind`, which restores the previous resolved state.

Loading a level is a separate engine lifecycle operation, not a gameplay
command.

A **resolved state** is the authoritative command-boundary snapshot produced
after level initialization or after an accepted movement command has finished
all of its derived work. It is either stable and ongoing or terminal. Per-tick
snapshots are output for animation and diagnosis, but they are not rewind
boundaries.

The engine must maintain the current resolved state and a stack of earlier
resolved states for only the currently loaded level:

- The stabilized result of level initialization is the beginning of the
  history. There is no state before it to rewind to.
- When a movement command is accepted, the engine preserves the exact resolved
  state from before that turn. After the turn resolves, its final state becomes
  current.
- A rejected movement leaves both the current state and history unchanged.
- An accepted `Rewind` restores the most recent earlier resolved state and
  removes it from the history stack. It does not replay events, run rules,
  perform initialization, or create a tick or turn.
- Rewind may be repeated until the initialized state is current. A further
  rewind is rejected as `history_empty` and changes nothing.
- Rewind is allowed from a won or lost state. This is the only gameplay command
  accepted while the current state is terminal.
- There is no redo operation or redo storage. Once a state is rewound, the
  abandoned later state is permanently discarded by the engine. A subsequent
  movement therefore starts a new one-way history from the restored state.
- State history is an engine concern. Callers must not need to retain or replay
  tick events to implement rewind.

## Level loading and lifetime

The engine must expose an explicit operation for loading a level. Loading is
available when there is no level, while a level is ongoing, and after a win or
loss; it does not require the current level to reach a particular gameplay
state.

- A successfully loaded level completely replaces the preceding level. The
  engine must clear its current state, resolved-state history, pending derived
  work, terminal outcome, and any other level-scoped data before establishing
  the new level's initialized state and history.
- No resolved state from a preceding level may be reached by rewind, affect
  determinism, or receive events after the replacement.
- Structural validation should occur before committing the replacement. If a
  load is rejected as invalid, the currently loaded level remains unchanged.
- If an integration permits a load request to overlap active initialization or
  turn resolution, the accepted load supersedes that work. Results from the
  superseded level must be discarded and must not overwrite or emit events into
  the new level. A synchronous integration may instead serialize the call while
  preserving the same observable behavior.
- Immutable snapshots already returned to a caller may remain in caller-owned
  memory, but the engine must retain no old level state after a successful
  replacement.

The newly loaded level then follows the initialization and stabilization rules
below. Its stabilized or terminal initialization result is the first and only
resolved state in a fresh history.

## Movement rules

### Input gating

- The engine accepts at most one gameplay command at a time.
- An accepted movement command starts a **turn**. A rejected command and an
  accepted rewind do not.
- After applying the player's action, the engine must resolve the complete
  causal chain to a resolved state before accepting another gameplay command.
- While falls, slides, explosions, chain reactions, or other derived actions
  remain pending, additional player input must be rejected or left outside the
  engine by the wrapper. It must never be inserted into the active turn.
- Consequently, the player cannot race an explosion chain, move between two
  explosion waves, or act while another entity is still falling or sliding.

### Walking

- The player may move only one cell in a cardinal direction per command.
- The player cannot climb a ledge.
- The player cannot voluntarily walk down a ledge.
- The player changes elevation only by traversing a ramp.
- The player may walk onto the top of a box when that box's top is at the same
  height as the player's current supporting surface.
- A closed door and an ineligible teleporter cell block walking.

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
- The player cannot voluntarily fall.
- An unsupported entity or stack falls to the highest legal support surface in
  its current cell.
- A falling stack retains its bottom-to-top order and falls as one unit.
- A fall is resolved in a derived tick after the movement or removal that
  caused the loss of support.
- A box survives a fall of any distance.
- A barrel that falls any positive distance becomes armed and must explode
  after its forced movement has finished.
- An involuntary player fall of less than `1.0` is survivable.
- If the player ever falls `1.0` or more in a single fall, the level is lost.
- If a barrel directly supporting the player explodes, the level is lost
  immediately. The player does not first fall into the space left by the
  barrel.
- If a falling entity or stack would land on the player, the level is lost.
- A fall event should record the complete start height, end height, and fall
  distance rather than emitting one tick per unit of height.

### Falling-column resolution

Falling changes only vertical height; it never changes an entity's grid
coordinate. Gravity is resolved independently for each cell column.

- Entities and separated groups in one column retain their pre-tick
  bottom-to-top order.
- The engine compacts every unsupported group downward onto the cell's support
  surface and onto the groups below it.
- Landing positions are calculated bottom-up. The lowest unsupported group
  lands first logically, and each higher group lands on the final position of
  the group below it.
- All of those calculated falls may occur in the same tick because they come
  from one pre-tick column and have one deterministic final arrangement.
- Fall distance and barrel arming are calculated separately for every falling
  entity or group.
- In a ramp cell, unsupported groups compact onto the ramp surface before the
  resulting whole stack is eligible to slide in a later derived tick.
- Falling groups in different cells cannot collide because falling has no
  horizontal component.
- If horizontal blast movements or ramp slides would send two groups into
  overlapping destination space, that conflict is resolved before falling.
  The conflicting horizontal movements fail, so gravity never receives two
  columns competing for the same landing volume.

For example:

```text
before                 after one falling tick
z=4  Box A             z=1  Box A
z=3  empty             z=0  Box B
z=2  Box B                  floor
z=1  empty
z=0  floor
```

Box B falls to the floor and Box A lands on Box B. Their order cannot reverse,
and no arbitrary entity or event-queue ordering is needed.

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

Switches, doors, and teleporters cannot be placed on ramp cells.

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
- Cell elevation matters. Across two flat cells, the blast acts at the
  exploding barrel's bottom height and can select only the entity whose bottom
  height is the same. Merely touching at a top/bottom boundary does not count.
- For blast targeting, a ramp occupant is considered connected to the low
  endpoint at height `X` and the high endpoint at height `X + 1`. A blast may
  cross either oriented ramp edge at that endpoint's height. Perpendicular ramp
  edges do not carry a blast between levels.
- Doors, switches, teleporters, and cell geometry are not damaged or moved by a
  blast.
- The exploding barrel is removed from the world as part of its explosion.

### Adjacent-cell effects

- In an adjacent stack, the blast selects only the entity at the blast height.
  Entities above and below it are not directly pushed by that blast.
- If the selected entity is a box or barrel and its movement is legal, it is
  popped horizontally out of the stack and moved at most one cell directly
  away from the exploding barrel. A box is not destroyed.
- A pop destination is legal only if it is inside the board, its fixtures allow
  the selected entity to enter, it contains no entity volume at the selected
  entity's arrival height, and its support surface is not higher than that
  arrival height. A lower support surface causes a subsequent fall.
- The portion of the original stack below the popped entity stays in place.
- Every entity above the popped entity becomes unsupported and falls together
  during the following derived tick. It does not move horizontally with the
  popped entity.
- The popped entity may itself fall or enter a ramp after moving into its
  destination. It completes that forced movement normally.
- If the pop movement is blocked or canceled, the selected entity remains in
  the stack and the entities above it do not fall.
- A selected barrel becomes armed whether or not its pop movement succeeds.
- An affected player causes a loss condition.

A blast pop is not a player push. It may remove an entity from the middle of a
stack even though the player may push only unstacked entities.

For example, suppose a barrel exploding at height `1` is west of this stack:

```text
height 2:  Box B
height 1:  Box A  <- selected by the blast
height 0:  Box L
```

If the cell east of the stack is a legal destination, Box A pops east. Box L
stays at height `0`, and Box B falls to height `1` during the following derived
tick. If Box B were the player, that `1.0` fall would cause a loss.

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
- A directly touching player causes a loss condition. In particular, a player
  standing on the exploding barrel loses immediately.
- A box in the source cell receives no horizontal impulse because there is no
  unique away direction. A box above the source may nevertheless fall when the
  source disappears.

### Simultaneous blast impulses

All explosions in the same explosion wave inspect the same pre-wave state.

- One or more impulses from the same direction produce at most one one-cell
  movement.
- If an entity receives impulses from two or more different directions, all of
  its horizontal blast movement is canceled.
- Direction conflicts are determined before checking whether any one of the
  candidate destinations is blocked. No direction gets priority merely because
  another direction would have failed.
- Counts do not matter. For example, two eastward impulses and one westward
  impulse still conflict and result in no movement.
- A barrel hit by any blast becomes armed even when its movement is canceled.
- If two otherwise valid movements require overlapping destination space, the
  conflicting movements fail.
- Barrels already exploding in the current wave cannot be moved by that wave.
- Entity ID or collection iteration order must never decide a physics result.

For example, suppose one barrel is north of a box and another is east of it:

```text
             north barrel
                  |
                  | south impulse
                  v
                [box] <--- west impulse --- east barrel
```

If both barrels explode in the same wave, the box receives one south impulse
and one west impulse. Because those directions differ, both impulses are
consumed and the box remains in place. Neither explosion is "first," and the
box does not retain either impulse for a later tick. If the target were another
barrel, it would remain in place, become armed, and explode after settling.

## Switches and doors

### Switches

- Every switch has a color. The initial palette is red, green, blue, and yellow.
- A switch can be placed only on a flat cell.
- A switch cannot share a cell with a door.
- A switch is pressed when an entity is resting directly on the switch's floor
  surface. An entity higher in the stack does not itself press the switch.
- A box, barrel, or player may press a switch.
- All switches of a color use AND behavior: that color is active only when at
  least one switch of that color exists and every switch of that color is
  pressed.
- All doors of an active color are open.
- A switch color with no corresponding doors is legal and simply controls
  nothing.

### Doors

- Every door has a switch color and cannot move.
- A door color with no corresponding switches is legal. Because a color with
  zero switches is inactive, such a door remains closed unless it is being held
  open by an entity already in its cell during initialization.
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

Level validation must not require switch colors and door colors to correspond.

## Exit teleporters and terminal outcomes

- A level may contain any number of exit teleporters, including zero. Every
  teleporter has identical behavior and reaching any one of them wins the
  level.
- A teleporter can be placed only on a flat cell and cannot share its cell with
  another fixture.
- A teleporter reserves its entire vertical column to effectively infinite
  height. No box, barrel, or stack may enter, pass over, land in, or be placed
  in that cell at any height.
- The player may enter a teleporter cell only directly at the teleporter floor
  height and only if the player will be the cell's sole entity. Nothing may be
  above or below the player there; the player's stack status in the cell it is
  leaving is irrelevant.
- The player wins as soon as it directly touches the teleporter floor.
- A state containing a player stacked above another entity in a teleporter cell
  is invalid rather than a non-winning way to occupy the teleporter.
- If win and loss conditions occur during the same tick, **win takes
  precedence**.
- The tick in which the player touches the teleporter completes as one atomic
  tick. The engine emits `LevelWon` and then immediately makes the level
  terminal.
- All falls, slides, explosions, door changes, and other derived ticks that
  would have happened afterward are canceled. They produce no events or state
  snapshots.
- After the level becomes terminal, the engine rejects movement commands but
  still accepts rewind when history is available. Loading another level also
  remains available.

## Level initialization and stabilization

A loaded level is allowed to be physically unstable. Boxes and barrels may
begin unsupported, ramp occupants may be ready to slide, switches may initially
change door states, and barrels may begin in positions that cause them to fall
and explode.

After structural validation, the engine must:

1. Construct the supplied initial state.
2. Check whether the player already touches a teleporter. If so, win
   immediately and stop initialization.
3. Derive initial switch and door states.
4. Run the same falling, sliding, explosion, door, loss, and conflict rules used
   for a normal turn until the world becomes stable or terminal.
5. Return every initialization tick and its events to the caller.
6. Accept the first movement command only if initialization finishes in a
   stable, nonterminal state. A rewind has no earlier state and is rejected as
   `history_empty`.

Initialization is not a player turn and has no gameplay command. A useful API
may return a `LoadResult` containing the supplied state, initialization ticks,
resolved state, and outcome. This allows a UI to animate a level settling into
place before control is given to the player. A successful `LoadResult` begins a
new history even when initialization immediately produces a terminal state.

## Turns, ticks, and causal resolution

### Definitions

- A **turn** begins with one accepted movement command and includes every
  consequence of that command until the world becomes stable or terminal
  handling stops it. Rewind and level loading are not turns.
- A **tick** contains effects that happen simultaneously from one pre-tick
  state.
- Sequentially dependent effects must occur in different ticks.
- Multiple independent effects may occur in the same tick.
- The world is **stable** when no fall, slide, explosion, chain reaction, door
  transition, or other derived action remains pending.

### Required resolution order

The engine should resolve a normal turn as follows:

1. Validate the movement command against a stable, nonterminal state.
2. Compute the complete player walk or atomic push from the pre-tick state.
3. If it is illegal, return a rejected-command result with a `MoveBlocked`
   presentation event and leave the world unchanged. No turn or tick begins.
4. Otherwise, apply the player movement tick.
5. After every tick, first check whether the player directly touched a
   teleporter. If so, emit `LevelWon`, discard all future derived work, and end
   the turn. Otherwise, recompute switches and effective door states, detect
   crushing, and identify newly unsupported entities and newly armed barrels.
6. Resolve all currently possible falls and ramp slides in derived ticks until
   the affected entities are settled.
7. Detonate all settled barrels ready for the same explosion wave from one
   shared pre-wave snapshot.
8. Apply that wave's blast movements simultaneously.
9. Return to step 5 and continue until no derived work remains.
10. Only then may the wrapper submit another gameplay command.

An armed barrel does not explode while it still has a fall or available ramp
slide caused by the triggering interaction. This is why a blast can push a
barrel off a ledge and the barrel can explode only after landing.

### Rejected movement commands

A movement command that cannot produce a legal player action is rejected.
Rejection is observable so a UI can play a blocked-movement animation, sound,
or other feedback.

- A rejected movement command does not start a turn and produces no world tick.
- The authoritative world state is unchanged.
- The result includes the attempted direction and a stable reason code, such as
  `world_boundary`, `ledge`, `closed_door`, `teleporter_restriction`,
  `occupied`, `stacked_push_target`, `engine_busy`, or `level_terminal`.
- It includes a `MoveBlocked` event outside the tick list for presentation
  hooks.
- A movement command submitted while initialization or another turn is
  resolving is rejected as `engine_busy`; it is never queued into the active
  resolution. Level loading follows its separate supersession rules.
- A movement command submitted after win or loss is rejected as
  `level_terminal`. Rewind remains eligible.
- A malformed movement direction is an API validation error rather than a
  gameplay rejection.

### Rewind commands

Rewind is resolved only at command boundaries, so it never interrupts a turn
or selects an intermediate tick snapshot.

- If earlier history exists, rewind succeeds and returns the restored current
  state. The discarded later state is not retained for redo.
- If no earlier history exists, rewind is rejected as `history_empty`.
- Rewind produces no world tick. A result may include a `StateRewound`
  presentation event outside the tick list so a UI can animate or announce the
  jump.
- A rewind rejected as `engine_busy` or `history_empty` leaves the current
  state and history unchanged.

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
   gameplay command be accepted.

## Engine output

The engine returns a rejected-command result, a rewind result, or the state
after every tick plus the events that occurred in an accepted turn. Level
loading has its separate `LoadResult`. A useful logical gameplay result shape
is:

```text
GameplayCommand = Move(direction) | Rewind
CommandResult = RejectedCommand | RewindResult | TurnResult

RejectedCommand
  command
  accepted: false
  reason
  events[]
  state

RewindResult
  command: Rewind
  accepted: true
  events: [StateRewound]
  state
  outcome: ongoing | won | lost

TurnResult
  command: Move(direction)
  accepted: true
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
- `StateRewound`
- `EntityMoved`
- `EntityPushed`
- `EntityPoppedFromStack`
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
- Rewinding restores the exact earlier resolved state; resolving the same
  movement command from that state must produce the same result as before.
- Loading identical level data into a fresh engine or over an existing level
  must produce identical initialization output and resolved state. Old level
  history must have no effect.

## Level validation

At minimum, level validation should eventually check:

- Exactly one player exists.
- Every entity has a unique ID.
- Every cell coordinate lies within the declared rectangular width and height,
  and every in-bounds coordinate has a cell.
- No entity volumes overlap in the initial state.
- No box, barrel, stack, or non-teleporter fixture occupies a teleporter cell.
- Entities at one coordinate have a deterministic bottom-to-top order. They may
  initially contain unsupported gaps, which initialization will resolve.
- Fixtures obey their cell-placement restrictions.
- Every ramp has one valid low endpoint and one valid high endpoint on opposite
  sides.
- No ramp connects through a perpendicular edge.
- Every door and switch has a syntactically valid color identifier; matching
  colors are not required.

Initial door state is derived from switch occupancy during initialization and
must not be rejected merely because a serialized door state disagrees with it.

## Suggested implementation order

1. World schema, entity IDs, fixtures, stacks, validation, and replaceable
   level lifetime.
2. Resolved-state history, undo-only rewind, and history reset on level load.
3. Flat-cell walking and single-entity player pushes.
4. Falling and crushing.
5. Switches, doors, teleporters, and terminal events.
6. Ramp traversal, automatic sliding, and whole-stack ramp movement.
7. Single explosions and height-aware blast targeting.
8. Simultaneous explosion waves and chain reactions.
9. Full tick snapshots, event output, rewind/load lifecycle tests, and conflict
   tests.

Every phase should add behavior-level tests before implementation proceeds to
the next phase.

## Project scaffold

The implementation scaffold uses dependency-free C++20 with CMake, CTest, native and WebAssembly
presets, a primitive C ABI, and a future Unreal plugin wrapper. The engine is intentionally a stub.

- [Development setup](docs/development.md)
- [Architecture and portability constraints](docs/architecture.md)
- [Unreal integration scaffold](integrations/unreal/README.md)
