<p align="center">
    <a href="https://github.com/varn-org/varn" target="_blank" rel="noopener noreferrer">
        <img width="200" src="extras/images/logo.png" alt="Varn Logo"> </a> </p>

<h1 align="center">Varn</h1>

<p align="center">
    <a href="https://github.com/varn-org/varn/actions/workflows/build-linux.yml"><img src="https://github.com/varn-org/varn/actions/workflows/build-linux.yml/badge.svg" alt="Linux"></a> <a href="https://github.com/varn-org/varn/actions/workflows/build-macos.yml"><img src="https://github.com/varn-org/varn/actions/workflows/build-macos.yml/badge.svg" alt="macOS"></a> <a href="https://github.com/varn-org/varn/actions/workflows/build-windows.yml"><img src="https://github.com/varn-org/varn/actions/workflows/build-windows.yml/badge.svg" alt="Windows"></a> <br> <a href="https://github.com/varn-org/varn/actions/workflows/build-apple.yml"><img src="https://github.com/varn-org/varn/actions/workflows/build-apple.yml/badge.svg" alt="Apple"></a> <a href="https://github.com/varn-org/varn/actions/workflows/build-android.yml"><img src="https://github.com/varn-org/varn/actions/workflows/build-android.yml/badge.svg" alt="Android"></a> <a href="https://github.com/varn-org/varn/actions/workflows/build-wasm.yml"><img src="https://github.com/varn-org/varn/actions/workflows/build-wasm.yml/badge.svg" alt="WebAssembly"></a> </p>

**Lua everywhere.** Write the whole application once in Lua and run that same code on a computer, a phone, or in the browser. Underneath it is a fast C++ core that already carries what a real product needs — a web layer, networking, background work, storage, data protection, and packaging — so there is nothing to assemble and nothing that behaves differently on the next device.

Lua is small enough to learn in an afternoon and expressive enough to hold a whole product, and the engine is what turns it from a scripting language into a place you can actually ship from.

