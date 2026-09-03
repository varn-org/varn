# 🛠️ Building and running

`varn.py` is the entry point. Run `python3 varn.py <task> --help` for a task's options.

## 🚀 Build and run

```bash
python3 varn.py build
./build/bin/varn apps/lua/server.lua
```

Then open <http://localhost:3000>.

## 🖥️ The executable

```
varn <script.lua> [arguments...]   run a script
varn -e <source> [arguments...]    run an inline source string
varn --version                     print the version
varn --help                        print the usage
```

Arguments after the script reach it as `arg[1]`, `arg[2]` and so on, with the script itself as `arg[0]`. `VARN_WORKERS` above `1` supervises that many worker processes, each running the same script.

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
| Windows | NSIS `.exe` | the chosen folder, which the installer adds to the `PATH` |

macOS uses a product archive rather than a disk image on purpose: a `.dmg` only copies files across and has no install step, so it cannot place anything on the `PATH`.

Releases carry these alongside the plain archives, so nobody has to build to get a working `varn` command.

## 📦 Targets

| command | what it builds |
|---------|----------------|
| `python3 varn.py build` | the desktop app |
| `python3 varn.py apple` | the Apple build (iOS, tvOS, watchOS, visionOS, macOS) |
| `python3 varn.py android` | the Android build |
| `python3 varn.py wasm` | the browser build |
| `python3 varn.py app-wasm` | the browser build bundled into `apps/wasm/dist` |
| `python3 varn.py serve` | the browser demo dev server |
| `python3 varn.py lib` | the embeddable `varn` shared library (`find_package(varn)` package) |
| `python3 varn.py format` | run clang-format over the sources |
| `python3 varn.py clean` | remove the build directory |
| `python3 varn.py zip` | create a source archive |

## ⚙️ Backends

Each module picks an implementation at configure time. Pass overrides through `build` with `-D`, for example `python3 varn.py build -D VARN_LOG_DRIVER=STDOUT -D VARN_ENABLE_TLS=OFF`. The `DUMMY` backend keeps the module loadable but makes its calls return a clear "not available" error, which is what the browser and reduced platforms use.

| Option | Values (default first) | Selects |
|--------|------------------------|---------|
| `VARN_HTTP_SERVER_DRIVER` | `POCO`, `DUMMY` | web server transport |
| `VARN_HTTP_CLIENT_DRIVER` | `POCO`, `APPLE`, `ANDROID`, `EMSCRIPTEN_FETCH`, `DUMMY` | http client transport (`APPLE` and `ANDROID` are selected automatically by their targets and are described under [Platform networking](#-platform-networking)) |
| `VARN_SOCKET_DRIVER` | `POCO`, `DUMMY` | tcp and udp sockets |
| `VARN_CRYPTO_DRIVER` | `OPENSSL`, `PORTABLE`, `DUMMY` | crypto primitives (`PORTABLE` is dependency-free and offers digest/hmac/random/uuid only. It backs the browser build) |
| `VARN_JSON_DRIVER` | `NLOHMANN`, `DUMMY` | json serializer |
| `VARN_XML_DRIVER` | `PUGIXML`, `DUMMY` | xml serializer |
| `VARN_LOG_DRIVER` | `SPDLOG`, `STDOUT`, `DUMMY` | log backend |
| `VARN_FS_DRIVER` | `STD`, `DUMMY` | filesystem storage |
| `VARN_FFI_DRIVER` | `LIBFFI`, `DUMMY` | native function calls |
| `VARN_ENABLE_TLS` | `ON`, `OFF` | TLS for http and sockets (pulls in OpenSSL on its own, independent of `VARN_CRYPTO_DRIVER`) |
| `VARN_NO_SENDFILE` | `OFF`, `ON` | disables the zero-copy `sendfile` fast path so the http server serves files portably |

## 📱 Platform networking

On the mobile targets the HTTP client runs on the operating system's own networking stack instead of a transport bundled with the engine, so an application steers it the way it steers any other library it uses. Both drivers are selected by their target and need no configuration.

| Target | Driver | Stack | Steered by |
|--------|--------|-------|------------|
| `apple` | `APPLE` | `NSURLSession` | `Info.plist` — App Transport Security, exception domains, the minimum TLS version |
| `android` | `ANDROID` | `HttpURLConnection` | `network_security_config.xml` — trust anchors, certificate pinning, the cleartext policy |

The trust store, the system proxy and HTTP/2 come from the platform on both, so a device profile installed by an MDM or a user is honoured without the engine shipping a certificate bundle of its own. The Lua API does not change. `require("http")` behaves the same on every target, and a redirect is still handed to the caller rather than followed, which is what every other transport the engine ships does.

## 🌍 Browser demo

```bash
python3 varn.py serve
```

This builds the browser version and opens the demo.

## 🖥️ Platforms

The same project builds for Linux, macOS, Windows, iPhone, Android, and the browser, with the same features wherever they are available.

Some features are not available everywhere: in the browser there is no built-in web server or raw socket, and a few platforms run a reduced set of features. When a feature is not available on a platform, it still loads, and using it returns a clear error.
