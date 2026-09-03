# Varn — project guide for Claude

Varn is Lua everywhere: the whole application is written once in Lua and runs on a fast C++ core that behaves the same on desktop, mobile, and the browser (WebAssembly). The C++ `modules/` expose native capabilities to Lua via `require`. The higher-level Lua libraries built on top of them live in their own repository, [varn-org/components](https://github.com/varn-org/components).

This file is binding. Follow it exactly instead of re-deriving conventions each session.

## Core principles (non-negotiable)

- **No gambiarras, no fallbacks, no dead/legacy code, no backward-compat shims.** Write it the way an experienced product C++ engineer at a top company would, using current best practices.
- **No `else` for unknown cases that create surprising implicit behavior.** Handle the known cases explicitly. Do not paper over the unknown.
- **Code and comments are in English.**
- **Do the work only when it genuinely makes sense and is actually needed — never just to show work.**
- **IMPORTANT: never run git commit/push on your own.** Leave the working tree dirty. The user runs git.
- Build with `cmake --build build -j 4` (this machine can freeze with all cores).

## Commands

Everything goes through `python3 varn.py <task>`:

- `build` — build the native `varn` executable (then `./build/bin/varn script.lua` runs a script).
- `test` — run the cross-platform Lua suite (`modules/*/lua/tests/*.lua`, auto-discovered), each test gets a fresh `VARN_TEST_DIR`.
- `test-cpp` — build and run the googletest C++ target.
- `format` — run clang-format over `modules/` and `src/`.
- **Sanitizers** — `build` and `test` both take `--build-dir`, and the build takes `-D VARN_SANITIZE=<list>`, so the whole Lua suite runs instrumented: `python3 varn.py build --build-dir build/asan --config RelWithDebInfo -D VARN_SANITIZE=address,undefined` then `python3 varn.py test --build-dir build/asan` (swap `thread` for a race hunt). Run both before declaring a runtime or networking change safe.
- `wasm` / `app-wasm` / `serve` / `site-deploy` — build the wasm engine, bundle the browser app, dev server, publish the site.
- `lib` — build the embeddable `varn` shared library (`find_package(varn)` package). `lib --prefix <dir> --install` installs the `varn` component into a clean prefix.
- Run a single Lua test directly: `./build/bin/varn modules/<mod>/lua/tests/<name>_test.lua` (set `VARN_TEST_DIR` to a scratch dir).

## Architecture and organization

- **Modules** (`modules/<name>/`) are independent C++ units exposed to Lua through `require("<name>")`. Layout per module: `include/varn/<name>/` (public headers), `src/` (impl), `src/drivers/<driver>/` (swappable backends), `lua/examples/`, `lua/tests/`, `README.md`, `<name>.cmake`.
- **Drivers** are backends selected at build time (e.g. `poco`, `std`, `openssl`, `portable`, `apple`, `android`, `emscripten_fetch`, `dummy`). The `dummy` driver throws `"... not available in this build"`. Keep all drivers of a module consistent in behavior and signatures.
- **The mobile targets talk to the network through the platform.** `VARN_TARGET=apple` forces the http client onto `NSURLSession` and `VARN_TARGET=android` onto `HttpURLConnection`, so an app's `Info.plist` and `network_security_config.xml` govern App Transport Security, trust anchors, certificate pinning and the cleartext policy, and the trust store, system proxy and HTTP/2 come from the OS. A platform transport follows a redirect and decompresses a body on its own, so the driver blocks the redirect and drops `content-encoding`/`content-length` to keep the Lua-facing contract identical across every target. The Android driver resolves its Java classes in `JNI_OnLoad` because a pool thread attached later only sees the bootstrap classloader.
- **Runtime** (`modules/core/.../runtime/`): one `EventLoop`, two `TaskPool`s — `taskPool()` (general work) and `ioPool()` (blocking I/O). Blocking work runs on a pool and resolves a `Promise` on the event loop. Use `varn::async::AsyncTask::runOnPool(...)` for that pattern. `async` exposes `sleep`, `spawn`, `run`, `promise`, `deferred` plus combinators (`all`, `allSettled`, `race`, `any`, `timeout`, `mapLimit`).
- **Runtime invariants (binding):**
  - A job posted to the loop or a pool must never let an exception escape its boundary — `EventLoop::post`/`postDelayed` and `TaskPool::post` wrap the work so an escaping throw cannot `std::terminate` the process and the `WorkLedger` entry is always released.
  - A secure (TLS) connection drives blocking I/O on the `ioPool`, so its socket fd is released by RAII once the last in-flight pool task drops the connection — never eager-close a secure fd from the loop while a pool op may be in flight (see `PocoStreamConnection::close`).
  - Secure (TLS) reads, writes and the handshake are serialized per connection through a strand (`PocoStreamConnection::enqueueSecure`) so no two operations ever touch the same OpenSSL `SSL` object concurrently on the `ioPool`. Overlapping ops on one secure connection run in order, they do not run in parallel.
  - Loop-owned state reachable from the cross-thread public C API is mutex-guarded, since `Runtime::stop()` can be called from another thread while the loop runs — this covers the `Runtime` server list and the lazily created `ioPool()`.
  - The `EventLoop` outlives the `LuaEngine` by declaration order, so its socket watchers must be released **before** the `lua_State` closes — `~Runtime` calls `EventLoop::shutdownIo()` for the paths where `loop.run()` is never entered (a script that fails, an async entry that fails, an entry that requests stop), since an armed accept watcher holds registry refs and `AppState::~AppState` would unref into a closed state.
  - Nothing reached from a cross-thread `Runtime::stop()` may touch the `lua_State` — a destructor that releases a Lua registry ref checks `Runtime::stopped()` first and skips, since `lua_close` reclaims the whole registry anyway (see the server `shared_ptr` deleter in `HttpServerModule` and the `refHolder` in `AsyncModule`).
  - Reading an untrusted path into memory rejects non-regular files (`FsStorage::readAll`) so an endless stream like a device or fifo cannot loop forever.
  - Lua chunks are always loaded text-only (`luaL_loadfilex`/`luaL_loadbufferx` with mode `"t"`) — never enable bytecode loading, since the wasm host runs source supplied by the page.
  - Network output buffers (the WebSocket send queue, streamed responses) are bounded and the connection is dropped when a peer stops draining — never buffer outbound data without a limit. The cap is measured against the **outstanding backlog**, never cumulative traffic, and the sent prefix is reclaimed once it dominates the buffer (`ReactorHelpers::compactSent`), so a busy but healthy peer is never dropped and a partial drain cannot grow the buffer without bound.
  - Captured child-process output is bounded and a child that overruns the cap is killed, so `process.exec` on an endless producer cannot exhaust host memory. A child may also carry `timeoutMs`, which kills it at the deadline and **rejects** rather than resolving a partial result — each `exec` holds one `ioPool` thread for its whole run, and that pool also serves `fs` and the HTTP client.
  - The worker supervisor restarts a worker only on an abnormal exit and backs off a crash loop, so a fast-failing child can never become a fork storm.
  - **Never iterate a container while the loop body can run Lua or a user callback** — that callback can register a handler or close a connection, which reallocates or erases out from under the iterator. Snapshot first (copy the refs, or lock the `weak_ptr`s into a local vector) and walk the snapshot, the way `luaEmit`, the request/response/start hook loops and `wsBroadcast` do. Two remotely triggerable use-after-frees came from missing this.
  - Caller text written into a line-oriented protocol frame is escaped when it is content and refused when it is structure — an SSE payload is split so every CR, LF and CRLF opens a new `data:` field, while an event name or comment, which owns its whole line, raises on a line break the way an invalid header name does. Escaping one field and forgetting its neighbour is how forged records reach a client.
  - A cost or size parameter read back out of stored data (the `N,r,p` of a `scrypt$` hash, a declared length, a window) is validated against a ceiling **before** it is handed to the primitive that allocates for it, and it is range-checked on its full parsed width so a narrowing cast cannot turn an absurd value into an accepted one. An unvalidated cost is a remote kill, not a slow path.
  - Every store keyed by something a client controls is bounded, and the bound is enforced where the entry is added — the session store by `maxSessions` with eviction, the rate-limit store by `maxClients` with a sweep and a reset. An IPv6 peer can source from a whole prefix, so a per-address map with no cap is an unbounded allocation, not a cache.
  - A host event crosses into Lua only through the loop: `Runtime::emitHostEvent` posts the delivery rather than calling in place, skips when `stopped()`, and copies the handler list before invoking any of it, since a handler may subscribe another one.
  - Worker processes share nothing. Anything a handler keeps in memory (sessions, the CSRF secret, rate-limit counters) is private to one worker, so a feature that depends on cross-request state either says so loudly at runtime or lives in a shared store.
  - **A host drives the runtime one of two ways, and both are first-class.** `run_file`/`run_string` take the thread until the loop is idle, which is what a command-line program wants. `load_file`/`load_string` plus `poll` let the platform keep its own run loop, which is what a user interface needs, since every `host.<name>` call then lands on the thread that pumps and a UI bridge touches its widgets with no dispatch and no deadlock. Under wasm `poll` must also drain both task pools, because the browser has no threads and a pool job only ever runs when the pump drains it.
  - A subscription made with `host.on` does **not** hold the loop open. A timer will fire and a socket has a peer, but an event may never arrive, so keeping a process alive forever for one is wrong — this matches Node, where a listener does not keep the process running and only a handle does. A host that wants to wait says so with `varn_runtime_retain`.
  - **A runtime runs as many chunks as the host asks it to.** Per-chunk state (`unhandledError`, `entryRequestedStop`) is reset in `Runtime::beginChunk` before each one, and tearing down the io state belongs to `~Runtime`, never to `EventLoop::run` — a `run()` that clears the poller closes the uv loop and makes every later chunk a silent no-op that still reports success. `tests/cpp/runtime_lifetime_test.cpp` guards this.
  - The loop's keep-alive count lives on the `EventLoop` and is changed under the same mutex the idle-exit decision is taken under, so a `varn_runtime_retain` from another thread can never be read after the loop has already decided to exit. Releasing what was never retained is refused rather than driving the count negative, which would leave the loop unable to ever exit.
  - **A C++ exception must never unwind through a platform frame.** The Apple driver runs every caller callback inside `runHandler:` and the Android sink turns a throw into the Java exception the transport already handles, since unwinding through an `NSOperationQueue` block or a JNI frame terminates the process.
  - The Apple driver's `.mm` is compiled with `-fobjc-arc` (set per source in `http.cmake`). Without it the session, its delegate, the operation queue and every `NSString` handed to Foundation leak on every request.
  - **JNI hygiene is not optional on Android.** An engine pool thread never returns to Java, so each request runs inside a `PushLocalFrame`/`PopLocalFrame` guard or its local references accumulate until the table overflows and aborts. Every `FindClass`/`GetMethodID`/`GetFieldID` result is checked and named in the failure, and the AAR ships `consumer-rules.pro` so R8 in a consuming app cannot rename what JNI resolves by name — the sample app builds a minified release and CI inspects its dex to prove those rules still hold.
- **Lua is compiled as C++**, so a Lua `error()` unwinds as a C++ exception caught by `pcall`. The wasm build needs `-fexceptions`.
- **Components live in their own repository**, [varn-org/components](https://github.com/varn-org/components). They are pure Lua on top of the modules and need only a released `varn` binary, so they never enter this build. A change to a module's Lua-facing API can break them, and that repository's CI is what catches it — it tests against a released engine and re-runs weekly.
- **Targets** (`-DVARN_TARGET=`): `cli` (desktop executable), `wasm` (browser), `apple` (`varn.xcframework`), `android` (`aar`), and `lib` (an embeddable shared library exporting only the C API, installed as a `find_package(varn)` → `varn::varn` package). The same Lua script runs on all of them.
- **Embedding**: the C ABI is `modules/api/include/varn/varn.h` (`varn_runtime_new`/`register`/`emit`/`retain`/`release`/`run_file`/`run_string`/`stop`/`free`, `varn_version`). The bridge runs both ways: `varn_runtime_register` exposes a native function to Lua under the global `host` table, and `varn_runtime_emit` delivers an event from any thread to the handlers a script registered with `host.on`. `varn_runtime_retain`/`release` hold the loop open so a runtime can wait for events instead of exiting once its chunk returns. Everything is marshalled through json, so a host app drives varn (and reaches native capabilities like UI) without touching Lua types. `python3 varn.py lib --prefix <dir> --install` builds and installs the package. The shared library exports only the C API (a linker script hides every statically linked dependency). See [docs/embedding.md](docs/embedding.md) and [examples/embedding](examples/embedding).

## C++ style and formatting

Match the existing visual, structural, and architectural pattern. Compact, professional, consistent. clang-format is LLVM-based with **Allman braces** (run `python3 varn.py format`).

- Avoid excess vertical space. Use only the blank lines needed to separate reading contexts. **Separate blocks of different responsibility with one blank line.**
- Never leave multiple `if`s, validations, loops, state mutations, and returns visually glued together. A function must read with an identifiable beginning, middle, and end at a glance.
- Prefer early returns. Avoid unnecessary nesting. **No unnecessary `else` after a `return`.**
- Extract a function only when one is doing too much — never to shrink size or for artificial abstraction that hides the main flow.
- Keep includes clean, direct, organized. Keep headers, implementation, namespaces, types, names, and responsibilities consistent.
- Prefer `const`, references, and smart pointers for clarity, safety, and ownership. Avoid macros, unsafe casts, and raw owning pointers when a safer project-consistent alternative exists.
- **No free functions anywhere — group helpers as `static` methods of a class** (like `varn::lua::LuaHelpers`). Even a file-local helper is a `static` method of a file-local class declared inside the anonymous namespace, never a bare function floating in the namespace. The only exception is the C-ABI interop functions that must stay `extern "C"` and free.
- **Do not put comments in headers describing methods, sections, or members.**
- **Lambdas: clang-format mangles C++ lambdas, so wrap each non-trivial lambda region in `// clang-format off` … `// clang-format on` and hand-format it cleanly.**
- TLS client contexts must point verification at the OS CA bundle (the bundled OpenSSL ships no trust store) — see `resolveCaBundle` in the poco drivers.

## Comments

- **Every comment is a complete sentence: it starts with a capital letter and ends with a full stop.**
- **A sentence never starts with a lowercase identifier.** Keep the identifier's exact spelling and reword the sentence so it is not the first word.
- **A comment above a function, method, class or module says what it does for the caller, never how it is implemented inside.**
- **Never break one sentence across lines, and never continue a sentence on the next line.** If you need a second sentence, close the first with a full stop and start the next on its own line.
- Comments are objective and natural. Nothing verbose, fragmented or narrative, no decorative banners like `helpers` or `public methods`, and no historical or before/after framing.
- **Comment in the `.cpp`, not in the `.h`, and only where it genuinely earns its place.** A header carries declarations, not prose.
- A comment explains intent or context. It never restates what the code literally does, and usage examples belong in the docs.
- Code and comments are in English.

## The Lua-facing API

- Anything reachable from Lua is a contract with [varn-org/components](https://github.com/varn-org/components) and with anyone else building on the engine. Changing a signature there is a breaking change even when nothing in this repository fails, so it belongs in a major version and the components repository is told before it ships.
- `platform.version` carries `{ major, minor, patch, string }` so a caller can gate on the engine it needs without parsing the string.

## Testing and docs

- Each capability has runnable examples and individual Lua tests. CI runs `modules/*/lua/tests` on Windows/Linux/macOS, and those tests are auto-discovered from the tree.
- **Check `git status` before finishing.** `.gitignore` patterns for build and environment directories are anchored to the repository root (`/env/`, not `env/`) precisely so they cannot swallow a source directory that happens to share the name. A new module or component that does not appear as untracked in `git status` is being ignored, not committed.
- Each build workflow cancels its own superseded runs through a `concurrency` group. The group name is a **literal per file** (`build-linux-${{ github.ref }}`), never `${{ github.workflow }}` — `release.yml` calls all six as reusable workflows, where that context resolves to the caller's name, so they would share one group and cancel each other mid-release. A tag carries its own ref, so a release is never cancelled by a push to main.
- CI gates on Linux with **ASan + UBSan** over the whole Lua suite and the C++ target, and with **TSan** over the C++ target — the Lua suite is not run under TSan because forking under it is pathologically slow. A concurrency fix therefore belongs in `tests/cpp/` where TSan will see it.
- Native unit tests are plain googletest in `tests/cpp/*.cpp` (auto-globbed into `varn_tests`). The target's include dirs let a test reach a module's internal headers. Concurrency and internal-state invariants (`WorkLedger`, `TaskPool`, `EventLoop`) are exercised with multi-threaded stress tests there.
- **Prose in Markdown is never hard-wrapped.** One paragraph is one line, however long it runs, and a list item is one line with its continuation folded in. Never break a sentence across lines — a sentence that starts mid-line in the source is the defect this rule exists to prevent. Tables, fenced code and indented code keep their own line structure.
- **A sentence starts with a capital letter.** The product is `Varn` in prose even though the binary, the library and the header directory are all `varn`, so a sentence opening on the product name is written `Varn`. A sentence never opens with a lowercase identifier — reword it so the identifier is not first.
- **No `;` splitting clauses in documentation either.** Two independent clauses are two sentences. `LICENSE.md` is exempt from every rule here and is never reformatted, since its text is legally verbatim, and so are the GitHub issue templates, whose front matter and field layout are structure rather than prose.
- Docs are two-tier, and **both tiers exist for every module**. Tier one is the `README.md` next to the code: a short intro, a capabilities table, and a closing `## Reference, examples, and tests` section linking the reference page, `lua/examples/`, and `lua/tests/`. Tier two is the full reference under `docs/lua-api/<module>.md` — the complete API plus every runnable example inlined verbatim. `docs/lua-api.md` indexes them, and every row of that table points at a `docs/` page, never at a `README.md`. Adding a module means adding its reference page and its index row in the same change.
- `docs/roadmap.md` records what is covered and what is genuinely still open. Closing a gap means moving it into "Already covered" in the same change — a roadmap that lists shipped features as missing is worse than no roadmap.
- The main `README.md` is presentation only. Keep docs objective and present-tense.
