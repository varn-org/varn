# Embedding Varn in another project

Varn is a fast C++ core that runs Lua and is exposed to host programs as a small C library. The same Lua runs on desktop, iOS, Android, and the browser (wasm). This guide shows how another CMake project links Varn, how the host and Lua talk to each other, and how you would drive native mobile UI from one shared Lua script.

Varn is **headless**: it ships modules for HTTP, sockets, filesystem, crypto, JSON, process, and so on, but **no UI toolkit**. Screens and widgets are provided by the host as *host functions* backed by native views, described below.

## The C API

The whole embedding surface is `varn/varn.h`:

| Function | Purpose |
| --- | --- |
| `varn_runtime* varn_runtime_new(void)` | Create a runtime (its own Lua state, event loop, and worker pools) |
| `int varn_runtime_register(rt, name, fn, userdata)` | Expose a native function to Lua as `host.<name>` (call before running a chunk) |
| `int varn_runtime_emit(rt, name, json_argument)` | Deliver an event to the Lua handlers registered for `name` with `host.on`, from any thread |
| `int varn_runtime_retain(rt)` / `varn_runtime_release(rt)` | Hold the event loop open so the runtime waits for events instead of exiting |
| `int varn_runtime_run_file(rt, path)` | Load and run a Lua file, then pump the event loop until it is idle |
| `int varn_runtime_load_file(rt, path)` / `varn_runtime_load_string(rt, source, name)` | Run a chunk and hand control straight back, leaving the event loop to the host |
| `int varn_runtime_poll(rt)` | Advance the runtime once without blocking, answering 1 while work remains |
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

### In the browser

The WebAssembly build exposes the same bridge to JavaScript, so a page is a host like any other:

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

The function `varnRunChunk` is still there and still runs a chunk to completion, which is what the playground uses. The pair `varnLoadChunk` plus `varnPoll` is the shape an interface wants.

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

A failure that no caller can receive, a task of `async.spawn` or `async.run` that raises or an ffi callback that fails outside any ffi call, goes to the one handler an application sets from Lua with `async.onFailure(handler)`, together with its traceback as text and as a list of frames. That handler is the one way a host learns of such failures, so an engine that shows them sets it in its first chunk. Without a handler, the runtime logs the failure with its traceback and stops the loop.

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
