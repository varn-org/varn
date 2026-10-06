# Embedding Varn in another project

Varn is a fast C++ core that runs Lua and is exposed to host programs as a small C library. The same Lua runs on desktop, iOS, Android, and the browser (wasm). This guide shows how another CMake project links Varn, how the host and Lua talk to each other, and how you would drive native mobile UI from one shared Lua script.

Varn is **headless**: it ships modules for HTTP, sockets, filesystem, crypto, JSON, process, and so on, but **no UI toolkit**. Screens and widgets are provided by the host as *host functions* backed by native views, described below.

## The C API

The whole embedding surface is `varn/varn.h`:

| Function | Purpose |
| --- | --- |
| `varn_runtime* varn_runtime_new(void)` | Create a runtime (its own Lua state, event loop, and worker pools) |
| `varn_runtime* varn_runtime_new_with_options(options)` | Create a runtime whose worker pools have the sizes `options` names |
| `int varn_runtime_register(rt, name, fn, userdata)` | Expose a native function to Lua as `host.<name>` (call before running a chunk) |
| `int varn_runtime_emit(rt, name, json_argument)` | Deliver an event to the Lua handlers registered for `name` with `host.on`, from any thread |
| `int varn_runtime_retain(rt)` / `varn_runtime_release(rt)` | Hold the event loop open so the runtime waits for events instead of exiting |
| `int varn_runtime_run_file(rt, path)` | Load and run a Lua file, then pump the event loop until it is idle |
| `int varn_runtime_load_file(rt, path)` / `varn_runtime_load_string(rt, source, name)` | Run a chunk and hand control straight back, leaving the event loop to the host |
| `int varn_runtime_poll(rt)` | Advance the runtime once without blocking, answering 1 while work remains |
| `int varn_runtime_poll_budget(rt, budget_nanoseconds, result)` | Advance the runtime without blocking for about the budget, repeating passes while they make progress, and report in `result` whether anything ran |
| `void varn_runtime_set_wake(rt, fn, userdata)` | Have `fn` called from any thread whenever work reaches the runtime from another thread, or stop with a null `fn` |
| `long long varn_runtime_idle(rt)` | The milliseconds the runtime can wait before it has work of its own, `0` when work is ready and `-1` when only a wake can bring work |
| `int varn_runtime_run_string(rt, source, chunk_name)` | Run Lua from a string |
| `void varn_runtime_stop(rt)` | Ask a running runtime to stop |
| `void varn_runtime_free(rt)` | Destroy the runtime (joins its threads) |
| `const char* varn_version(void)` | The library version string |

Return codes: `0` success, `1` a load/run error, `2` a bad argument.

## Adding Varn to a CMake project

### Option A — installed package (`find_package`)

Build Varn as a self-contained shared library and install it once:

```sh
python3 varn.py lib --prefix /opt/varn --install
```

This wraps the plain CMake flow:

```sh
cmake -B build/lib -S path/to/varn \
  -DVARN_TARGET=lib -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/varn
cmake --build build/lib
cmake --install build/lib --component varn   # Library, header and CMake package only
```

The setting `VARN_TARGET=lib` produces a `varn` shared library that links every dependency (Lua, Poco, OpenSSL, libuv, …) **privately** and exports only the C API, plus a `find_package` package. Consume it:

```cmake
find_package(varn REQUIRED)

add_executable(app main.c)
target_link_libraries(app PRIVATE varn::varn)   # Makes #include <varn/varn.h> available
```

Point your build at the install prefix with `-DCMAKE_PREFIX_PATH=/opt/varn`. A complete, buildable consumer lives in [examples/embedding](../examples/embedding).

### Option B — from source (`FetchContent` / `add_subdirectory`)

```cmake
include(FetchContent)
FetchContent_Declare(varn GIT_REPOSITORY <url> GIT_TAG <ref>)
set(VARN_TARGET lib CACHE STRING "" FORCE)
FetchContent_MakeAvailable(varn)

target_link_libraries(app PRIVATE varn::varn)
```

A host that already builds nlohmann/json keeps one copy of it, because Varn takes the `nlohmann_json::nlohmann_json` target it finds instead of fetching its own.

### Option C — prebuilt mobile artifacts

