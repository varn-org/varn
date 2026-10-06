# 🛠️ Building and running

The script `varn.py` is the entry point. Run `python3 varn.py <task> --help` for a task's options.

## 🚀 Build and run

```bash
python3 varn.py build
./build/bin/varn apps/lua/server.lua
```

Then open <http://localhost:3000>.

## 🖥️ The executable

```
varn <script.lua> [arguments...]   Run a script
varn -e <source> [arguments...]    Run an inline source string
varn --version                     Print the version
varn --help                        Print this message
```

Arguments after the script reach it as `arg[1]`, `arg[2]` and so on, with the script itself as `arg[0]`. A `VARN_WORKERS` value above `1` supervises that many worker processes, each running the same script. A value that is not a whole number from `1` to `1024` stops the program with a message that names it.

## 📦 Installers

A desktop build also produces the installer for its platform, which is what puts `varn` on the `PATH`:

```bash
python3 varn.py build
cpack --config build/CPackConfig.cmake -B build/pkg
```

| Platform | Package | Lands in |
|---|---|---|
| macOS | `productbuild` `.pkg` | `/usr/local/bin`, already on the `PATH` |
| Linux | `.deb` | `/usr/bin`, already on the `PATH` |
| Windows | NSIS `.exe` | The chosen folder, which the installer adds to the `PATH` |

The macOS installer uses a product archive rather than a disk image on purpose. A `.dmg` only copies files across and has no install step, so it cannot place anything on the `PATH`.

Releases carry these alongside the plain archives, so nobody has to build to get a working `varn` command.

## 📦 Targets

| Command | What it builds |
|---------|----------------|
| `python3 varn.py build` | The desktop app |
| `python3 varn.py apple` | The Apple build (iOS, tvOS, watchOS, visionOS, macOS) |
| `python3 varn.py android` | The Android build |
| `python3 varn.py wasm` | The browser build |
| `python3 varn.py app-wasm` | The browser build bundled into `apps/wasm/dist` |
| `python3 varn.py serve` | The browser demo dev server |
| `python3 varn.py lib` | The embeddable `varn` shared library (`find_package(varn)` package) |
| `python3 varn.py format` | Run `clang-format` over the sources |
| `python3 varn.py clean` | Remove the build directory |
| `python3 varn.py zip` | Create a source archive |

The command `python3 varn.py apple` builds every slice of `varn.xcframework` and assembles it. With `--family` it builds only the slices of one family, such as `ios`, which is the device and the Apple silicon simulator of iOS, and with `--assemble` it joins the slices that the families built into the XCFramework. The families live next to the slices in `tools/core/targets/apple.py`.

## ⚙️ Backends

Each module picks an implementation at configure time. Pass overrides through `build` with `-D`, for example `python3 varn.py build -D VARN_LOG_DRIVER=STDOUT -D VARN_ENABLE_TLS=OFF`. The `DUMMY` backend keeps the module loadable but makes its calls return a clear "not available" error, which is what the browser and reduced platforms use. The `EXTERNAL` backend compiles the sources that the host names in `VARN_<MODULE>_DRIVER_SOURCES`, with the libraries of `VARN_<MODULE>_DRIVER_LIBRARIES`, which implement the public header of the module that every backend implements, such as `varn/socket/SocketTransport.h` or `varn/process/ProcessRunner.h`, so a host brings the transport of a system that Varn does not know.

