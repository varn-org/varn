import type { MainToWorker, RunResult, WorkerToMain } from "./worker";
import { mountCameraPanel } from "./camera";
import { el } from "./dom";
import { DEFAULT_LUA, EXAMPLES } from "./examples";

export function mountPlayground(root: HTMLElement): () => void {
  const shell = el("div", "mx-auto flex max-w-5xl flex-col gap-6 px-4 py-8 md:px-8");

  const intro = el("div", "space-y-1");
  intro.appendChild(el("h1", "text-2xl font-semibold tracking-tight text-white", "Playground"));
  intro.appendChild(
    el(
      "p",
      "max-w-2xl text-sm leading-relaxed text-zinc-400",
      "Lua runs in a Web Worker, so Stop ends even a loop that never yields. Pick an example, grouped by the module it exercises, to try what is available in the browser build. Output is captured from \"print()\". The modules \"socket\", \"process\" and \"ffi\" and the HTTP server need a native build, and calling them here raises a clear error.",
    ),
  );

  const grid = el("div", "grid gap-6 lg:grid-cols-[minmax(0,1fr)_minmax(0,1fr)]");
  const editorCard = el(
    "section",
    "flex flex-col gap-3 rounded-2xl border border-zinc-800/80 bg-zinc-900/40 p-4 shadow-xl shadow-black/40 backdrop-blur",
  );
  editorCard.appendChild(el("h2", "text-sm font-medium text-zinc-300", "Editor"));

  const exampleRow = el("div", "flex items-center gap-2");
  exampleRow.appendChild(el("label", "text-xs font-medium text-zinc-400", "Example"));
  const exampleSelect = document.createElement("select");
  exampleSelect.className =
    "flex-1 rounded-lg border border-zinc-800 bg-zinc-950/80 px-2 py-1.5 text-sm text-zinc-200 outline-none transition focus:border-cyan-600/60";

  const moduleGroups = new Map<string, HTMLOptGroupElement>();
  for (const ex of EXAMPLES) {
    let group = moduleGroups.get(ex.module);
    if (!group) {
      group = document.createElement("optgroup");
      group.label = ex.module;
      moduleGroups.set(ex.module, group);
      exampleSelect.appendChild(group);
    }

    const opt = document.createElement("option");
    opt.value = ex.label;
    opt.textContent = ex.label;
    group.appendChild(opt);
  }
  exampleRow.appendChild(exampleSelect);
  editorCard.appendChild(exampleRow);

  const textarea = document.createElement("textarea");
  textarea.rows = 16;
  textarea.spellcheck = false;
  textarea.autocomplete = "off";
  textarea.className =
    "min-h-[220px] max-h-[440px] w-full resize-y varn-scroll rounded-xl border border-zinc-800 bg-zinc-950/80 px-3 py-2 font-mono text-sm leading-relaxed text-cyan-50 outline-none ring-cyan-500/40 transition focus:border-cyan-600/60 focus:ring-2";
  textarea.value = DEFAULT_LUA;

  exampleSelect.addEventListener("change", () => {
    const chosen = EXAMPLES.find((ex) => ex.label === exampleSelect.value);
    if (chosen) {
      textarea.value = chosen.code;
    }
  });

  const toolbar = el("div", "flex flex-wrap gap-2");
  const btnPrimary =
    "inline-flex items-center justify-center rounded-lg bg-cyan-500 px-4 py-2 text-sm font-semibold text-zinc-950 shadow-lg shadow-cyan-500/25 transition hover:bg-cyan-400 focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-cyan-300 disabled:cursor-not-allowed disabled:opacity-40";
  const btnMuted =
    "inline-flex items-center justify-center rounded-lg border border-zinc-700 bg-zinc-900 px-4 py-2 text-sm font-medium text-zinc-200 transition hover:border-zinc-500 hover:bg-zinc-800 focus-visible:outline focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-zinc-400 disabled:cursor-not-allowed disabled:opacity-40";
  const runBtn = el("button", btnPrimary, "Run");
  const stopBtn = el("button", btnMuted, "Stop");
  const clearBtn = el("button", btnMuted, "Clear output");
  runBtn.type = "button";
  stopBtn.type = "button";
  clearBtn.type = "button";

  editorCard.appendChild(textarea);
  toolbar.appendChild(runBtn);
  toolbar.appendChild(stopBtn);
  toolbar.appendChild(clearBtn);
  editorCard.appendChild(toolbar);

  const outCard = el(
    "section",
    "flex min-h-[320px] flex-col gap-3 rounded-2xl border border-zinc-800/80 bg-zinc-900/40 p-4 shadow-xl shadow-black/40 backdrop-blur",
  );
  outCard.appendChild(el("h2", "text-sm font-medium text-zinc-300", "Console"));
  const consolePre = document.createElement("pre");
  consolePre.className =
    "h-[440px] overflow-auto varn-scroll whitespace-pre-wrap break-words rounded-xl border border-zinc-800 bg-black/50 p-3 font-mono text-xs leading-relaxed text-emerald-200/95";
  consolePre.textContent = "Ready.\n";
  const status = el("p", "text-xs text-zinc-500", "Worker idle.");

  outCard.appendChild(consolePre);
  outCard.appendChild(status);

  grid.appendChild(editorCard);
  grid.appendChild(outCard);
  shell.appendChild(intro);
  shell.appendChild(grid);
  const camera = mountCameraPanel(shell);
  root.appendChild(shell);

  // Printed lines are written once per frame, so a chunk that prints in a tight loop costs the page one write per frame instead of one per line.
  let pendingOutput = "";
  let flushScheduled = false;

  const flushOutput = () => {
    flushScheduled = false;

    if (!pendingOutput) {
      return;
    }

    consolePre.appendChild(document.createTextNode(pendingOutput));
    consolePre.scrollTop = consolePre.scrollHeight;
    pendingOutput = "";
  };

  const queueOutput = (text: string) => {
    pendingOutput += text;

    if (!flushScheduled) {
      flushScheduled = true;
      requestAnimationFrame(flushOutput);
    }
  };

  const appendLine = (line: string, kind?: "error") => {
    flushOutput();
    const span = document.createElement("span");
    if (kind === "error") {
      span.className = "text-red-400";
    }
    span.textContent = line.endsWith("\n") ? line : `${line}\n`;
    consolePre.appendChild(span);
    consolePre.scrollTop = consolePre.scrollHeight;
  };

  let worker: Worker;
  let ready = false;

  const post = (msg: MainToWorker, transfer: Transferable[] = []) => {
    worker.postMessage(msg, transfer);
  };

  const onMessage = (ev: MessageEvent<WorkerToMain>) => {
    const msg = ev.data;
    if (msg.type === "ready") {
      ready = true;
      status.textContent = "Worker ready.";
      runBtn.disabled = false;
      return;
    }
    if (msg.type === "output") {
      queueOutput(msg.text);
      return;
    }
    if (msg.type === "cameraStart") {
      camera.start(
        msg.width,
        msg.height,
        (frame) => post({ type: "cameraFrame", frame }, [frame]),
        (message) => post({ type: "cameraError", message }),
      );
      return;
    }
    if (msg.type === "cameraTaken") {
      camera.taken();
      return;
    }
    if (msg.type === "cameraShow") {
      camera.show(msg.frame);
      return;
    }
    if (msg.type === "done") {
      // The output already arrived line by line, so only what follows it is written here.
      const r: RunResult = msg.result;
      camera.stop();
      flushOutput();
      if (!r.ok && r.error) {
        appendLine(`error: ${r.error}`, "error");
      }
      status.textContent = r.ok ? "Finished." : "Finished with errors.";
      runBtn.disabled = false;
      return;
    }
    if (msg.type === "error") {
      camera.stop();
      appendLine(`worker: ${msg.message}`, "error");
      status.textContent = "Error.";
      runBtn.disabled = false;
    }
  };

  const onError = (e: ErrorEvent) => {
    camera.stop();
    appendLine(`worker fault: ${e.message}`, "error");
    status.textContent = "Worker fault.";
    runBtn.disabled = false;
  };

  const spawnWorker = () => {
    worker = new Worker(new URL("./worker.ts", import.meta.url), { type: "module" });
    worker.onmessage = onMessage;
    worker.onerror = onError;
    ready = false;
    runBtn.disabled = true;
    post({ type: "init" });
  };

  spawnWorker();

  runBtn.addEventListener("click", () => {
    if (!ready) {
      appendLine("worker: Still loading wasm…");
      return;
    }
    runBtn.disabled = true;
    camera.hide();
    status.textContent = "Running…";
    post({ type: "run", source: textarea.value });
  });

  stopBtn.addEventListener("click", () => {
    // A hard stop terminates the worker so a sleeping or runaway chunk ends immediately, then a fresh worker re-enables the run button.
    worker.terminate();
    camera.stop();
    appendLine("Stopped.");
    status.textContent = "Stopped.";
    spawnWorker();
  });

  clearBtn.addEventListener("click", () => {
    pendingOutput = "";
    consolePre.textContent = "";
    status.textContent = "Console cleared.";
  });

  // Navigating away terminates the worker so it does not linger after the view is gone.
  return () => {
    camera.hide();
    try {
      worker.terminate();
    } catch {
      // The worker is already gone.
    }
  };
}
