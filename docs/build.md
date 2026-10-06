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

Arguments after the script reach it as `arg[1]`, `arg[2]` and so on, with the script itself as `arg[0]`. A `VARN_WORKERS` value above `1` supervises that many worker processes, each running the same script.

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

## ⚙️ Backends

Each module picks an implementation at configure time. Pass overrides through `build` with `-D`, for example `python3 varn.py build -D VARN_LOG_DRIVER=STDOUT -D VARN_ENABLE_TLS=OFF`. The `DUMMY` backend keeps the module loadable but makes its calls return a clear "not available" error, which is what the browser and reduced platforms use.

| Option | Values (default first) | Selects |
|--------|------------------------|---------|
| `VARN_HTTP_SERVER_DRIVER` | `POCO`, `DUMMY` | Web server transport |
| `VARN_HTTP_CLIENT_DRIVER` | `POCO`, `APPLE`, `ANDROID`, `EMSCRIPTEN_FETCH`, `DUMMY` | HTTP client transport (`APPLE` and `ANDROID` are selected automatically by their targets and are described under [Platform networking](#-platform-networking)) |
| `VARN_SOCKET_DRIVER` | `POCO`, `DUMMY` | TCP and UDP sockets |
| `VARN_CRYPTO_DRIVER` | `OPENSSL`, `PORTABLE`, `DUMMY` | Crypto primitives (`PORTABLE` is dependency-free and offers digest/hmac/random/uuid only. It backs the browser build) |
| `VARN_JSON_DRIVER` | `NLOHMANN`, `DUMMY` | JSON serializer |
| `VARN_XML_DRIVER` | `PUGIXML`, `DUMMY` | XML serializer |
| `VARN_LOG_DRIVER` | `SPDLOG`, `STDOUT`, `DUMMY` | Log backend |
| `VARN_FS_DRIVER` | `STD`, `DUMMY` | Filesystem storage |
| `VARN_FFI_DRIVER` | `LIBFFI`, `DUMMY` | Native function calls |
| `VARN_ENABLE_TLS` | `ON`, `OFF` | TLS for HTTP and sockets (pulls in OpenSSL on its own, independent of `VARN_CRYPTO_DRIVER`) |
| `VARN_NO_SENDFILE` | `OFF`, `ON` | Disables the zero-copy `sendfile` fast path so the HTTP server serves files portably |

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