- **iOS / macOS**: The command `python3 varn.py apple` builds `varn.xcframework` (device + simulator) exposing `varn/varn.h`. Add it to an Xcode target or a Swift package.
- **Android**: The command `python3 varn.py android` builds an `.aar` (`libvarn.so` plus the JNI bindings under `android/varn/`).

## Exposing native capabilities to Lua

The function `varn_runtime_register` binds a C callback to a name inside the global Lua table `host`. When Lua calls `host.<name>(value)`, Varn serializes the argument to JSON, hands that string to your callback, and decodes the JSON your callback returns back into a Lua value.

```c
#include <varn/varn.h>
#include <stdio.h>

static const char* greet(const char* json_argument, void* userdata)
{
    (void)userdata;
    printf("[host] %s\n", json_argument);       // For example {"name":"world"}.
    return "{\"message\":\"Hello from the host\"}";
}

int main(void)
{
    varn_runtime* rt = varn_runtime_new();
    varn_runtime_register(rt, "greet", greet, NULL);
    varn_runtime_run_string(rt,
        "local r = host.greet({ name = 'world' })\n"
        "print(r.message)\n", "example");
    varn_runtime_free(rt);
    return 0;
}
```

The JSON boundary keeps the C ABI free of Lua types, so any language that can implement a `const char* (*)(const char*, void*)` callback and speak JSON can plug in. The returned pointer is copied before Lua resumes, so it only needs to be valid until the callback returns, and returning `NULL` is read as JSON `null`. Register every host function before running a chunk.

## Sending events back to Lua

The function `varn_runtime_register` is one direction: Lua calls the host. The function `varn_runtime_emit` is the other, and it is what a user interface needs — a tap happens on the host side and has to reach the script.

Lua subscribes with `host.on`, which takes a name and a function:

```lua
host.on("tap", function(payload)
    print("tapped " .. payload.id)
end)
```

The host delivers whenever it likes, from whatever thread it is on:

```c
varn_runtime_emit(rt, "tap", "{\"id\":\"save\"}");
```

Two rules make this safe. The call is posted to the event loop, so the Lua state is only ever touched on the loop thread no matter which thread emitted. And the handler list is copied before any handler runs, so a handler may subscribe another one without disturbing the delivery in progress.

An event with no subscriber is not an error, and a handler that raises is logged without taking the loop down.

### Keeping the runtime alive

The functions `varn_runtime_run_file` and `varn_runtime_run_string` pump the loop until it is idle and then return. A script that only subscribes to events has nothing pending, so the runtime would finish immediately. The function `varn_runtime_retain` holds the loop open until the matching `varn_runtime_release`:

```c
varn_runtime_retain(rt);              /* Before running, so the loop never sees itself idle. */
varn_runtime_run_string(rt, script, "=app");   /* Returns when the host releases. */
```

Run it on a thread of your own. It blocks for as long as the runtime lives, and the emitting side stays free.

Retain and release are safe from any thread, and a retain taken while the loop is already running is never lost to the loop's own decision to exit. The two must balance, so `varn_runtime_release` answers non-zero when there is no retain left to give back. That answer is worth checking, because a host that releases more than it retained has a bug that would otherwise surface as a runtime exiting earlier than it should.

A runtime is not single-shot. Each `varn_runtime_run_file` or `varn_runtime_run_string` gets the event loop and its own exit code, so a host can load one chunk and then run another on the same runtime, keeping every host function, subscription and piece of Lua state the earlier chunk left behind.

### Sizing the worker pools

A runtime runs blocking work on two pools of threads. The task pool has a thread per core, and the I/O pool, created on first use, serves files, processes and the HTTP client with a fixed count of threads that `kIoThreads` in `Runtime.cpp` names. A host that knows its device better, such as a game on a phone that wants its cores for itself, sizes both with `varn_runtime_new_with_options`, where a size left at zero keeps its default:

```c
varn_runtime_options options = { .task_threads = 2, .io_threads = 4 };
varn_runtime* rt = varn_runtime_new_with_options(&options);
```

An engine that links the static core passes the same sizes as `Runtime::Options`, as in `varn::runtime::Runtime runtime({"game"}, 1, {2, 4})`. A post wakes a worker only when one is idle and no other wake is already on its way, and a worker that takes a job wakes the next one while jobs remain, so a burst of posts from the loop thread costs it one wake rather than one per job. A promise that settles while nothing awaits it posts nothing to the loop, and the loop thread never wakes itself.

