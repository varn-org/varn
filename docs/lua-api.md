# 📖 Lua API reference

Every built-in module is available with `require`. Each module has its own page below, with
its full API and every runnable example. The examples also live next to each module under
`modules/<module>/lua/examples/` and run from the project root.

## Modules

| Module | What you get |
|----------------|--------------|
| [🌐 `http`](lua-api/http.md) | Web server and client, routing, middleware, WebSockets, and static file serving |
| [🔌 `socket`](lua-api/socket.md) | Network client and server connections |
| [⏳ `async`](lua-api/async.md) | Background tasks, timers, and awaitable promises |
| [📁 `fs`](lua-api/fs.md) | Read and write files, check what already exists |
| [🔐 `crypto`](lua-api/crypto.md) | Hashing, signatures, and secure random data |
| [🧾 `json`](lua-api/json.md) | Encode and decode JSON with a full Lua mapping |
| [🧬 `xml`](lua-api/xml.md) | Encode and decode XML with a full Lua mapping |
| [🗜️ `zip`](lua-api/zip.md) | Create, extract, and list archives |
| [🧩 `ffi`](lua-api/ffi.md) | Call functions from native libraries |
| [🖥️ `platform`](lua-api/platform.md) | System, architecture, processor, and path information |
| [⚙️ `process`](lua-api/process.md) | Run commands and read the environment, working directory, and arguments |
| [🕒 `datetime`](lua-api/datetime.md) | Parse, format, and do calendar arithmetic on instants, with ISO-8601 and fixed offsets |
| [📝 `log`](lua-api/log.md) | Leveled logging with debug, info, warn, and error |

## The `arg` global

The host sets `arg` to the command-line arguments (the script path and its parameters). In
the browser build it is usually empty.