**Try it in your browser at [varn.pages.dev](https://varn.pages.dev)** — a playground running the real engine compiled to WebAssembly.

## 📦 Install

Grab the installer for your system from the [latest release](https://github.com/varn-org/varn/releases/latest). Each one puts `varn` on your `PATH`, so a terminal finds it straight away:

| System | File | Installs to |
|---|---|---|
| macOS | `varn-macos-arm64.pkg` / `varn-macos-x86_64.pkg` | `/usr/local/bin` |
| Linux | `varn-linux-x86_64.deb` | `/usr/bin` |
| Windows | `varn-windows-x86_64-setup.exe` | your chosen folder, added to `PATH` |

```bash
varn --version
varn script.lua
```

The plain archives are there too when you would rather drop the binary somewhere yourself.

## 🚀 Quickstart

```bash
python3 varn.py build
./build/bin/varn apps/lua/server.lua
```

Then open <http://localhost:3000>.

A working web server in a few lines:

```lua
local http = require("http")

local server = http.createServer(function(req, res)
    res:json({ hello = req.query.name or "world" })
end)

server:listen(3000)
```

Every feature is shown with a runnable example inside its reference page under [docs/lua-api/](docs/lua-api.md).

## ✨ Features

Every module is independent and used through `require`, so you pull in only what a script needs.

| Module | What you get |
|----------------|--------------|
| 🌐 `http` | Web server and client, routing, middleware, WebSockets, SSE, and static files |
| 🔌 `socket` | TCP, TLS, UDP, and unix-domain connections |
| ⏳ `async` | Background tasks, timers, promises, and combinators |
| 📁 `fs` | Read, write, stream, stat, and list files |
| 🔐 `crypto` | Hashing, HMAC, password hashing, encryption, encoding, UUIDs, and secure random |
| 🧾 `json` | Encode and decode JSON with a full Lua mapping |
| 🧬 `xml` | Encode and decode XML with a full Lua mapping |
| 🗜️ `zip` | Create, extract, and list archives |
| 🧩 `ffi` | Call functions from native libraries |
| 🖥️ `platform` | System, architecture, processor, and path information |
| ⚙️ `process` | Run commands and read the environment, working directory, and arguments |
| 🕒 `datetime` | Parse, format, and do calendar arithmetic on instants, with ISO-8601 and fixed offsets |
| 📝 `log` | Leveled, structured logging to the console or a file |

Full feature reference: [docs/lua-api.md](docs/lua-api.md).

## 🧱 Components

Databases, caches, job queues, AI providers, validation and testing ship as pure-Lua components in their own repository, [varn-org/components](https://github.com/varn-org/components). They need a `varn` binary and nothing else, so they move at their own pace instead of waiting on an engine release.

```lua
package.path = package.path .. ";varn-components/?.lua;varn-components/?/init.lua"
local redis = require("redis")
```

## 🌍 Runs everywhere

The same scripts run on Linux, macOS, and Windows, on iPhone and Android, and in the browser. You can run them as a standalone app you launch, or embed them inside an app you already have.

## ⚡ Performance

The same `/plaintext` and `/json` routes with no framework on any side, driven by `wrk` on Linux — Varn against raw Node `http` and a raw ASGI app on uvicorn (`uvloop`+`httptools`):

| Scenario | Varn | Node | Python |
|----------|-----:|-----:|-------:|
| 1 core, plaintext | **147k req/s** | 55k | 54k |
| 1 core, json | **120k req/s** | 53k | 48k |
| 4 workers, plaintext | **452k req/s** | 199k | 201k |
| 4 workers, json | **410k req/s** | 184k | 186k |

About **2.5× the throughput of Node and Python per core**, scaling near-linearly with `VARN_WORKERS`.

It holds up on real work too. A second benchmark adds `/db` (a MySQL `SELECT` through `vdo`) and `/cache` (a Redis `INCR`), each over a pooled connection that never blocks the loop:

| Scenario | Varn | Node | Python |
|----------|-----:|-----:|-------:|
| db (MySQL `SELECT`) | **13.5k req/s** | 10.6k | 10.7k |
| cache (Redis `INCR`) | **44.4k req/s** | 35.0k | 15.7k |

Varn **leads every route**, the database and cache included. Database work goes through `vdo` on the io pool so a query never stalls the loop, and the redis client auto-pipelines concurrent commands onto one multiplexed connection. Tail latency is in another class: `/db` p99 of 26 ms against Node's 421 ms and `/cache` p99 of 8 ms against Node's 72 ms, because no GC pauses the event loop. Reproduce it with `python3 varn.py bench`. Method and caveats: [docs/stress-test.md](docs/stress-test.md).

## 📚 Documentation

| Topic | File |
|-------|------|
| 📖 Feature reference | [docs/lua-api.md](docs/lua-api.md) |
| 🧱 Components | [varn-org/components](https://github.com/varn-org/components) |
| 🛠️ Building and running | [docs/build.md](docs/build.md) |
| ⏳ Async and promises | [docs/async.md](docs/async.md) |
| 🔒 Safety notes | [docs/security.md](docs/security.md) |
| 🔥 Stress testing | [docs/stress-test.md](docs/stress-test.md) |
| 🐳 Docker | [docs/docker.md](docs/docker.md) |
| 🌐 WebAssembly in your site | [docs/wasm.md](docs/wasm.md) |
| 🧩 Embedding the C API | [docs/embedding.md](docs/embedding.md) |
| 🧭 Platform availability | [docs/platform-availability.md](docs/platform-availability.md) |
| 🧭 Roadmap | [docs/roadmap.md](docs/roadmap.md) |

## 💜 Support

If this project saved you time, consider supporting it: [GitHub Sponsors](https://github.com/sponsors/paulocoutinhox) · [Ko-fi](https://ko-fi.com/paulocoutinho).

Made with care by [Paulo Coutinho](https://github.com/paulocoutinhox).

Licensed under [MIT](LICENSE.md).