## Driving the runtime from the host's own run loop

The functions `varn_runtime_run_file` and `varn_runtime_run_string` take the calling thread until the runtime has nothing left to do. That is what a command-line program wants, and it is the wrong shape for a user interface, because UIKit and Android's `View` may only be touched on the main thread. A host that runs the engine on a background thread has to hop every call across threads, and a call that must return something — the handle of a widget it just created — has to hop back synchronously, which blocks the engine thread on the main thread.

The alternative is to let the platform keep its own run loop and drive the runtime from it. The functions `varn_runtime_load_file` and `varn_runtime_load_string` run the chunk and return immediately, leaving whatever it armed in place. The function `varn_runtime_poll` then advances the runtime once and never blocks, answering `1` while something can still make progress and `0` once nothing can.

```c
varn_runtime_load_file(rt, "app.lua");   /* Returns as soon as the chunk itself is done. */

/* Called from CADisplayLink, Choreographer, a Handler, or requestAnimationFrame. */
varn_runtime_poll(rt);
```

Everything then happens on the thread that pumps. A `host.<name>` call from Lua arrives on that thread, so a UI bridge touches its widgets directly with no dispatch, no lock and no chance of deadlock. Timers, sockets, the HTTP client and events delivered with `varn_runtime_emit` are all advanced by the same call.

Poll and the blocking calls are two ways to drive one runtime, not two modes it has to be put into. A host may load a chunk, pump it for a while, and still call `varn_runtime_stop` to end it.

### Sleeping until there is work

A desktop application that draws only when something changes has no display link to pump from, and polling on a fixed tick wakes it forever for nothing. The runtime tells such a host when to poll instead. The function `varn_runtime_idle` answers how long the runtime can wait before it has work of its own, which is `0` while work is ready, the time to its next timer while one is armed and `-1` when only something from outside can bring work. The function `varn_runtime_set_wake` gives it a function it calls from any thread whenever work arrives from another thread, such as a promise a worker settled, a response of the HTTP client or an event emitted with `varn_runtime_emit`.

```c
static void wake(void* userdata) { glfwPostEmptyEvent(); }

varn_runtime_set_wake(rt, wake, NULL);

for (;;)
{
    varn_runtime_poll(rt);
    const long long idle = varn_runtime_idle(rt);
    if (idle < 0) glfwWaitEvents();
    else glfwWaitEventsTimeout(idle / 1000.0);
}
```

The function runs on the thread that posted the work, so it only wakes the host and never touches the runtime. Passing a null function stops the calls and returns once no call is still running, so the host can release what the function reaches right after it. A watched socket is read only when the runtime is polled, so while one is open `varn_runtime_idle` answers at most fifty milliseconds.

### Bounding a poll to a frame

The function `varn_runtime_poll` runs every job and timer that is ready, including the work they post, and then serves the sockets once. A game that polls once per frame needs two things it cannot give: a bound, since thousands of tasks that each sleep one frame can take longer to wake than the frame lasts and keep that poll from ever returning, and more than one step per socket, since a task resumed by a receive only reads again in the next frame.

The function `varn_runtime_poll_budget` gives both. The loop runs in passes, and each pass runs the jobs that were queued and the timers that were due when it started and then serves every socket that is ready without waiting. A timer armed during a pass, even for zero milliseconds, waits for the next pass, and so does a task that a promise settled during the pass resumes. The poll repeats passes while they make progress and the budget lasts, so a request to the runtime's own server, a stream or a burst of datagrams moves as far in one frame as the budget allows, and it checks the budget between jobs and between passes, so it overruns the budget by at most one job and one pass over the sockets. It always makes progress when work is ready, even with a budget of zero.

```c
varn_poll_result result;
const long long frameBudget = 4 * 1000 * 1000; /* Four milliseconds of the frame. */

/* Called once per frame. */
varn_runtime_poll_budget(rt, frameBudget, &result);
if (!result.pending) { /* Nothing can make progress until an event or a wake arrives. */ }
```

