// Runs a chunk and then pumps the runtime from the event loop of the page until nothing can make progress, answering a promise of what the chunk printed.
// Each pump runs for a slice of time and hands the thread back, so fetches, timers and messages reach the page between slices.
Module["varnRunChunk"] = async function (source) {
  const sliceMilliseconds = 8;
  const loaded = Module["varnLoadChunk"](source);
  const result = { ok: loaded.ok, output: loaded.output, error: loaded.error };

  for (;;) {
    const step = Module["varnPollBudget"](sliceMilliseconds);
    result.output += step.output;
    if (!step.pending) {
      return result;
    }

    await varnWaitFor(step.idleMilliseconds);
  }
};

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
