# 🧭 Platform availability

Every module is selected through a driver in its `<module>.cmake`. A module that cannot work on a
target falls back to a **dummy** driver that still compiles and links, so a script always loads; calling
an unavailable feature raises a clear error instead of failing the build. This page is the source of
truth for what is real where.

## Matrix

Legend: ✅ native · 🌐 browser-equivalent · ❌ dummy (compiles, raises at call) · — impossible on the platform

| Module | Linux | macOS | Windows | iOS | tvOS / watchOS / visionOS | Android | wasm |
|---|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| `api` / `platform` / `async` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `datetime` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `json` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `xml` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `log` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `fs` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `zip` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| `crypto` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | 🌐 essentials |
| `http` client | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | 🌐 fetch |
| `http` server | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | — |
| `socket` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | — |
| `process` | ✅ | ✅ | ✅ | ❌ | ❌ | ✅ | — |
| `ffi` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | — |

## Why the gaps exist

- **`process`** runs a command through a shell. On Windows it uses Win32 `CreateProcess`; elsewhere it
  forks and execs a shell. Among Apple targets only macOS permits that, so iOS, tvOS, watchOS, and
  visionOS use the dummy. Android execs `/system/bin/sh` instead of `/bin/sh`. The browser has no
  process model at all.
- **`socket`** and the **`http` server** need raw TCP and the ability to host a listener, which a browser
  page cannot do. Native platforms all have them.
- **`ffi`** calls into native libraries through libffi; wasm has no native calling convention to target.
- **`crypto`** is backed by OpenSSL on native platforms. In the browser it exposes the **essential**
  subset (base64/hex, UUIDs, SHA digests, HMAC, secure random) without pulling OpenSSL into the bundle.
  AES-GCM, scrypt, and PBKDF2/HKDF stay native-only.

## wasm specifics

The browser build forces the wasm-capable driver set (`http` client over `fetch`, dummy server and
socket) and pumps the event loop manually, so timers and promises still work. `fs` runs against the
in-memory virtual filesystem. Everything marked ✅ or 🌐 above is exercised in the
[wasm demo](../apps/wasm), grouped per module so each can be tried in isolation.
