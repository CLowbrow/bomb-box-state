# Bomb box state/rules engine

This is a game engine that's meant to keep track of world state for a sokoban-like game. 

## The world

The world is composed of a grid that can be navigated by the player. The grid is made of squares. Each square can have an elevation that is represented by an integer. A square can also be a ramp, meaning that it can connect two other squares that have an elevation that's different by one. The two squares have to be on opposite sides of the ramp. a ramp can only connect two squares. 

If a ramp has an arrangement like 

  1
1 R 2
  2 
  
It can only connect one 1 block with a 2 block across from it. 

A square can have an object on it. The list of objects is:

- The player
- A box
- An exploding barrel
- A switch
- A door
- THE exit

## How objects behave

- Doors can open or close but can't move. 
- A box and an exploding barrel can be pushed off a ledge
- A player can't walk off a ledge
- A ledge is just a block with a higher elevation next to a block with a lower elevation. The different can be any number. 
- A box is not hurt by falling off a ledge
- When an exploding barrel falls off a ledge it explodes
- When an exploding barrel explodes here is how it interacts with the world:
  - For all the interactions above: the bomb only interacts with objects on the same level or objects that are on a ramp that is connecting the square the bomb is on to another level. 
  - Boxes are pushed one space away from the barrel if possible but not destroyed. 
  - Doors and switches are unaffected
  - Other exploding barrels are pushed away one space (if possible) and then explode, continuing the cycle.
  - The player being next to an exploding bomb (adjacent square) when the bomb goes off ends causes the level to become lost. 
- Switches open doors
  - A switch always has a basic color (red, green, blue, yellow)
  - Doors also have corresponding colors
  - You can't have a door of a color without a corresponding switch of that color and vice versa
  - the relationship is N:N
  - When all of the switches of [color] have a box, a barrel, or the player on top of them, all doors of that color open. 
- Switches can share a flat square (not a ramp) with any other object (box, barrel, player) but not with doors
- No other objects can share a square
- Door behavior
  - Doors effectively make a square impassible to anything
  - When doors open, for the purpose of other objects moving around, they don't exist. 
  - If there is an object under a door and something is moved from a switch controlling that door, the door is effectively still open until that object is pushed out from under it at which point it will close. 
- Objects are a height of 1. 
  - If a bomb is on a square of height 2 and there is a box next to it on a square of height 1, then it can be pushed on top of the box.
  - if the bomb is pushed off a ledge height 3 onto a height 1 square with one box on it, it will still drop 1, causing it to explode.
- Exits are special
  - The player is the only other kind of thing that can move onto a square that has an exit on it
  - The player stepping on the square that has an exit wins the level
  - Nothing else can be pushed onto an exit square by any means
  - Exits can't be on ramps
- Ramps
  - A box or exploding barrel will try to slide down a ramp if able, but won't if there is something blocking them from moving off the ramp. 
  - Sliding down a ramp does not explode barrels

## How the player interacts with the world

- The player (person) can send (through a wrapper ui commands to this game engine) to move in a cardinal direction. 
- If there is an object in the way that can be moved (bomb, box) the player will push it in that direction when someone inputs that direction. 
  - If there is a box above the player and it can move one space up, pressing up will push both the player "pawn" and the object up at the same time.
- Players can push boxes and barrels down ramps but not up.
- Because bombs can chain react and push more and more stuff it's possible for an object to eventually drop off of a ledge onto a player. This is a level failed condition. 

## How the engine processes stuff

- The engine works on turns.
- The input is one of the cardinal directions
- Then the engine processes what that action would do, then the results of that action and so forth until there are no more actions in the queue and then it's the players turn again. 
- The engine output is the world state after every "tick" of the world happening, as well as a list of things that happened during that turn
  - Assume the player pushes a bomb off a ledge and it blows up two more bombs when it falls. The events and game state would be roughly: player and bomb move to next locations, bomb falls, bomb explodes, both bombs next to first bomb move 1 square away from the blast, both bombs explode.
  - It's fine if more than one thing moves on a tick but a tick should only contain things that happen "at the same time", not sequentially
- Engine can also emit "win" or "lose" events.
