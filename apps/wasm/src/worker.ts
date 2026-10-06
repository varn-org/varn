/// <reference lib="webworker" />

export type MainToWorker =
  | { type: "init" }
  | { type: "run"; source: string }
  | { type: "cameraFrame"; frame: ImageBitmap }
  | { type: "cameraError"; message: string };

export type WorkerToMain =
  | { type: "ready" }
  | { type: "output"; text: string }
  | { type: "done"; result: RunResult }
  | { type: "error"; message: string }
  | { type: "cameraStart"; width: number; height: number }
  | { type: "cameraTaken" }
  | { type: "cameraShow"; frame: ImageBitmap };

export interface RunResult {
  ok: boolean;
  output: string;
  error: string;
}

type DataViewToken = { $view: number; pointer: string; size: number };

interface VarnDataView {
  readonly size: number;
  bytes(): Uint8Array;
  token(): DataViewToken;
  release(): void;
}

type WasmModule = {
  varnRunChunk(source: string, options?: { onPrint?: (line: string) => void }): Promise<RunResult>;
  varnRegister(name: string, callback: (json: string) => string): void;
  VarnDataView: {
    allocate(size: number): VarnDataView;
    acquire(token: DataViewToken): VarnDataView | null;
  };
};

type WasmFactory = (opts?: { locateFile?: (path: string) => string }) => Promise<WasmModule>;

// The camera of a run: the frame Lua and the page share as one data view, the canvas that scales a camera frame into it and the canvas that turns the bytes Lua changed into a picture.
type Camera = {
  width: number;
  height: number;
  frame: VarnDataView;
  input: OffscreenCanvasRenderingContext2D;
  output: OffscreenCanvas;
  outputContext: OffscreenCanvasRenderingContext2D;
  fresh: boolean;
  error: string | null;
};

const CAMERA_MIN_SIZE = 16;
const CAMERA_MAX_SIZE = 1280;

let wasm: WasmModule | null = null;
let busy = false;
let camera: Camera | null = null;

function send(message: WorkerToMain, transfer: Transferable[] = []): void {
  self.postMessage(message, transfer);
}

function answer(value: unknown): string {
  return JSON.stringify(value ?? null);
}

function closeCamera(): void {
  camera?.frame.release();
  camera = null;
}

function context2d(canvas: OffscreenCanvas, willReadFrequently: boolean): OffscreenCanvasRenderingContext2D {
  const context = canvas.getContext("2d", { willReadFrequently });
  if (!context) {
    throw new Error("The worker could not create a 2D canvas.");
  }
  return context;
}