The field `ran` says whether any job, timer or socket callback ran, so a host that polls again while time is left stops once a poll ran nothing, the field `pending` answers what `varn_runtime_poll` answers, and the field `idle_milliseconds` answers what `varn_runtime_idle` answers, which is `0` when the budget ended a poll with work still ready. A task gives the host its frame back with `async.yield()`, a promise that settles in the next pass. An engine that links the static core calls `Runtime::poll(std::chrono::nanoseconds)`, which answers the same `ran`, `pending` and `idleMilliseconds` in an `EventLoop::PollResult`.

### In the browser

The WebAssembly build exposes the same bridge to JavaScript, so a page is a host like any other. The browser runs everything on one thread, so the page pumps the runtime from its own loop and Lua never blocks it:

```js
varnRegister("ui_button", (json) => {
    const spec = JSON.parse(json);
    const id = createRealButton(spec);
    return JSON.stringify({ id });
});

varnLoadChunk(appLuaSource);

const tick = () => { varnPoll(); requestAnimationFrame(tick); };
requestAnimationFrame(tick);

button.onclick = () => varnEmit("tap", JSON.stringify({ id: "save" }));
```

The function `varnPollBudget(milliseconds)` is `varn_runtime_poll_budget` for the page. It answers `{ ran, pending, idleMilliseconds, output }`, where `output` is what Lua printed during the poll, while `varnPoll` writes that to the console. The function `varnRunChunk` is written on top of them in JavaScript. It loads the chunk and pumps the runtime in slices of a few milliseconds from the event loop of the page until nothing can make progress, waiting between slices for the next timer or for the callback of the page that brings work, such as the end of a fetch, and answers a promise of what the chunk printed, which is what the playground uses. The pair `varnLoadChunk` plus `varnPoll` or `varnPollBudget` is the shape an interface wants.

## Adding native modules and symbols from C++

An engine that links the static core and holds a `varn::runtime::Runtime` itself adds what Lua reaches through two calls made before its first chunk runs, so it never reaches into the Lua state of the core.

```cpp
static int openGraphics(lua_State* L)
{
    lua_newtable(L);
    // Fill the module table with the functions of the engine.
    return 1;
}

varn::runtime::Runtime runtime({"game"});

if (!runtime.addModule("engine.graphics", &openGraphics))
{
    // The name is one a module of Varn or an earlier call already answers.
}

if (!runtime.addSymbol("SteamAPI_RunCallbacks", reinterpret_cast<void*>(&SteamAPI_RunCallbacks)))
{
    // The name was already added with another address.
}

runtime.runScript("main.lua");
```

The function `addModule` makes a module that Lua loads with `require("engine.graphics")`, once, on its first `require`. It answers `false` for an empty name, a null opener and a name that a module of Varn or an earlier call on the same runtime already answers, so a host never replaces a module by accident.

The function `addSymbol` makes a native function or value reachable through `ffi.C` under its name before the dynamic linker is asked, which is what a library linked statically needs on iOS and tvOS, where no export reaches it. Adding a name again with the same address is accepted, and with another address it answers `false`.

A failure that no caller can receive goes to the one handler an application sets from Lua with `async.onFailure(handler)`, together with its traceback as text and as a list of frames, and its kind. That covers a task of `async.spawn` or `async.run` that raises, a handler of `http.createServer`, a route of `http.createApp`, a WebSocket callback and a callback of `http.client.stream` that fail, an ffi callback that fails outside any ffi call, and a promise that rejects with no one observing it, whose kind is `"rejection"` where every other one is `"failure"`. An ffi callback that fails inside an ffi call raises its error from that call as it was raised, and a task the error ends reports the frames of the callback. That handler is the one way a host learns of such failures, so an engine that shows them sets it in its first chunk. Without a handler, the runtime logs a failed task or an unobserved rejection with its traceback and stops the loop, and logs the failure of a server handler or a callback and goes on.

### A yield runs no destructor

Varn builds Lua as C++, so a Lua error unwinds the C++ frames between the raise and the protected call that catches it as an exception, and their destructors run. A yield does not. On every compiler but MSVC it jumps straight back to the `lua_resume` that waits for it with `_longjmp`, which keeps a yield and its resume under a hundred nanoseconds, and it leaves every C++ frame between `lua_yieldk` and that resume without running a destructor.

A native function that yields, or that calls Lua through `lua_callk` or `lua_pcallk` with a continuation, therefore keeps no destructible local alive across that call. A `std::string`, a container, a smart pointer or a lock lives in a block that closes before the call, and whatever the continuation needs travels on the Lua stack or in its `lua_KContext`. Such a call is never made from inside a `catch` block either, since the jump would leave the exception it caught unfinished.

