import path from "node:path";
import { pathToFileURL } from "node:url";

const modulePath = process.argv[2];
if (!modulePath) {
  throw new Error("usage: node smoke_test.mjs <generated bomb_box.mjs>");
}

const { default: createBombBoxModule } = await import(pathToFileURL(path.resolve(modulePath)));
const module = await createBombBoxModule();

const apiVersion = module.ccall("bomb_box_api_version", "number", [], []);
const engineStatus = module.ccall("bomb_box_engine_status", "string", [], []);

if (apiVersion !== 1 || engineStatus !== "schema_ready") {
  throw new Error(`unexpected Wasm API: version=${apiVersion}, status=${engineStatus}`);
}

console.log("WebAssembly scaffold check passed.");

