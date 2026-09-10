import type { MainToWorker, RunResult, WorkerToMain } from "./worker";
import { el } from "./dom";

const DEFAULT_LUA = `local function fib(n)
  if n < 2 then
    return n
  end
  return fib(n - 1) + fib(n - 2)
end

print("fib(12) =", fib(12))

for i = 1, 4 do
  print(string.format("loop %d", i))
end
`;

type Example = { module: string; label: string; code: string };

// examples grouped by module so the demo mirrors what is available in the browser build
// crypto exposes its essential subset here while ffi and the http server stay native-only
const EXAMPLES: ReadonlyArray<Example> = [
  { module: "Lua", label: "Lua — fibonacci", code: DEFAULT_LUA },
  {
    module: "Lua",
    label: "Lua — strings & patterns",
    code: `local text = "varn: fast, small, embeddable"

-- split the sentence into words with a pattern
for word in text:gmatch("%a+") do
  print(word)
end

print("upper:", text:upper())
print("commas:", select(2, text:gsub(",", "")))
`,
  },
  {
    module: "Lua",
    label: "Lua — tables & iteration",
    code: `local fruits = { "apple", "banana", "cherry" }

-- ordered iteration over an array
for index, name in ipairs(fruits) do
  print(index, name)
end

local counts = { apple = 3, banana = 5 }
counts.cherry = 1

-- keyed iteration over a map
for name, n in pairs(counts) do
  print(name, "=", n)
end
`,
  },
  {
    module: "Lua",
    label: "Lua — error handling",
    code: `-- pcall turns a runtime error into a value you can inspect
local ok, err = pcall(function()
  error("something went wrong")
end)
print("ok:", ok)
print("err:", err)

local divided, result = pcall(function(a, b)
  return a / b
end, 10, 2)
print("divided:", divided, "result:", result)
`,
  },
  {
    module: "Lua stdlib",
    label: "string — format, find, gsub, bytes",
    code: `-- the string library for formatting search substitution and bytes
local s = "Varn 1.0 fast and small"
print("format:", string.format("%s has %d chars", s, #s))
print("find:", s:find("%d+%.%d+"))
print("gsub:", (s:gsub("%s+", "_")))
print("upper sub rep:", s:upper():sub(1, 4):rep(2))

local words = {}
for word in s:gmatch("%a+") do
  words[#words + 1] = word
end
print("words:", table.concat(words, " "))
print("byte and char:", string.byte("A"), string.char(86, 97, 114, 110))
`,
  },
  {
    module: "Lua stdlib",
    label: "table — insert, sort, concat, unpack",
    code: `-- the table library to build sort join and unpack arrays
local t = { "banana", "apple", "cherry" }
table.insert(t, "date")
table.remove(t, 1)
table.sort(t)
print("sorted:", table.concat(t, ", "))

local nums = { 3, 1, 2 }
table.sort(nums, function(a, b) return a > b end)
print("desc:", table.unpack(nums))
print("count:", #t)
`,
  },
  {
    module: "Lua stdlib",
    label: "math — round, roots, random",
    code: `-- the math library for rounding roots extremes and randomness
print("floor and ceil:", math.floor(3.7), math.ceil(3.2))
print("sqrt and abs:", math.sqrt(144), math.abs(-9))
print("max and min:", math.max(3, 8, 1), math.min(3, 8, 1))
print("pi:", string.format("%.5f", math.pi))

math.randomseed(42)
print("random:", math.random(1, 6), math.random(1, 6))
print("type:", math.type(3), math.type(3.0))
`,
  },
  {
    module: "Lua stdlib",
    label: "coroutine — cooperative tasks",
    code: `-- coroutines are cooperative tasks that yield values and resume where they left off
local function squares(limit)
  for i = 1, limit do
    coroutine.yield(i * i)
  end
end

local co = coroutine.create(squares)
for _ = 1, 3 do
  local ok, value = coroutine.resume(co, 3)
  print("resumed:", ok, value)
end
print("status:", coroutine.status(co))

local gen = coroutine.wrap(function()
  for c in ("abc"):gmatch(".") do
    coroutine.yield(c)
  end
end)
print("wrap:", gen(), gen(), gen())
`,
  },
  {
    module: "Lua stdlib",
    label: "metatable — operators & objects",
    code: `-- metatables give operator overloading and prototype-based objects
local Vec = {}
Vec.__index = Vec

function Vec.new(x, y)
  return setmetatable({ x = x, y = y }, Vec)
end

function Vec.__add(a, b)
  return Vec.new(a.x + b.x, a.y + b.y)
end

function Vec:length()
  return math.sqrt(self.x ^ 2 + self.y ^ 2)
end

function Vec.__tostring(v)
  return string.format("(%d, %d)", v.x, v.y)
end

local sum = Vec.new(3, 4) + Vec.new(1, 2)
print("sum:", tostring(sum))
print("length:", Vec.new(3, 4):length())
`,
  },
  {
    module: "Lua stdlib",
    label: "os — time, date, clock",
    code: `-- the os library for time dates and a high-resolution clock
print("time is a number:", type(os.time()))
print("formatted date:", os.date("!%Y-%m-%d %H:%M:%S", 1750000000))

local started = os.clock()
local sum = 0
for i = 1, 1000000 do
  sum = sum + i
end
print("sum 1..1e6:", sum)
print(string.format("cpu time: %.3f s", os.clock() - started))
`,
  },
  {
    module: "datetime",
    label: "datetime — parse, format, math",
    code: `local datetime = require("datetime")

local d = datetime.parse("2026-06-21T12:30:00Z")
print("iso:", d:iso())
print("weekday:", d:weekdayName(), "day of year:", d:fields().yearday)

-- calendar-aware arithmetic clamps to the end of a short month
print("jan 31 + 1 month:", datetime.parse("2026-01-31"):add({ months = 1 }):iso())

-- diffs in plain or calendar units
print("days apart:", datetime.parse("2026-03-15"):diffIn(datetime.parse("2026-01-10"), "days"))

-- render the same instant at a fixed utc offset
print("in +05:30:", d:iso(330))
`,
  },
  {
    module: "json",
    label: "json — encode & decode",
    code: `local json = require("json")

local text = json.encode({ name = "varn", tags = { "fast", "small" }, version = 1 })
print("encoded:", text)

local value = json.decode(text)
print("name:", value.name)
print("first tag:", value.tags[1])

print("pretty:")
print(json.encode({ user = { id = 1, roles = { "admin" } } }, { pretty = true }))
`,
  },
  {
    module: "xml",
    label: "xml — encode & decode",
    code: `local xml = require("xml")

-- build a document from the node model
local doc = xml.encode({
  name = "note",
  attributes = { priority = "high" },
  children = {
    { name = "to", text = "Lua" },
    { name = "from", text = "C++" },
  },
}, { pretty = true })
print(doc)

-- parse it back into the same node model
local node = xml.decode(doc)
print("root:", node.name)
print("priority:", node.attributes.priority)
print("first child:", node.children[1].name, node.children[1].text)
`,
  },
  {
    module: "crypto",
    label: "crypto — digests, hmac, codecs, uuid",
    code: `local crypto = require("crypto")

print("sha256:", crypto.digest("SHA256", "varn"))
print("sha512:", crypto.digest("SHA512", "varn"):sub(1, 32) .. "…")
print("hmac:", crypto.hmac("SHA256", "secret-key", "message"))

print("base64:", crypto.base64Encode("hello, varn"))
print("hex:", crypto.hexEncode("abc"))

print("uuid v4:", crypto.uuidV4())
print("uuid v7:", crypto.uuidV7())
print("random:", crypto.hexEncode(crypto.randomBytes(8)))

-- AES-GCM, scrypt and PBKDF2 are native-only and raise in the browser build.
`,
  },
  {
    module: "fs",
    label: "fs — read & write (MEMFS)",
    code: `local async = require("async")
local fs = require("fs")

async.spawn(function()
  fs.writeFile("demo.txt", "hello from lua\\n"):await()

  local content = fs.readFile("demo.txt"):await()
  print("read back:", content)
end)
`,
  },
  {
    module: "zip",
    label: "zip — create & list",
    code: `local async = require("async")
local zip = require("zip")

async.spawn(function()
  -- write a source file into MEMFS
  local file = io.open("hello.txt", "w")
  file:write("zipped!\\n")
  file:close()

  local _, err = zip.create("demo.zip", { { file = "hello.txt", entry = "a/hello.txt" } }):await()
  if err then
    print("zip error:", err)
    return
  end

  local entries = zip.list("demo.zip"):await()
  print("entries:", table.concat(entries, ", "))
end)
`,
  },
  {
    module: "async",
    label: "async — sleep & spawn",
    code: `local async = require("async")

print("start")
async.spawn(function()
  print("task started")
  async.sleep(1500):await()
  print("task woke up after 1500ms")
end)
print("main chunk returned first")
`,
  },
  {
    module: "http",
    label: "http — url encode & decode",
    code: `local http = require("http")

-- percent-encoding works in every build, including the browser
local q = http.urlEncode("a b & c=d/e")
print("encoded:", q)
print("decoded:", http.urlDecode(q))
print("form plus:", http.urlDecode("name=jo%C3%A3o+silva"))
`,
  },
  {
    module: "http",
    label: "http — client fetch",
    code: `local async = require("async")
local http = require("http")

-- the http client works in the browser through the fetch api, and httpbin /delay/N answers after N seconds so the await is visible
async.spawn(function()
  print("requesting with a 2s delay...")
  local res, err = http.client.get("https://httpbin.org/delay/2"):await()
  if err then
    print("request failed:", err)
    return
  end

  print("status:", res.status)
  print("ok:", res.ok)
end)

print("main chunk returned before the response arrived")
`,
  },
  {
    module: "log",
    label: "log — levels",
    code: `local log = require("log")

log.debug("debug", 1)
log.info("info", "hello")
log.warn("careful")
log.error("boom", { code = 7 })

print("log lines were emitted")
`,
  },
  {
    module: "platform",
    label: "platform — system info",
    code: `local p = require("platform")

print("os", p.os())
print("arch", p.arch())
print("cpus", p.cpuCount())
print("pointer bytes", p.pointerSize())
print("endianness", p.endianness())
print("host version", p.hostVersion())
`,
  },
];