```cpp
static int guardContinue(lua_State* L, int status, lua_KContext ctx)
{
    return 0;
}

static int guard(lua_State* L)
{
    {
        const std::string token = readToken(L);
        // Use the token while it lives.
    }

    lua_pushvalue(L, 2);
    lua_callk(L, 0, 0, 0, &guardContinue);
    return guardContinue(L, LUA_OK, 0);
}
```

A call through `lua_call` or `lua_pcall`, which takes no continuation, is no yield point, since Lua refuses a yield across it with the error "attempt to yield across a C-call boundary", so the rule covers only the calls that take a continuation and `lua_yieldk` itself. On MSVC both a yield and an error stay exceptions, because its `longjmp` unwinds every frame the way an exception does and would save only about half the cost, but code meant for every target follows the rule anyway.

## Sending HTTP requests from C++

An engine that links the static core and holds a `varn::runtime::Runtime` sends HTTP requests through the class `varn::http::client::HttpClient` of the header `varn/http/HttpClient.h`, with no Lua table in between. It works on every target over the transport the Lua client uses there, Poco on the desktop, `NSURLSession` on Apple platforms, `HttpURLConnection` on Android and `fetch` in the browser, and the functions of `http.client` are built on it. The head of a response arrives before its body, and the body arrives in pieces as the connection delivers them, so no response is ever held whole.

```cpp
#include <varn/http/HttpClient.h>

namespace client = varn::http::client;

client::HttpClient http(runtime);

client::Request request;
request.url = "https://example.com/levels/forest.pak";
request.headers = {{"If-None-Match", "\"v41\""}};
request.timeout = std::chrono::minutes(5);
request.maxResponseBytes = 2ull * 1024 * 1024 * 1024;

client::HttpClient::Callbacks callbacks;
callbacks.onHead = [](const client::ResponseHead& head)
{
    // The status and every header arrive before the first byte of the body.
};
callbacks.onChunk = [&file](std::string_view chunk)
{
    file.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
};
callbacks.onProgress = [](const client::Progress& progress)
{
    // The field `total` is empty when the server did not say how long the body is.
};
callbacks.onComplete = [](const std::optional<client::Error>& error)
{
    if (error && error->code == client::ErrorCode::Timeout)
    {
        // Every failure carries a code and a message.
    }
};

client::Handle handle = http.send(std::move(request), std::move(callbacks));

// From any thread, at any time.
handle.cancel();
```

The function `send` answers a handle at once, and every callback runs later on the thread that runs or polls the loop of the runtime, like everything else in Varn, a refusal of the request included. The callback `onHead` runs once, `onChunk` runs for each piece with a view that is valid only during the call, `onProgress` follows each piece with the bytes received so far and the total when the server declared one, and `onComplete` runs exactly once, with no error on success. Nothing runs after `onComplete`. A callback that throws ends the request, and `onComplete` reports the failure with the code `Callback`.

