// JavaScript ownership layer over the primitive C ABI. This file is appended inside the generated
// Emscripten module factory, where Module and the exported runtime methods are visible.
Module.gameRules = Object.freeze((() => {
  const directions = Object.freeze({
    north: "north",
    east: "east",
    south: "south",
    west: "west",
  });
  const directionValues = Object.freeze({ north: 0, east: 1, south: 2, west: 3 });

  function takeJson(pointer) {
    if (!pointer) {
      throw new Error("game-rules module could not allocate response memory");
    }
    try {
      return JSON.parse(Module.UTF8ToString(pointer));
    } finally {
      Module.ccall("game_rules_string_free", null, ["number"], [pointer]);
    }
  }

  function createEngine() {
    let handle = Module.ccall("game_rules_engine_create", "number", [], []);
    if (!handle) {
      throw new Error("game-rules module could not allocate an engine");
    }

    function liveHandle() {
      if (!handle) {
        throw new Error("game-rules engine has been destroyed");
      }
      return handle;
    }

    function loadLevel(level) {
      const engineHandle = liveHandle();
      const json = typeof level === "string" ? level : JSON.stringify(level);
      if (typeof json !== "string") {
        throw new TypeError("level must be a JSON string or JSON-serializable value");
      }
      const result = Module.ccall(
        "game_rules_engine_load_level",
        "number",
        ["number", "string", "number"],
        [engineHandle, json, Module.lengthBytesUTF8(json)],
      );
      return takeJson(result);
    }

    function getState() {
      const result = Module.ccall(
        "game_rules_engine_get_state",
        "number",
        ["number"],
        [liveHandle()],
      );
      return takeJson(result);
    }

    function move(direction) {
      const engineHandle = liveHandle();
      if (typeof direction !== "string"
          || !Object.prototype.hasOwnProperty.call(directionValues, direction)) {
        throw new RangeError("direction must be north, east, south, or west");
      }
      const result = Module.ccall(
        "game_rules_engine_move",
        "number",
        ["number", "number"],
        [engineHandle, directionValues[direction]],
      );
      return takeJson(result);
    }

    function rewind() {
      const result = Module.ccall(
        "game_rules_engine_rewind",
        "number",
        ["number"],
        [liveHandle()],
      );
      return takeJson(result);
    }

    function destroy() {
      if (!handle) {
        return;
      }
      const ownedHandle = handle;
      handle = 0;
      Module.ccall("game_rules_engine_destroy", null, ["number"], [ownedHandle]);
    }

    return Object.freeze({ loadLevel, getState, move, rewind, destroy });
  }

  return { directions, createEngine };
})());
