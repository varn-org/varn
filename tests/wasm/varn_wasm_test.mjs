// Runs the WebAssembly engine in Node.js and checks that Lua errors and yields work on the native exceptions of WebAssembly and that the pump of the page drives timers, pools, fetches and events.
// Run it with the command "node tests/wasm/varn_wasm_test.mjs build/wasm/bin" after building the engine with "python3 varn.py wasm".
import fs from "node:fs";
import http from "node:http";
import path from "node:path";
import { pathToFileURL } from "node:url";

const dir = path.resolve(process.argv[2] ?? "build/wasm/bin");
const factory = (await import(pathToFileURL(path.join(dir, "varn_wasm.js")).href)).default;

// The engine is built for the web and workers, so Node.js hands it the module bytes instead of letting it fetch them.
const varn = await factory({ wasmBinary: fs.readFileSync(path.join(dir, "varn_wasm.wasm")) });

let failures = 0;

function report(name, passed, detail) {
  console.log(`${passed ? "PASS" : "FAIL"}  ${name}`);
  if (!passed) {
    failures++;
    console.log(`      ${JSON.stringify(detail)}`);
  }
}

async function check(name, source, predicate) {
  const result = await varn.varnRunChunk(source);
  report(name, predicate(result), result);
}

await check("A chunk prints what it computes.", `local n = 0 for i = 1, 10 do n = n + i end print("sum:", n)`, (r) => r.ok && r.output === "sum:\t55\n");
await check("A chunk that does not compile reports why.", `local = 1`, (r) => !r.ok && r.error.includes("expected near"));
await check("A chunk that raises reports its error.", `error("The chunk failed.")`, (r) => !r.ok && r.error.includes("The chunk failed."));
await check("A protected call catches an error.", `print(pcall(error, "boom"))`, (r) => r.ok && r.output === "false\tboom\n");
await check("A protected call catches a table raised as an error.", `local ok, e = pcall(error, { code = 7 }) print(ok, e.code)`, (r) => r.ok && r.output === "false\t7\n");
await check("A coroutine yields across a protected call and fails after its yields.", `
  local co = coroutine.wrap(function()
    local ok, v = pcall(function() local got = coroutine.yield(1) return got * 2 end)
    coroutine.yield(v)
    error("after the yields")
  end)
  print(co(), co(21), pcall(co))`, (r) => r.ok && r.output.startsWith("1\t42\tfalse\t") && r.output.includes("after the yields"));
await check("A coroutine yields from a metamethod.", `
  local mt = { __index = function(_, k) return coroutine.yield(k) end }
  local co = coroutine.wrap(function() local t = setmetatable({}, mt) return t.x .. "!" end)
  print(co(), co("y"))`, (r) => r.ok && r.output === "x\ty!\n");
await check("A stack overflow is caught.", `local function f() return f() + 1 end print((pcall(f)))`, (r) => r.ok && r.output === "false\n");
await check("A sleep resolves through the pump.", `
  local async = require("async")
  async.run(function()
    print("before")
    async.sleep(30):await()
    print("after")
  end)`, (r) => r.ok && r.output === "before\nafter\n");
await check("Many tasks yield and finish.", `
  local async = require("async")
  local done = 0
  for i = 1, 50 do async.spawn(function() for j = 1, 20 do async.yield():await() end done = done + 1 end) end
  async.run(function() while done < 50 do async.sleep(1):await() end print("done", done) end)`, (r) => r.ok && r.output === "done\t50\n");
await check("A failure after an await is caught by a protected call.", `
  local async = require("async")
  async.run(function()
    local ok, e = pcall(function() async.sleep(1):await() error("late") end)
    print(ok, e:match("late"))
  end)`, (r) => r.ok && r.output === "false\tlate\n");
await check("Work a task posts to a pool after an await runs before the chunk settles.", `
  local async = require("async")
  local fs = require("fs")
  async.spawn(function()
    fs.writeFile("pooled.txt", "first"):await()
    print(fs.readFile("pooled.txt"):await())
  end)`, (r) => r.ok && r.output === "first\n");

// A fetch settles through a callback of the page, which wakes the pump that waits for it.
const server = http.createServer((request, response) => setTimeout(() => response.end(`hello ${request.url}`), 300));
await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
const started = Date.now();
await check("A fetch wakes the pump as soon as it settles.", `
  local async = require("async")
  async.run(function()
    local response = require("http").client.get("http://127.0.0.1:${server.address().port}/varn"):await()
    print(response.status, response.body)
  end)`, (r) => r.ok && r.output === "200\thello /varn\n" && Date.now() - started < 2000);
server.close();

await check("State survives from one chunk to the next.", `persisted = 41`, (r) => r.ok);
await check("A later chunk reads the state.", `print(persisted + 1)`, (r) => r.ok && r.output === "42\n");

// A page drives an interface with a load, events and budgeted polls of its own.
varn.varnRegister("twice", (json) => JSON.stringify(JSON.parse(json) * 2));
const loaded = varn.varnLoadChunk(`
  local async = require("async")
  host.on("tick", function(n) ticks = (ticks or 0) + n end)
  async.spawn(function() async.sleep(10):await() print("slept", host.twice(21)) end)`);
let output = loaded.output;
varn.varnEmit("tick", "5");
for (let poll = 0; poll < 200; poll++) {
  const step = varn.varnPollBudget(4);
  output += step.output;
  if (!step.pending) {
    break;
  }

  await new Promise((resolve) => setTimeout(resolve, Math.max(1, step.idleMilliseconds)));
}

const ticks = await varn.varnRunChunk(`print(ticks)`);
report("A page loads a chunk, emits an event and polls with a budget.", loaded.ok && output === "slept\t42\n" && ticks.output === "5\n", { loaded, output, ticks });

console.log(failures === 0 ? "Every check passed." : `${failures} checks failed.`);
process.exit(failures === 0 ? 0 : 1);
