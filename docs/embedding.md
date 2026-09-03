# Embedding varn in another project

Varn is a fast C++ core that runs Lua and is exposed to host programs as a small C library. The same Lua runs on desktop, iOS, Android, and the browser (wasm). This guide shows how another CMake project links varn, how the host and Lua talk to each other, and how you would drive native mobile UI from one shared Lua script.

Varn is **headless**: it ships modules for http, sockets, filesystem, crypto, json, process, and so on, but **no UI toolkit**. Screens and widgets are provided by the host as *host functions* backed by native views, described below.

## The C API

The whole embedding surface is `varn/varn.h`:

| Function | Purpose |
| --- | --- |
| `varn_runtime* varn_runtime_new(void)` | create a runtime (its own Lua state, event loop, and worker pools) |
| `int varn_runtime_register(rt, name, fn, userdata)` | expose a native function to Lua as `host.<name>` (call before running a chunk) |
| `int varn_runtime_emit(rt, name, json_argument)` | deliver an event to the Lua handlers registered for `name` with `host.on`, from any thread |
| `int varn_runtime_retain(rt)` / `varn_runtime_release(rt)` | hold the event loop open so the runtime waits for events instead of exiting |
| `int varn_runtime_run_file(rt, path)` | load and run a Lua file, then pump the event loop until it is idle |
| `int varn_runtime_run_string(rt, source, chunk_name)` | run Lua from a string |
| `void varn_runtime_stop(rt)` | ask a running runtime to stop |
| `void varn_runtime_free(rt)` | destroy the runtime (joins its threads) |
| `const char* varn_version(void)` | the library version string |

Return codes: `0` success, `1` a load/run error, `2` a bad argument.

## Adding varn to a CMake project

### Option A — installed package (`find_package`)

Build varn as a self-contained shared library and install it once:

```sh
python3 varn.py lib --prefix /opt/varn --install
```

which wraps the plain CMake flow:

```sh
cmake -B build/lib -S path/to/varn \
  -DVARN_TARGET=lib -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/varn
cmake --build build/lib
cmake --install build/lib --component varn   # library, header and cmake package only
```

`VARN_TARGET=lib` produces a `varn` shared library that links every dependency (Lua, Poco, OpenSSL, libuv, …) **privately** and exports only the C API, plus a `find_package` package. Consume it:

```cmake
find_package(varn REQUIRED)

add_executable(app main.c)
target_link_libraries(app PRIVATE varn::varn)   # #include <varn/varn.h>
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

### Option C — prebuilt mobile artifacts

- **iOS / macOS**: `python3 varn.py apple` builds `varn.xcframework` (device + simulator) exposing `varn/varn.h`. Add it to an Xcode target or a Swift package.
- **Android**: `python3 varn.py android` builds an `.aar` (`libvarn.so` plus the JNI bindings under `android/varn/`).

## Exposing native capabilities to Lua

`varn_runtime_register` binds a C callback to a name inside the global Lua table `host`. When Lua calls `host.<name>(value)`, varn serializes the argument to JSON, hands that string to your callback, and decodes the JSON your callback returns back into a Lua value.

```c
#include <varn/varn.h>
#include <stdio.h>

