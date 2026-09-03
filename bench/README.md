# Comparison benchmark — Varn vs Node vs Python

A fair, apples-to-apples comparison: the **same four routes** on three runtimes, driven by the **same `wrk` load**, with no web framework on any side.

| Route | What it measures |
|------------|------------------|
| `/plaintext` | raw HTTP throughput (a fixed string) |
| `/json` | per-request JSON serialization of `{"hello":"world"}` |
| `/db` | a random-row `SELECT` from MySQL over a pooled async connection |
| `/cache` | an `INCR` on Redis over a pooled async connection |

- **Varn** — `http.createServer`, vdo over libmysqlclient and the redis client over a shared connection `pool` (`servers/varn_server.lua`).
- **Node** — raw `http`, `mysql2` pool, `ioredis` (`servers/node_server.js`).
- **Python** — a raw ASGI app on `uvicorn`, `aiomysql` pool, `redis.asyncio` (`servers/asgi_server.py`).

Every runtime keeps its database and cache work non-blocking and pooled, so the numbers reflect each stack's async I/O and protocol handling, not a framework.

The Lua server uses the `vdo`, `pool` and `redis` components, which live in [varn-org/components](https://github.com/varn-org/components). The benchmark clones them into `.bench-components/` on first run, so nothing extra is needed.

## Architecture

Varn talks to MySQL through a **native protocol client written over the async socket** (no `libmysqlclient`): it authenticates with `mysql_native_password` and runs `COM_QUERY`, yielding on the event loop at every read so one process keeps many queries in flight, sharing a bounded set of connections through a generic [`pool`](https://github.com/varn-org/components/blob/main/pool) — the model `mysql2` and `aiomysql` use. Redis uses a single **multiplexed connection** ([`pipeline = true`](https://github.com/varn-org/components/blob/main/redis)) that auto-pipelines concurrent commands into one stream and demultiplexes the replies in order, the ioredis model, both built on an `async.deferred` primitive.

On an in-network Linux run (`-c256`, one process each) Varn **leads every route** — about 2.5× on `/plaintext` and `/json` from its C++ HTTP core, ~1.3× on `/db` from the non-blocking pooled MySQL client, and it edges out `ioredis` on `/cache` with the auto-pipelining redis client — all with far tighter tail latency, since no GC pauses the loop. Measure on a real network: a Docker-Desktop port-forward adds milliseconds per round trip and skews the database routes.

## Run with Docker (recommended, real network)

`docker-compose.yml` brings up MySQL (seeded with a 10k-row `world` table and a `mysql_native_password` user) and Redis, then builds Varn for Linux and drives all three servers on the same Docker network — no host port-forward in the path.

```bash
JOBS=4 CONNECTIONS=256 DURATION=10s WORKERS=1 bash bench/docker-bench.sh
```

## Run locally

Point the servers at a local or dockerized MySQL/Redis and drive them with `wrk` on the host. Needs `wrk`, a Varn build (`python3 varn.py build`), the node deps (`cd bench/servers && npm install`), and the python deps (`pip install uvicorn uvloop aiomysql redis`).

```bash
# bring up just the data services from the compose file
docker compose -f bench/docker-compose.yml up -d mysql redis

MYSQL_PORT=33061 REDIS_PORT=63791 THREADS=4 CONNECTIONS=200 DURATION=10s bash bench/run.sh
```

Knobs (env): `THREADS`, `CONNECTIONS`, `DURATION`, `WORKERS`, `POOL_SIZE`, `ROUTES`, `PORT_BASE`, `MYSQL_HOST`/`MYSQL_PORT`, `REDIS_HOST`/`REDIS_PORT`, `VARN`.

## Reading the results

`req/s` is throughput (higher is better). `lat(avg)`/`lat(p99)` are response latency (lower is better). Run the load generator on a **different machine** than the servers for accurate high-throughput numbers — sharing cores with `wrk` caps every runtime at the same ceiling.

Varn workers and the Node/uvicorn equivalents all rely on `SO_REUSEPORT`. The **Linux** kernel load-balances new connections across workers, so multi-core scaling is near-linear there, while **macOS/BSD** does not distribute accepts evenly, so a `WORKERS>1` run on a Mac understates every fork-based server — measure multi-core scaling on Linux.
