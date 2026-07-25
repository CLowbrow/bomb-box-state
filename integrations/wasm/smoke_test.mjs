import assert from "node:assert/strict";
import fs from "node:fs/promises";
import path from "node:path";
import { pathToFileURL } from "node:url";

const modulePath = process.argv[2];
const contractScripts = process.argv.slice(3);
if (!modulePath || contractScripts.length === 0) {
  throw new Error("usage: node smoke_test.mjs <generated game_rules.mjs> <contract-script>...");
}

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

const readText = (base, name) => fs.readFile(path.resolve(base, name), "utf8");
const readJson = async (base, name) => JSON.parse(await readText(base, name));

async function runContract(scriptPath) {
  const base = path.dirname(scriptPath);
  const lines = (await fs.readFile(scriptPath, "utf8")).split(/\r?\n/u);
  const engine = api.createEngine();
  try {
    for (const [index, line] of lines.entries()) {
      if (line.length === 0 || line.startsWith("#")) {
        continue;
      }
      const fields = line.split("|");
      assert.ok(fields.length >= 2 && fields.length <= 3,
        `${scriptPath}:${index + 1} expected 2 or 3 pipe-separated fields`);

      let actual;
      switch (fields[0]) {
        case "load":
          assert.equal(fields.length, 3, `${scriptPath}:${index + 1} load requires an input`);
          actual = engine.loadLevel(await readText(base, fields[2]));
          break;
        case "get-state":
          assert.equal(fields.length, 2, `${scriptPath}:${index + 1} get-state takes no input`);
          actual = engine.getState();
          break;
        case "move":
          assert.equal(fields.length, 3, `${scriptPath}:${index + 1} move requires a direction`);
          actual = engine.move(fields[2]);
          break;
        case "rewind":
          assert.equal(fields.length, 2, `${scriptPath}:${index + 1} rewind takes no input`);
          actual = engine.rewind();
          break;
        default:
          assert.fail(`${scriptPath}:${index + 1} invalid contract operation ${fields[0]}`);
      }
      assert.deepEqual(actual, await readJson(base, fields[1]),
        `${scriptPath}:${index + 1} mismatch for ${fields[1]}`);
    }
  } finally {
    engine.destroy();
  }
  engine.destroy();
  assert.throws(() => engine.getState(), /game-rules engine has been destroyed/);
}

// Boundary-specific JavaScript ownership and validation behavior supplements the shared corpus.
const ownershipEngine = api.createEngine();
const browserContractBase = path.dirname(contractScripts[0]);
const browserLevel = JSON.parse(await readText(browserContractBase, "level.json"));
ownershipEngine.loadLevel(browserLevel);
assert.throws(() => ownershipEngine.move(99), {
  name: "RangeError",
  message: "direction must be north, east, south, or west",
});
ownershipEngine.destroy();
ownershipEngine.destroy();
assert.throws(() => ownershipEngine.move(api.directions.east),
  /game-rules engine has been destroyed/);

for (const script of contractScripts) {
  await runContract(path.resolve(script));
}

console.log(`WebAssembly passed ${contractScripts.length} authored contract scripts.`);
