/// <reference lib="webworker" />

export type MainToWorker = { type: "init" } | { type: "run"; source: string };

export type WorkerToMain =
  | { type: "ready" }
  | { type: "done"; result: RunResult }
  | { type: "error"; message: string };

export interface RunResult {
  ok: boolean;
  output: string;
  error: string;
}

type WasmModule = {
  varnRunChunk(source: string): RunResult | Promise<RunResult>;
};

type WasmFactory = (opts?: { locateFile?: (path: string) => string }) => Promise<WasmModule>;

let wasm: WasmModule | null = null;
let busy = false;

async function loadWasm(): Promise<WasmModule> {
  if (wasm) {
    return wasm;
  }
  const origin = self.location.origin;
  const moduleUrl = `${origin}/wasm/varn_wasm.js`;
  const imported = (await import(/* @vite-ignore */ moduleUrl)) as { default: WasmFactory };
  wasm = await imported.default({
    locateFile: (path: string) => `${origin}/wasm/${path}`,
  });
  return wasm;
}

self.onmessage = async (event: MessageEvent<MainToWorker>) => {
  const message = event.data;
  try {
    if (message.type === "init") {
      await loadWasm();
      const ready: WorkerToMain = { type: "ready" };
      self.postMessage(ready);
      return;
    }

    if (message.type === "run") {
      if (busy) {
        const err: WorkerToMain = { type: "error", message: "A run is already in progress." };
        self.postMessage(err);
        return;
      }

      busy = true;
      const mod = await loadWasm();
      const result = await Promise.resolve(mod.varnRunChunk(message.source));
      busy = false;
      const done: WorkerToMain = { type: "done", result };
      self.postMessage(done);
    }
  } catch (e) {
    busy = false;
    const text = e instanceof Error ? e.message : String(e);
    const err: WorkerToMain = { type: "error", message: text };
    self.postMessage(err);
  }
};
