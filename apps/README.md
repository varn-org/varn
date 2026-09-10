# 📱 Apps

Four front ends over the same engine, each one a playground: pick a sample, edit the Lua, run it, read what it printed.

| App | What it is |
|---|---|
| [wasm/](wasm/) | The site behind [varn.pages.dev](https://varn.pages.dev), running the engine compiled to WebAssembly in a worker. |
| [android/](android/) | A Kotlin app linking the released `.aar`. |
| [ios/](ios/) | A Swift app in UIKit linking the released `.xcframework`. |
| [lua/](lua/) | The static files the `http` examples and the benchmark serve. |

## Shared samples

[samples/](samples/) holds the Lua the native apps offer, as one `.lua` file per sample plus a `manifest.json` naming and grouping them. Android reaches it through an asset source directory and iOS through a folder reference, so a sample is written once and both apps ship it.

The web playground keeps its own set, because the browser build carries less than a native one — no sockets, no subprocesses, no FFI — and its samples are chosen to match.

## Getting the engine

Neither native app builds the engine. Both link what a release already published:

```bash
python3 varn.py fetch-native                    # both, from the latest release
python3 varn.py fetch-native --platform ios --version v0.0.1
```

That drops the `.aar` into `android/app/libs/` and the `.xcframework` into `ios/Frameworks/`. Neither is committed.

## Running them

```bash
# android
cd apps/android && ./gradlew installDebug

# ios
cd apps/ios && xcodegen generate && open Varn.xcodeproj
```

The Xcode project is generated from [ios/project.yml](ios/project.yml) rather than committed, so it never conflicts. Install XcodeGen with `brew install xcodegen`.

## How the console works

The C API answers a run with an exit code, not with what the script printed. Both apps therefore wrap the chunk in a small Lua harness that replaces `print` with one writing to a file, run that, and read the file back. The harness also catches a Lua error and writes it as an `error:` line, which is what the console tints red.

Because `run_string` drives the event loop before returning, a sample that uses `async` has already finished by the time the app reads the file.
