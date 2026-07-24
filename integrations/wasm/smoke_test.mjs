import assert from "node:assert/strict";
import fs from "node:fs/promises";
import path from "node:path";
import { pathToFileURL } from "node:url";

const modulePath = process.argv[2];
const contractDirectory = process.argv[3];
if (!modulePath || !contractDirectory) {
  throw new Error("usage: node smoke_test.mjs <generated game_rules.mjs> <contract-directory>");
}

const readJson = async (name) => JSON.parse(
  await fs.readFile(path.join(contractDirectory, name), "utf8"),
);
const { default: createGameRulesModule } = await import(pathToFileURL(path.resolve(modulePath)));
const module = await createGameRulesModule();
const api = module.gameRules;

assert.equal(module.ccall("game_rules_api_version", "number", [], []), 1);
assert.deepEqual(api.directions, {
  north: "north",
  east: "east",
  south: "south",
  west: "west",
});

const level = await readJson("level.json");
const engine = api.createEngine();
try {
  assert.deepEqual(engine.loadLevel(level), await readJson("load.expected.json"));
  const initialState = await readJson("initial-state.expected.json");
  assert.deepEqual(engine.getState(), initialState);

  const rejectedLoad = engine.loadLevel("{");
  assert.equal(rejectedLoad.status, "invalid_json");
  assert.deepEqual(rejectedLoad.state, initialState.state);
  assert.throws(() => engine.move(99), {
    name: "RangeError",
    message: "direction must be north, east, south, or west",
  });
  const rejectedMove = engine.move(api.directions.west);
  assert.equal(rejectedMove.status, "world_boundary");
  assert.deepEqual(rejectedMove.state, initialState.state);

  assert.deepEqual(engine.move(api.directions.east), await readJson("move-east.expected.json"));
  assert.deepEqual(engine.rewind(), await readJson("rewind.expected.json"));
  assert.deepEqual(engine.getState(), await readJson("rewound-state.expected.json"));
} finally {
  engine.destroy();
}
engine.destroy();
assert.throws(() => engine.getState(), /game-rules engine has been destroyed/);
assert.throws(() => engine.move(api.directions.east), /game-rules engine has been destroyed/);

console.log("WebAssembly browser vertical-slice contract passed.");
