import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const appRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const wasmDir = process.env.VARN_WASM_DIR
  ? path.resolve(process.env.VARN_WASM_DIR)
  : path.resolve(appRoot, "..", "..", "build", "wasm", "bin");
const outDir = path.join(appRoot, "public", "wasm");

fs.mkdirSync(outDir, { recursive: true });

const files = ["varn_wasm.js", "varn_wasm.wasm"];
for (const name of files) {
  const src = path.join(wasmDir, name);
  if (!fs.existsSync(src)) {
    console.error(`Expected "${src}". From the repository root, build the wasm binary first, for example with:
  python3 varn.py wasm
Then set "VARN_WASM_DIR" (or rely on the default "../../build/wasm/bin" relative to this app). For the full UI build, use the command "python3 varn.py app-wasm".`);
    process.exit(1);
  }
  fs.copyFileSync(src, path.join(outDir, name));
}

console.log(`Copied the wasm bundle from "${wasmDir}" to "${outDir}".`);
