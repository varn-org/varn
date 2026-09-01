# 🐳 Docker

A small production base image for Varn apps lives at [docker/Dockerfile](../docker/Dockerfile). It is
built the same way the official `python:slim` and `node:slim` images are: a heavy builder stage compiles
the release binary, and a slim runtime stage ships only the stripped binary — the toolchain and the
dependency sources never reach the final image.

The binary is **statically linked**, so the whole Varn core is built in: the HTTP server and client,
TCP/TLS/UDP sockets, the event loop, crypto and TLS (OpenSSL), zip (libzip + zlib), ffi (libffi), JSON,
XML, datetime, logging, and the rest. The runtime image carries only the binary (~12 MB) plus the few
shared libraries it links (`libstdc++`, `libgcc`, `libm`, `libc`) and `ca-certificates` for outbound
TLS — nothing else, **around 35 MB in total** (smaller than `python:slim` or `node:slim`). `docker
image inspect varn:latest` reports that real size; the `docker images` column may show a larger
shared-layer disk figure.

> The image covers the **core**. Pure-Lua [components](https://github.com/varn-org/components) like `vdo` (which loads a database
> client through ffi) or `redis` are your choice per app, so their runtime dependencies are not bundled —
> add them in your own image when you use them.

## Build the base image

```bash
docker build -t varn:latest -f docker/Dockerfile .
```

## Use it as a base for your app

Base your app image on it, drop in your Lua script, and run it:

```dockerfile
FROM varn:latest
COPY app.lua /app/app.lua
CMD ["app.lua"]
```

```bash
docker build -t my-varn-app .
docker run --rm -p 3000:3000 my-varn-app
```

The entrypoint is the `varn` binary, so the image arguments are passed straight to it — `CMD
["app.lua"]` runs `varn app.lua`. The container runs as a non-root `varn` user with `/app` as the
working directory.

A minimal `app.lua`:

```lua
local http = require("http")

http.createServer(function(req, res)
    res:json({ hello = req.query.name or "world" })
end):listen({ host = "0.0.0.0", port = 3000 })
```

## Sharing the image

The base image is built locally above. To share it across machines or a deployment, tag it for a
registry and push it — for example GitHub Container Registry:

```bash
docker tag varn:latest ghcr.io/<org>/varn:latest
docker push ghcr.io/<org>/varn:latest
```

Then teams base their apps on `FROM ghcr.io/<org>/varn:latest` without rebuilding the core.

## Benchmark stack

A separate, database-backed benchmark stack (MySQL + Redis behind Varn, Node, and Python) lives under
[bench/](../bench/) and is run with `python3 varn.py bench`; it is for measuring, not for production.
