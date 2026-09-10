# 🌐 WebAssembly

The engine compiled for the browser is two files, and any page can load them and run Lua. This is the same build the [playground](https://varn.pages.dev) uses, so what runs there runs in your site.

| File | What it is |
|---|---|
| `varn_wasm.js` | An ES module whose default export is a factory that instantiates the engine. |
| `varn_wasm.wasm` | The engine itself, fetched by the factory at runtime. |

Take them from the `varn-wasm.tar.gz` asset of a [release](https://github.com/varn-org/varn/releases), or build them yourself with `python3 varn.py wasm` (they land in `build/wasm/bin`).

## Running a script

Serve both files from the same directory and point the factory at it:

```html
<script type="module">
  import createVarnWasm from "/varn/varn_wasm.js";

  const varn = await createVarnWasm({
    locateFile: (path) => `/varn/${path}`,
  });

  const result = varn.varnRunChunk(`
    local n = 0
    for i = 1, 10 do n = n + i end
    print("sum:", n)
  `);

  console.log(result.ok, result.output);   // true, "sum:\t55\n"
</script>
```

`locateFile` is how the module finds `varn_wasm.wasm`. Without it the factory looks beside the page rather than beside the module, which is the usual cause of a 404 on first load.

## The call

`varnRunChunk(source)` runs one chunk and answers a plain object:

| Field | What it holds |
|---|---|
| `ok` | `true` when the chunk ran to completion. |
| `output` | Everything the chunk printed, joined in order, with a trailing newline per `print`. |
| `error` | The Lua error message when `ok` is `false`, otherwise empty. |

State survives between calls: one engine is created on the first call and reused, so a global set by one chunk is still there for the next. Create a second module instance when you want a clean slate.

## Getting a value back

The call returns what the chunk printed, not what it returned. To hand a value to JavaScript, print it as JSON and parse that:

```js
const result = varn.varnRunChunk(`
  local json = require("json")
  local report = { total = 128, items = { "a", "b" } }
  print(json.encode(report))
`);

if (!result.ok) {
  throw new Error(result.error);
}

const report = JSON.parse(result.output);
console.log(report.total);   // 128
```

When the chunk prints more than the payload, give the value a marker and pick that line out rather than parsing the whole output.

## Passing data in

There is no argument list, so the data has to travel inside the chunk. Encode it to base64 and decode it on the Lua side:

```js
function toBase64(text) {
  const bytes = new TextEncoder().encode(text);
  let binary = "";
  for (const byte of bytes) {
    binary += String.fromCharCode(byte);
  }
  return btoa(binary);
}

function runWith(varn, source, data) {
  const payload = toBase64(JSON.stringify(data));
  return varn.varnRunChunk(
    `local input = require("json").decode(require("crypto").base64Decode("${payload}"))\n${source}`
  );
}

const result = runWith(varn, `print(input.name:upper())`, { name: "varn" });
```

Base64 is what makes this safe: the payload becomes plain ASCII with no quote, no backslash and no newline, so it cannot close the string literal or run as code. Interpolating the JSON directly would not be safe — `JSON.stringify` escapes control characters as `\u0000` sequences, which Lua does not accept, so such a payload would break the chunk rather than load it.

## Keeping the page responsive

`varnRunChunk` runs the chunk to completion before returning, so a long loop freezes the tab. Put the engine in a worker and talk to it by message:

```js
// varn-worker.js
import createVarnWasm from "/varn/varn_wasm.js";

const varn = await createVarnWasm({ locateFile: (path) => `/varn/${path}` });
self.postMessage({ type: "ready" });

self.onmessage = (event) => {
  const result = varn.varnRunChunk(event.data.source);
  self.postMessage({ type: "done", result });
};
```

```js
// on the page
const worker = new Worker("/varn-worker.js", { type: "module" });

worker.onmessage = (event) => {
  if (event.data.type === "done") {
    console.log(event.data.result.output);
  }
};

worker.postMessage({ source: 'print("hello from a worker")' });
```

The worker is also the only way to abort a runaway script: terminate it and start another. There is no way to interrupt a chunk in place.

## Serving the files

- `varn_wasm.wasm` must be served as `application/wasm`. A server that sends `application/octet-stream` makes the module fall back to a slower path or fail outright.
- Both files must be reachable from the page's origin, or served with CORS headers that allow it.
- The engine grows its own memory, so no `SharedArrayBuffer` and no cross-origin isolation headers are needed.

## With a bundler

The module is loaded at runtime rather than bundled, because it fetches its own `.wasm` beside itself. Copy both files into the directory your build serves as static assets, and import by URL:

```js
const varn = await (await import(/* @vite-ignore */ `${location.origin}/varn/varn_wasm.js`)).default({
  locateFile: (path) => `${location.origin}/varn/${path}`,
});
```

In Vite that directory is `public/`, which is copied to `dist/` untouched. The `@vite-ignore` comment stops the bundler from trying to resolve the URL at build time.

## What runs in the browser

Most of the engine does. `async`, `datetime`, `json`, `xml`, `log`, `zip` and `fs` all work, `fs` against an in-memory filesystem that starts empty and does not survive a reload. The `http` **client** works, backed by the page's own fetch, which means it obeys CORS like any other request the page makes.

Two modules are narrower than their native form:

- `crypto` carries the essentials — base64 and hex, UUIDs, SHA digests, HMAC and secure random. AES-GCM, scrypt, PBKDF2 and HKDF stay native-only rather than pull OpenSSL into the bundle.
- `http` has no **server**. A page cannot host a listener.

`socket`, `process` and `ffi` are absent for the same reason: a browser has no raw TCP, no process model, and no native calling convention for FFI to target. A script that reaches for one of those raises instead of running.

[docs/platform-availability.md](platform-availability.md) has the full table, module by module. A script can also ask at runtime:

```lua
if require("platform").os() == "wasm" then
    print("running in the browser")
end
```
