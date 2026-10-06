// Runs a chunk and then pumps the runtime from the event loop of the page until nothing can make progress, answering a promise of what the chunk printed.
// Each pump runs for a slice of time and hands the thread back, so fetches, timers and messages reach the page between slices.
// The option `onPrint` receives every line the chunk prints, with its newline, at the moment Lua prints it.
Module["varnRunChunk"] = async function (source, options = {}) {
  for (const name of Object.keys(options)) {
    if (name !== "onPrint") {
      throw new TypeError(`The option "${name}" is not known.`);
    }
  }

  const onPrint = options.onPrint ?? null;

  if (onPrint !== null && typeof onPrint !== "function") {
    throw new TypeError('The option "onPrint" must be a function.');
  }

  const sliceMilliseconds = 8;
  const loaded = varnPrintingTo(onPrint, () => Module["varnLoadChunk"](source));
  const result = { ok: loaded.ok, output: loaded.output, error: loaded.error };

  for (;;) {
    const step = varnPrintingTo(onPrint, () => Module["varnPollBudget"](sliceMilliseconds));
    result.output += step.output;
    if (!step.pending) {
      return result;
    }

    await varnWaitFor(step.idleMilliseconds);
  }
};

// The listener of the run whose load or poll is running, since only that run can be printing.
let varnPrintListener = null;

function varnPrintingTo(listener, call) {
  varnPrintListener = listener;
  try {
    return call();
  } finally {
    varnPrintListener = null;
  }
}

// Receives every line Lua prints. A listener that throws is reported once the engine has returned, since a throw unwinding through the engine would cut the Lua call short.
function varnPrinted(line) {
  if (varnPrintListener === null) {
    return;
  }

  try {
    varnPrintListener(line);
  } catch (error) {
    queueMicrotask(() => {
      throw error;
    });
  }
}

// The pumps waiting for work, which the runtime wakes through `varnWakeUp` when a callback of the page posts work while no poll runs.
const varnWaiting = new Set();

function varnWakeUp() {
  const waiting = [...varnWaiting];
  varnWaiting.clear();
  for (const resume of waiting) {
    resume();
  }
}

// Settles once the runtime may have work: at once when work is ready, at its next timer, or when a callback of the page wakes it.
function varnWaitFor(idleMilliseconds) {
  if (idleMilliseconds === 0) {
    // A zero timeout is clamped to several milliseconds once timeouts nest, so ready work yields through a message instead.
    return new Promise((resolve) => {
      const channel = new MessageChannel();
      channel.port1.onmessage = () => {
        channel.port1.close();
        resolve();
      };
      channel.port2.postMessage(null);
    });
  }

  return new Promise((resolve) => {
    const resume = () => {
      clearTimeout(timer);
      varnWaiting.delete(resume);
      resolve();
    };
    const timer = idleMilliseconds > 0 ? setTimeout(resume, idleMilliseconds) : undefined;
    varnWaiting.add(resume);
  });
}