static const char* greet(const char* json_argument, void* userdata)
{
    (void)userdata;
    printf("[host] %s\n", json_argument);       // e.g. {"name":"world"}
    return "{\"message\":\"hello from the host\"}";
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

`varn_runtime_register` is one direction: Lua calls the host. `varn_runtime_emit` is the other, and it is what a user interface needs — a tap happens on the host side and has to reach the script.

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

`varn_runtime_run_file` and `varn_runtime_run_string` pump the loop until it is idle and then return. A script that only subscribes to events has nothing pending, so the runtime would finish immediately. `varn_runtime_retain` holds the loop open until the matching `varn_runtime_release`:

```c
varn_runtime_retain(rt);              /* before running, so the loop never sees itself idle */
varn_runtime_run_string(rt, script, "=app");   /* returns when the host releases */
```

Run it on a thread of your own: it blocks for as long as the runtime lives, and the emitting side stays free.

Retain and release are safe from any thread, and a retain taken while the loop is already running is never lost to the loop's own decision to exit. The two must balance, so `varn_runtime_release` answers non-zero when there is no retain left to give back. That answer is worth checking, because a host that releases more than it retained has a bug that would otherwise surface as a runtime exiting earlier than it should.

A runtime is not single-shot. Each `varn_runtime_run_file` or `varn_runtime_run_string` gets the event loop and its own exit code, so a host can load one chunk and then run another on the same runtime, keeping every host function, subscription and piece of Lua state the earlier chunk left behind.

## Driving native UI from one Lua script

The pattern for a mobile app whose screens are written in Lua and rendered with the platform's native widgets:

1. the host registers UI primitives (`ui_screen`, `ui_label`, `ui_button`, …) as host functions, each backed by a real `UIView` on iOS or `View` on Android.
2. one shared `app.lua` describes the screens by calling those primitives — it is byte-for-byte the same on both platforms.
3. a native event (a tap) runs a Lua handler, so behaviour also lives in Lua.

Varn does not provide these primitives — you write the thin native bridge once per platform. What is identical is the Lua.

### The shared screen script (`app.lua`, same on iOS and Android)

```lua
local home = host.ui_screen({ title = "Home" })
host.ui_label({ screen = home, text = "Welcome to varn" })
host.ui_button({ screen = home, id = "tap", text = "Tap me" })
host.ui_present({ screen = home })

-- event handlers the host dispatches to on a native tap
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

        // register a native-backed primitive; the C callback reaches `self` through userdata
        varn_runtime_register(runtime, "ui_screen", { json, ud in
            let host = Unmanaged<VarnHost>.fromOpaque(ud!).takeUnretainedValue()
            return host.makeScreen(fromJSON: String(cString: json!))
        }, me)

        varn_runtime_register(runtime, "ui_button", { json, ud in
            let host = Unmanaged<VarnHost>.fromOpaque(ud!).takeUnretainedValue()
            return host.makeButton(fromJSON: String(cString: json!))
        }, me)
        // ...ui_label, ui_present, ui_alert the same way

        varn_runtime_run_file(runtime, Bundle.main.path(forResource: "app", ofType: "lua"))
    }

    func makeScreen(fromJSON json: String) -> UnsafePointer<CChar> {
        // decode {title=...}, build a UIViewController with native UIKit views, keep it in `screens`
        // return a json handle the Lua side stores, e.g. {"id":"home"}
        return staticCString("{\"id\":\"home\"}")
    }

    // on a real UIButton tap, dispatch back into Lua:
    @objc func buttonTapped(id: String) {
        varn_runtime_run_string(runtime, "onTap('\(id)')", "tap")
    }
}
```

### Android (Kotlin + a small C++/JNI shim) bridge

Kotlin cannot hand a raw C function pointer to `varn_runtime_register`, so the registration lives in the JNI layer, which calls up into Kotlin:

```cpp
// jni_bridge.cpp — registered once from JNI_OnLoad / a native init method
static JavaVM* gVm;
static jobject gHostUi;   // a global ref to the Kotlin object that owns the native Views

static const char* ui_screen(const char* json, void*) {
    JNIEnv* env; gVm->AttachCurrentThread(&env, nullptr);
    jstring arg = env->NewStringUTF(json);
    jstring res = (jstring) env->CallObjectMethod(gHostUi, gMakeScreen, arg);
    static thread_local std::string out;         // outlives the return, copied by varn immediately
    const char* c = env->GetStringUTFChars(res, nullptr);
    out = c; env->ReleaseStringUTFChars(res, c);
    return out.c_str();
}

extern "C" JNIEXPORT void JNICALL Java_com_varn_VarnRuntime_registerUi(JNIEnv*, jobject, jlong rt) {
    varn_runtime_register(reinterpret_cast<varn_runtime*>(rt), "ui_screen", &ui_screen, nullptr);
    // ...ui_button, ui_label, ui_present, ui_alert
}
```

```kotlin
class HostUi(private val activity: Activity) {
    // called from native ui_screen; builds real android Views and returns a json handle
    fun makeScreen(json: String): String {
        // parse {title=...}, inflate/compose native Views, remember the screen
        return "{\"id\":\"home\"}"
    }
    // on a real button tap: VarnRuntime.runString("onTap('tap')")  -> dispatches into Lua
}
```

The screen logic (`app.lua`) is shared. Only `makeScreen`/`makeButton`/… differ, and each builds the platform's own native components, so the app looks and feels native on both while the code you write is Lua.

### Notes and current limits

- There is no bundled widget set, layout engine, or navigation — those are yours to expose. Varn provides the runtime, the Lua ↔ native bridge, and everything non-visual (networking, storage, crypto, json, scheduling).