// The functions the page offers Lua under the global table `host`, each one taking and answering JSON the way a native host does.
function registerHostFunctions(mod: WasmModule): void {
  mod.varnRegister("pageInfo", () =>
    answer({
      language: navigator.language,
      languages: navigator.languages,
      online: navigator.onLine,
      cores: navigator.hardwareConcurrency,
      timeZone: Intl.DateTimeFormat().resolvedOptions().timeZone,
      userAgent: navigator.userAgent,
      now: Date.now(),
    }),
  );

  mod.varnRegister("cameraStart", (json) => {
    const options = JSON.parse(json) as { width?: unknown; height?: unknown } | null;
    const width = options?.width;
    const height = options?.height;
    const valid = (size: unknown): size is number =>
      Number.isInteger(size) && (size as number) >= CAMERA_MIN_SIZE && (size as number) <= CAMERA_MAX_SIZE;

    if (!valid(width) || !valid(height)) {
      return answer({ error: `The camera takes a width and a height from ${CAMERA_MIN_SIZE} to ${CAMERA_MAX_SIZE} pixels.` });
    }

    closeCamera();
    const output = new OffscreenCanvas(width, height);
    camera = {
      width,
      height,
      frame: mod.VarnDataView.allocate(width * height * 4),
      input: context2d(new OffscreenCanvas(width, height), true),
      output,
      outputContext: context2d(output, false),
      fresh: false,
      error: null,
    };
    send({ type: "cameraStart", width, height });
    return answer({});
  });

  // Answers the newest frame once, as a data view of RGBA bytes, so Lua never converts the same frame twice.
  mod.varnRegister("cameraFrame", () => {
    if (!camera) {
      return answer({ error: 'The camera is not open. Call "host.cameraStart" first.' });
    }

    if (camera.error) {
      return answer({ error: camera.error });
    }

    if (!camera.fresh) {
      return answer({});
    }

    camera.fresh = false;
    return answer({ frame: camera.frame.token(), width: camera.width, height: camera.height });
  });

  // Draws the bytes of a view in place, with no copy between Lua and the canvas that turns them into a picture.
  mod.varnRegister("cameraShow", (json) => {
    if (!camera) {
      return answer({ error: 'The camera is not open. Call "host.cameraStart" first.' });
    }

    const options = JSON.parse(json) as { frame?: DataViewToken } | null;
    const view = options?.frame ? mod.VarnDataView.acquire(options.frame) : null;
    if (!view) {
      return answer({ error: "The camera shows a frame given as a data view." });
    }

    try {
      if (view.size !== camera.width * camera.height * 4) {
        return answer({ error: `The frame must hold ${camera.width} by ${camera.height} pixels of 4 bytes.` });
      }

      const bytes = view.bytes();
      const pixels = new Uint8ClampedArray(bytes.buffer as ArrayBuffer, bytes.byteOffset, bytes.length);
      camera.outputContext.putImageData(new ImageData(pixels, camera.width, camera.height), 0, 0);
      const picture = camera.output.transferToImageBitmap();
      send({ type: "cameraShow", frame: picture }, [picture]);
      return answer({});
    } finally {
      view.release();
    }
  });
}

// The page sends a camera frame already scaled to the size Lua asked for, and it lands in the shared view between two slices of Lua.
function takeCameraFrame(frame: ImageBitmap): void {
  if (!camera || frame.width !== camera.width || frame.height !== camera.height) {
    frame.close();
    send({ type: "cameraTaken" });
    return;
  }

  camera.input.drawImage(frame, 0, 0);
  frame.close();
  camera.frame.bytes().set(camera.input.getImageData(0, 0, camera.width, camera.height).data);
  camera.fresh = true;
  send({ type: "cameraTaken" });
}

async function loadWasm(): Promise<WasmModule> {
  if (wasm) {
    return wasm;
  }
  const origin = self.location.origin;
  const moduleUrl = `${origin}/wasm/varn-wasm.js`;
  const imported = (await import(/* @vite-ignore */ moduleUrl)) as { default: WasmFactory };
  wasm = await imported.default({
    locateFile: (path: string) => `${origin}/wasm/${path}`,
  });
  registerHostFunctions(wasm);
  return wasm;
}

self.onmessage = async (event: MessageEvent<MainToWorker>) => {
  const message = event.data;

  if (message.type === "cameraFrame") {
    takeCameraFrame(message.frame);
    return;
  }

  if (message.type === "cameraError") {
    if (camera) {
      camera.error = message.message;
    }
    return;
  }

  try {
    if (message.type === "init") {
      await loadWasm();
      send({ type: "ready" });
      return;
    }

    if (message.type === "run") {
      if (busy) {
        send({ type: "error", message: "A run is already in progress." });
        return;
      }

      busy = true;
      const mod = await loadWasm();
      // Each line reaches the page as Lua prints it, even while a loop that never yields holds this worker.
      const result = await mod.varnRunChunk(message.source, {
        onPrint: (line) => send({ type: "output", text: line }),
      });
      busy = false;
      closeCamera();
      send({ type: "done", result });
    }
  } catch (e) {
    busy = false;
    closeCamera();
    const text = e instanceof Error ? e.message : String(e);
    send({ type: "error", message: text });
  }
};