export function mountPlayground(root: HTMLElement): () => void {
  const shell = el("div", "mx-auto flex max-w-5xl flex-col gap-6 px-4 py-8 md:px-8");

  const intro = el("div", "space-y-1");
  intro.appendChild(el("h1", "text-2xl font-semibold tracking-tight text-white", "Playground"));
  intro.appendChild(
    el(
      "p",
      "max-w-2xl text-sm leading-relaxed text-zinc-400",
      "Lua runs in a Web Worker with Asyncify so you can stop long loops. Pick an example, grouped by the module it exercises, to try what is available in the browser build. Output is captured from print().",
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
    "h-[440px] overflow-auto varn-scroll rounded-xl border border-zinc-800 bg-black/50 p-3 font-mono text-xs leading-relaxed text-emerald-200/95";
  consolePre.textContent = "Ready.\n";
  const status = el("p", "text-xs text-zinc-500", "Worker idle.");

  outCard.appendChild(consolePre);
  outCard.appendChild(status);

  grid.appendChild(editorCard);
  grid.appendChild(outCard);
  shell.appendChild(intro);
  shell.appendChild(grid);
  root.appendChild(shell);

  const appendLine = (line: string, kind?: "error") => {
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

  const post = (msg: MainToWorker) => {
    worker.postMessage(msg);
  };

  const onMessage = (ev: MessageEvent<WorkerToMain>) => {
    const msg = ev.data;
    if (msg.type === "ready") {
      ready = true;
      status.textContent = "Worker ready.";
      runBtn.disabled = false;
      return;
    }
    if (msg.type === "done") {
      const r: RunResult = msg.result;
      if (r.output) {
        appendLine(r.output.trimEnd());
      }
      if (!r.ok && r.error) {
        appendLine(`error: ${r.error}`, "error");
      }
      status.textContent = r.ok ? "Finished." : "Finished with errors.";
      runBtn.disabled = false;
      return;
    }
    if (msg.type === "error") {
      appendLine(`worker: ${msg.message}`, "error");
      status.textContent = "Error.";
      runBtn.disabled = false;
    }
  };

  const onError = (e: ErrorEvent) => {
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
      appendLine("worker: still loading wasm…");
      return;
    }
    runBtn.disabled = true;
    status.textContent = "Running…";
    post({ type: "run", source: textarea.value });
  });

  stopBtn.addEventListener("click", () => {
    // a hard stop terminates the worker so a sleeping or runaway chunk ends immediately, then a fresh worker re-enables the run button
    worker.terminate();
    appendLine("stopped.");
    status.textContent = "Stopped.";
    spawnWorker();
  });

  clearBtn.addEventListener("click", () => {
    consolePre.textContent = "";
    status.textContent = "Console cleared.";
  });

  // navigating away terminates the worker so it does not linger after the view is gone
  return () => {
    try {
      worker.terminate();
    } catch {
      // already gone
    }
  };
}