| Field of `Request` | What it holds | Default |
|---|---|---|
| `method` | The method, a token such as `GET` or `PUT` | `GET` |
| `url` | An `http` or `https` address | Required |
| `headers` | Name and value pairs, sent as given, such as `Range`, `If-None-Match` and `If-Modified-Since` | None |
| `body` | A body held in memory | None |
| `bodySource` | A callback that fills a buffer with the next piece of the body and answers how many bytes it wrote, zero at the end | None |
| `bodyLength` | The length a `bodySource` writes, sent as `Content-Length`, or else the body is sent in chunks | None |
| `timeout` | One deadline for the whole request, from the connection to the last byte and across every redirect | 60 seconds |
| `maxResponseBytes` | The largest body accepted | 64 MiB |
| `verifyTls` | Whether the certificate of a secure server is verified | `true` |
| `redirects` | `RedirectPolicy::Follow`, `Manual` or `Error`, with the rules of [the Lua client](lua-api/http.md#redirects) | `Follow` |
| `maxRedirects` | How many hops a redirect may make | `20` |

The deadline is a timer of the loop, so it ends a request on time whatever the transport waits for. A body larger than `maxResponseBytes` is refused at its head when the server declares its length, and otherwise at the piece that passes the limit, before any later piece is read. The transport hands the loop at most a bounded window of the body that the callbacks have not taken yet, the constant `kWindowBytes` of `modules/http/src/HttpClientTransfer.cpp`, so a host that stops polling stops the download rather than gathering it in memory. The method `Handle::cancel` works from any thread, ends the request with the code `Cancelled` and stops the transport where it waits. Dropping a handle does not cancel its request, and `Handle::active` answers whether the request is still running.

The callback `bodySource` runs on the thread of the transport, never on the loop, so it reads a file or a buffer of the engine and never touches Lua. In the browser the pieces are gathered into a `Blob` before the request starts, since a browser streams a request body only over HTTP/2 and only in some engines. A redirect that would send a streamed body again fails with the code `Redirect`, since the body was read once.

| `ErrorCode` | When |
|---|---|
| `Invalid` | The request cannot be sent as given, such as a method, an address or a header that is not valid, both `body` and `bodySource`, a `bodySource` that writes more or less than its `bodyLength`, or a redirect policy the browser cannot keep |
| `Unavailable` | The build has no transport for it, such as a secure address in a build without TLS |
| `Network` | The connection failed or ended before the body was whole |
| `Tls` | The secure connection failed, such as a certificate that does not verify |
| `Timeout` | The deadline passed |
| `Cancelled` | The handle cancelled it |
| `TooLarge` | The body is larger than `maxResponseBytes` |
| `Redirect` | A redirect was refused, the chain was longer than `maxRedirects`, or it would have sent a streamed body again |
| `Callback` | A callback or the body source threw |

### Keeping connections

A client keeps the connections it opened, so a request to an origin it already reached skips the TCP and TLS handshakes. On the desktop it keeps a bounded pool of idle connections per origin and closes one that idled too long or that the server closed. On Apple platforms it shares one `NSURLSession` among its requests, one for the requests that verify servers and one for those that do not, so a connection trusted without verification never carries a request that asked for verification. On Android the platform keeps the pool, and a connection whose body was read to its end goes back to it. The browser keeps its own. One client serves a whole engine, and the functions of `http.client` share one client per runtime.

Destroying a client cancels every request it still runs without calling their callbacks, so it is destroyed on the loop thread and before its runtime.

### Resuming a download

A download that stopped resumes from the bytes it already holds, which the server answers with a partial response.

1. Send the request again with the header `Range` set to `bytes=<held>-`, where `<held>` is the number of bytes already written, and with `If-Range` set to the `ETag` of the first response when it had one, so a file that changed is sent whole instead of spliced.
2. On the status `206`, read `ResponseHead::contentRange()`, check that its `first` equals the bytes held, and append the body. Its `completeLength` is the length of the whole file when the server knows it.
3. On the status `200` the server ignored the range or the file changed, so truncate what was held and write the body from the start.
4. On the status `416` the range starts past the end, which for an open range means the file is already whole.

```cpp
callbacks.onHead = [&resumed, held](const client::ResponseHead& head)
{
    const std::optional<client::ContentRange> range = head.contentRange();
    resumed = head.status == 206 && range && range->first == held;
};
```

The callback `onChunk` then appends to what is held when `resumed` is set and writes the file from its start otherwise.

The method `ResponseHead::header(name)` finds a header whatever the case of its name, and `ResponseHead::contentLength` holds the length of the body as it is delivered, which is empty when the server did not declare one or when a platform transport decoded a compressed body.

### Platform notes

- A platform transport and the browser decode a compressed body before handing it over, so they drop `Content-Encoding` and `Content-Length` from the headers, the way the Lua client does.
- The URL Loading System holds the first bytes of a body whose type it may sniff before it hands any of them over. A server that streams small pieces to Apple platforms sends the `Content-Type` `text/event-stream` or the header `X-Content-Type-Options: nosniff`.
- The browser shows only the headers its CORS rules expose to the page, and it follows every redirect itself, so a request there must keep the policy `Follow`.

## Driving native UI from one Lua script

The pattern for a mobile app whose screens are written in Lua and rendered with the platform's native widgets:

1. The host registers UI primitives (`ui_screen`, `ui_label`, `ui_button`, …) as host functions, each backed by a real `UIView` on iOS or `View` on Android.
2. One shared `app.lua` describes the screens by calling those primitives — it is byte-for-byte the same on both platforms.
3. A native event (a tap) runs a Lua handler, so behaviour also lives in Lua.

Varn does not provide these primitives — you write the thin native bridge once per platform. What is identical is the Lua.

### The shared screen script (`app.lua`, same on iOS and Android)

```lua
local home = host.ui_screen({ title = "Home" })
host.ui_label({ screen = home, text = "Welcome to varn" })
host.ui_button({ screen = home, id = "tap", text = "Tap me" })
host.ui_present({ screen = home })

-- Event handlers the host dispatches to on a native tap
function onTap(id)
    if id == "tap" then
        host.ui_alert({ title = "Hi", message = "Tapped from Lua" })
    end
end
```

### iOS (Swift) bridge

```swift
import Foundation

final class VarnHost {
    private var runtime: OpaquePointer?
    private var screens: [String: UIViewController] = [:]

    func start() {
        runtime = varn_runtime_new()
        let me = Unmanaged.passUnretained(self).toOpaque()

        // Register a native-backed primitive. The C callback reaches `self` through userdata.
        varn_runtime_register(runtime, "ui_screen", { json, ud in
            let host = Unmanaged<VarnHost>.fromOpaque(ud!).takeUnretainedValue()
            return host.makeScreen(fromJSON: String(cString: json!))
        }, me)

        varn_runtime_register(runtime, "ui_button", { json, ud in
            let host = Unmanaged<VarnHost>.fromOpaque(ud!).takeUnretainedValue()
            return host.makeButton(fromJSON: String(cString: json!))
        }, me)
        // Register ui_label, ui_present and ui_alert the same way.

        varn_runtime_run_file(runtime, Bundle.main.path(forResource: "app", ofType: "lua"))
    }

    func makeScreen(fromJSON json: String) -> UnsafePointer<CChar> {
        // Decode {title=...}, build a UIViewController with native UIKit views and keep it in `screens`.
        // Return a JSON handle the Lua side stores, for example {"id":"home"}.
        return staticCString("{\"id\":\"home\"}")
    }

    // On a real UIButton tap, dispatch back into Lua.
    @objc func buttonTapped(id: String) {
        varn_runtime_run_string(runtime, "onTap('\(id)')", "tap")
    }
}
```

### Android (Kotlin + a small C++/JNI shim) bridge

Kotlin cannot hand a raw C function pointer to `varn_runtime_register`, so the registration lives in the JNI layer, which calls up into Kotlin:

```cpp
// This bridge is registered once from JNI_OnLoad or a native init method.
static JavaVM* gVm;
static jobject gHostUi;   // A global ref to the Kotlin object that owns the native Views

static const char* ui_screen(const char* json, void*) {
    JNIEnv* env; gVm->AttachCurrentThread(&env, nullptr);
    jstring arg = env->NewStringUTF(json);
    jstring res = (jstring) env->CallObjectMethod(gHostUi, gMakeScreen, arg);
    static thread_local std::string out;         // Outlives the return and is copied by Varn immediately.
    const char* c = env->GetStringUTFChars(res, nullptr);
    out = c; env->ReleaseStringUTFChars(res, c);
    return out.c_str();
}

extern "C" JNIEXPORT void JNICALL Java_com_varn_VarnRuntime_registerUi(JNIEnv*, jobject, jlong rt) {
    varn_runtime_register(reinterpret_cast<varn_runtime*>(rt), "ui_screen", &ui_screen, nullptr);
    // Register ui_button, ui_label, ui_present and ui_alert the same way.
}
```

```kotlin
class HostUi(private val activity: Activity) {
    // Called from the native ui_screen. It builds real Android Views and returns a JSON handle.
    fun makeScreen(json: String): String {
        // Parse {title=...}, inflate or compose native Views and remember the screen.
        return "{\"id\":\"home\"}"
    }
    // On a real button tap, VarnRuntime.runString("onTap('tap')") dispatches into Lua.
}
```

The screen logic (`app.lua`) is shared. Only `makeScreen`/`makeButton`/… differ, and each builds the platform's own native components, so the app looks and feels native on both while the code you write is Lua.

### Notes and current limits

- There is no bundled widget set, layout engine, or navigation — those are yours to expose. Varn provides the runtime, the Lua ↔ native bridge, and everything non-visual (networking, storage, crypto, JSON, scheduling).