| Option | Values (default first) | Selects |
|--------|------------------------|---------|
| `VARN_HTTP_SERVER_DRIVER` | `POCO`, `DUMMY` | Web server transport |
| `VARN_HTTP_CLIENT_DRIVER` | `POCO`, `APPLE`, `ANDROID`, `EMSCRIPTEN_FETCH`, `DUMMY`, `EXTERNAL` | HTTP client transport (`APPLE` and `ANDROID` are selected automatically by their targets and are described under [Platform networking](#-platform-networking)) |
| `VARN_SOCKET_DRIVER` | `POCO`, `DUMMY`, `EXTERNAL` | TCP and UDP sockets |
| `VARN_CRYPTO_DRIVER` | `OPENSSL`, `PORTABLE`, `DUMMY`, `EXTERNAL` | Crypto primitives (`PORTABLE` is dependency-free and offers digest/hmac/random/uuid only. It backs the browser build) |
| `VARN_JSON_DRIVER` | `NLOHMANN`, `DUMMY` | JSON serializer |
| `VARN_XML_DRIVER` | `PUGIXML`, `DUMMY` | XML serializer |
| `VARN_LOG_DRIVER` | `SPDLOG`, `STDOUT`, `DUMMY` | Log backend |
| `VARN_FS_DRIVER` | `STD`, `DUMMY`, `EXTERNAL` | Filesystem storage |
| `VARN_FFI_DRIVER` | `LIBFFI`, `DUMMY`, `EXTERNAL` | Native function calls |
| `VARN_FFI_CLOSURES` | `ON`, `OFF` | Callbacks of Lua functions for `LIBFFI`, whose closures write code into memory where the platform has no trampolines of its own. A platform that forbids executable memory turns it off, and `ffi.cast` of a Lua function then fails with a sentence that says why |
| `VARN_PROCESS_DRIVER` | `POSIX`, `WINDOWS`, `DUMMY`, `EXTERNAL` | Running commands and reading the environment |
| `VARN_EVENT_LOOP_DRIVER` | `LIBUV`, `PUMP` | The event loop: `LIBUV` waits on the system and watches sockets, while with `PUMP` the host pumps the loop from its own run loop and the loop watches no socket, as the browser and ports do. `PUMP` takes no backend of `socket` or `http` that waits on the network, and the host counts the work it completes outside the queue, such as a fetch, with `varn::runtime::HostWork::begin` and `HostWork::end`. A build with `PUMP` refuses `varn_runtime_run_file` and `varn_runtime_run_string`, since its loop runs only under the polls of its host |
| `VARN_THREADS` | `ON`, `OFF` | The worker pools on threads of their own, with either event loop. A system without threads, such as the browser, turns it off, which only `PUMP` allows, and the pump then runs the jobs of the pools itself |
| `VARN_ENABLE_TLS` | `ON`, `OFF` | TLS for HTTP and sockets, the NetSSL of Poco over OpenSSL, which pulls in both on its own, independent of `VARN_CRYPTO_DRIVER`, and which a host also uses for connections of its own, such as WebSockets, with either event loop |
| `VARN_NO_SENDFILE` | `OFF`, `ON` | Disables the zero-copy `sendfile` fast path so the HTTP server serves files portably |

OpenSSL compiles with its own build system, so Varn hands its `Configure` the compiler launcher that CMake is given in `CMAKE_C_COMPILER_LAUNCHER`, or in the environment variable of the same name, whatever tool it names, such as a compiler cache, and a change of launcher configures OpenSSL again. Under MSVC, whose makefile quotes the compiler as one program, the launcher goes into the compiler macro of the command line of `nmake` instead, and OpenSSL builds through a target that keeps the debug information inside every object, since no compiler cache restores a shared PDB. One `nmake` builds the generated files and then the libraries with that macro, since the target `build_libs` would build the libraries in another `nmake`, which takes the compiler of the makefile again, and the build skips the dependency scan of MSVC with `no-makedepend`, which compiles every source a second time only to list its headers, while the sources of a release of OpenSSL never change in place. On Android, whose configuration of OpenSSL derives the compiler of the NDK itself, Varn puts the launcher in front of that compiler through a target that inherits it.

Poco builds with the patch `cmake/patches/poco-close-notify.patch`, which CPM applies with the program `patch` of the host, the one of Git on Windows. A non-blocking TLS socket of Poco marks its close_notify alert sent even when the socket had no room for it, so without the patch a later shutdown does not send it and the peer sees the connection end without the end of the session. The patch lets the next shutdown send that alert, which the HTTP server needs to end the session of a connection whose socket is full before it closes it. The source cache keeps the patched tree of Poco under a key made of the hash of its archive and the contents of the patch, so an edited patch reaches every checkout.

The cache path `VARN_PACKAGE_CACHE`, which the environment variable of the same name sets, names a folder of prebuilt packages of OpenSSL, the one dependency that builds with a build system of its own. Each package holds the public headers and the static libraries of one build, in a folder named after the hash of everything that build depends on: the release of OpenSSL and its recipe, the target, the options and the configuration file of `Configure` without the launcher, the build options, the compiler with its version and target, the systems of the host and the target, and the variables of the environment that `Configure` reads. A tree whose hash names a package of the folder takes its headers and libraries through the same imported targets `OpenSSL::Crypto` and `OpenSSL::SSL` and builds no OpenSSL, and a tree that builds OpenSSL puts its package there once the build ends, so every later tree of the same hash on the machine takes it. The file `inputs.txt` of a package lists what its hash covers. A package answers `find_package(OpenSSL)` itself, as a build of OpenSSL does, through the files `openssl-config.cmake` and `openssl-config-version.cmake` that it writes into the folder `CMAKE_FIND_PACKAGE_REDIRECTS_DIR`, so a dependency that looks for OpenSSL, such as Poco, takes the package and never an OpenSSL of the system, which the module `FindOpenSSL` of MSVC would search for. The project `tests/cmake/openssl-package` checks a package that way, and the Windows workflow runs it on every package that its build made or took. Without the variable, every tree builds its own OpenSSL.

```bash
export VARN_PACKAGE_CACHE="$HOME/.cache/varn-packages"
cmake -B build/release -DCMAKE_BUILD_TYPE=Release
```

## 📱 Platform networking

On the mobile targets the HTTP client runs on the operating system's own networking stack instead of a transport bundled with the engine, so an application steers it the way it steers any other library it uses. Both drivers are selected by their target and need no configuration.

| Target | Driver | Stack | Steered by |
|--------|--------|-------|------------|
| `apple` | `APPLE` | `NSURLSession` | `Info.plist` — App Transport Security, exception domains, the minimum TLS version |
| `android` | `ANDROID` | `HttpURLConnection` | `network_security_config.xml` — Trust anchors, certificate pinning, the cleartext policy |

The trust store, the system proxy and HTTP/2 come from the platform on both, so a device profile installed by an MDM or a user is honoured without the engine shipping a certificate bundle of its own. The Lua API does not change. A call to `require("http")` behaves the same on every target, and a redirect is still handed to the caller rather than followed, which is what every other transport the engine ships does.

## 🌍 Browser demo

```bash
python3 varn.py serve
```

This builds the browser version and opens the demo.

## 🖥️ Platforms

The same project builds for Linux, macOS, Windows, iPhone, Android, and the browser, with the same features wherever they are available.

Some features are not available everywhere. In the browser there is no built-in web server or raw socket, and a few platforms run a reduced set of features. When a feature is not available on a platform, it still loads, and using it returns a clear error.

## 🧪 Continuous integration

Every workflow under `.github/workflows` builds and tests one system: `Linux`, `macOS`, `Windows`, `Apple`, `Android` and `WebAssembly`, and `Apps` builds the sample apps against a released engine. The workflow `Release` runs the build workflows for a version tag and publishes their packages.

The Linux workflow builds the program on x86_64 and arm64 with warnings as errors and runs the Lua tests and the C++ tests, and it runs the sanitizers: the Lua tests and the C++ tests under AddressSanitizer with UndefinedBehaviorSanitizer and LeakSanitizer, the C++ tests under ThreadSanitizer, the tests of the pumped event loop of the target `port` with threads under ThreadSanitizer and without threads, and the portable crypto driver. The macOS and Windows workflows build and test the program on their processors, and the macOS build for Intel is a cross build that runs no test.

The Apple workflow builds the slices of `varn.xcframework` in one job for each family, the job `Apple slices`, and the job `Apple XCFramework` assembles the XCFramework from what they built. A family is two slices: the device and the Apple silicon simulator of iOS, tvOS, watchOS and visionOS, the Intel simulators of iOS and tvOS, and both processors of macOS.

### The compiler cache

Every compile of every job goes through sccache, which keeps the objects in the Cloudflare R2 bucket `varn-ci` and reaches it through the S3 API of R2 with the region `auto`. The workflows hand sccache three secrets of the repository: `VARN_R2_ENDPOINT`, the address of the S3 API of the account, and `VARN_R2_KEY_ID` and `VARN_R2_SECRET`, the token that reads and writes the buckets of Varn. Every run of a push or of a branch of the repository reads and writes the cache, so the first job that compiles an object stores it and every later job, in the same run or another, downloads it. A pull request from a fork gets no secrets, so its jobs compile without the cache, and nothing else differs for them. The bucket deletes an object of any prefix 30 days after it was stored and an upload that never finished after a day, so the cache never needs cleaning.

The action `.github/actions/build-environment`, which every job that compiles starts with, installs sccache and points the variables `CMAKE_<LANG>_COMPILER_LAUNCHER` of C, C++, Objective-C and Objective-C++ at it. So Clang on Linux and macOS, MSVC on Windows, the toolchains of Xcode for every Apple slice, the Clang of the Android NDK inside the build of Gradle and Emscripten all compile through it, and OpenSSL takes the same launcher in its own build. The cache cannot hide a failure: sccache finds an object by the hash of the compiler, its options and the preprocessed source, so every changed source compiles again, and every link and test runs on every run. The compiler of Emscripten is a script that stays the same from one version to the next, so the jobs that compile with it put the version of Emscripten into the hash of every object through `SCCACHE_C_CUSTOM_CACHE_BUSTER`.

The other caches live in the same bucket, never in the cache of GitHub, as Zstandard archives of tar that the AWS CLI of the runner moves with the token of the compiler cache. The bucket keeps one layer under each prefix, so each one can be listed, measured and expired on its own: the objects of sccache under `sccache/`, which `SCCACHE_S3_KEY_PREFIX` names, the sources of the dependencies, their prebuilt packages and what Gradle downloads under `dependencies/`, and the SDK of Emscripten under `tools/`. The jobs that build OpenSSL point `VARN_PACKAGE_CACHE` at `.cache/packages` and keep that folder under a key of the name of the job, the system, the version of each compiler of the runner and the hash of the files that pin the dependencies, so a run builds OpenSSL only when one of those changes. The sources of the dependencies are the folder that `CPM_SOURCE_CACHE` names, under a key of the system and the hash of the files that pin the dependencies. What Gradle downloads, its distribution and the dependencies of the Android library, is the folder `.gradle-home` that `GRADLE_USER_HOME` names, under a key of the system and the hash of the files of Gradle. The SDK of Emscripten is the folder `.tools/emsdk`, under a key of the system and its version, and the action installs it from the release of that version when the bucket lacks it. In the runs of the default branch the job that saves a cache does it with the action `.github/actions/caches` at its end, once the job succeeded with a key that the bucket lacks: the Linux build for x86_64, the macOS build for arm64 and the Windows build for x86_64 save the sources, every job that builds OpenSSL its packages, the Android build the downloads of Gradle and the WebAssembly build Emscripten, and every other job reads them and fetches what only its platform needs. An answer of the bucket other than a missing key stops the job, and a pull request from a fork downloads everything from its projects. No workflow caches a build tree.

The action sets `CMAKE_BUILD_PARALLEL_LEVEL` to the processors of the runner, so every build of the job, and the build of OpenSSL with its own make, runs that many jobs. Every job ends with the action `.github/actions/compiler-cache`, whose step `Report the compiler cache` prints how many compiles came from the cache, how many compiled and why sccache could not cache the others, since the log of a build names every compile it starts, whether its object comes from the cache or not.

