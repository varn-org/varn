# 🔥 Stress testing with k6

The tool [k6](https://k6.io) drives load against a running Varn HTTP server so you can measure throughput, latency, and where it starts to degrade. Varn does not bundle k6. You run it as a separate client against a server you start yourself.

## 📊 Varn vs Node vs Python

A reproducible comparison benchmark lives under [bench/](../bench/), with the same `/plaintext` and `/json` routes served by Varn, raw Node `http`, and a raw ASGI app on uvicorn — no framework on any side — driven by the same `wrk` load. Measured on Linux (the deploy target), requests per second:

| Scenario | Varn | Node | Python |
|----------|-----:|-----:|-------:|
| 1 core, plaintext | **147k** | 55k | 54k |
| 1 core, json | **120k** | 53k | 48k |
| 4 workers, plaintext | **452k** | 199k | 201k |
| 4 workers, json | **410k** | 184k | 186k |

Varn serves about **2.5× the requests of Node per core** (and ~2.5× Python) and scales near-linearly with `VARN_WORKERS` (147k → 452k across four cores). Each runtime is at its best with no framework. The Node baseline is raw `http`, and the Python one is a raw ASGI app on uvicorn with `uvloop`+`httptools`. Run it locally with `bash bench/run.sh`, or on Linux through Docker with `bash bench/docker-bench.sh`. A Mac's allocator and `kqueue` penalize every allocation-heavy server, so local macOS numbers understate production — measure on Linux.

## 🗄️ With a database and cache (MySQL + Redis)

A second, more real-world scenario adds two routes. The route `/db` reads a random row from MySQL and `/cache` runs a Redis `INCR`, each over a pooled, non-blocking connection. The reproducible stack is in [bench/](../bench/). The command `bash bench/docker-bench.sh` brings up MySQL (a seeded 10k-row table) and Redis and drives Varn, Node (`mysql2`/`ioredis`) and Python (`aiomysql`/`redis.asyncio`) on one network.

Varn reaches MySQL through a **native protocol client written over its async socket** — no `libmysqlclient` — authenticating with `mysql_native_password` and running `COM_QUERY`. Both MySQL and Redis go through a shared connection [`pool`](https://github.com/varn-org/components/blob/main/pool) that hands a free connection to each request and blocks the rest on an event-driven wait until one is released, so one process keeps many queries in flight. This mirrors the async, pooled model the Node and Python drivers use.

Measured on Linux (one process each, `wrk -t4 -c256`), requests per second:

| Scenario | Varn | Node | Python |
|----------|-----:|-----:|-------:|
| plaintext | **131k** | 50k | 48k |
| json | **109k** | 49k | 44k |
| db (MySQL `SELECT`) | **13.5k** | 10.6k | 10.7k |
| cache (Redis `INCR`) | **44.4k** | 35.0k | 15.7k |

Varn **leads every route**. On `/db` it reads MySQL through a wire-protocol client that stays non-blocking and pooled on the C++ event loop. On `/cache` the Redis client **auto-pipelines** concurrent commands onto one multiplexed connection (the [`pipeline = true`](https://github.com/varn-org/components/blob/main/redis) mode), so it edges out `ioredis`. Its **tail latency is in another class**. The `/db` p99 is 26 ms versus Node's 421 ms, and the `/cache` p99 is 8 ms versus Node's 72 ms and Python's 449 ms, because no GC ever pauses the event loop. Reproduce all of this with `python3 varn.py bench`.

> Run the bench on a real network, not a Docker-Desktop port-forward. The host→container proxy on macOS/Windows adds milliseconds per round trip and disproportionately penalizes whichever client does more round trips, which inverts the `/db` result. The numbers above are from the in-network Docker run.

## 📦 Install k6

```bash
# macOS
brew install k6

# Linux (Debian/Ubuntu)
sudo gpg -k && sudo gpg --no-default-keyring --keyring /usr/share/keyrings/k6-archive-keyring.gpg --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys C5AD17C747E3415A3642D57D77C6C491D6AC1D69
echo "deb [signed-by=/usr/share/keyrings/k6-archive-keyring.gpg] https://dl.k6.io/deb stable main" | sudo tee /etc/apt/sources.list.d/k6.list
sudo apt-get update && sudo apt-get install k6

# Windows
winget install k6 --source winget
```

## 🚀 Start a server to test

Build Varn and run one of the HTTP examples. The script `server_demo.lua` exposes a few routes that exercise different paths through the server.

```bash
python3 varn.py build
VARN_PORT=3000 ./build/bin/varn apps/lua/server.lua
```

It listens on `http://localhost:3000` with these routes:

| Route | Method | What it does |
|-------|--------|--------------|
| `/api/hello?name=...` | GET | JSON reply after a small simulated delay |
| `/api/echo` | POST | Echoes the posted body back as JSON |
| `/api/file` | GET | Reads a file from disk and returns its SHA-256 |
| `/` and static paths | GET | Static files from `apps/lua/public` |

## ✍️ Write the test

Save this as `stress-test.js`. It ramps the virtual user count up to a sustained load, pushes past it, then recovers, and fails the run if errors or latency cross the thresholds.

```javascript
import http from "k6/http";
import { check, sleep } from "k6";

// Base URL of a running Varn server. Override it with k6 run -e BASE_URL=http://host:port stress-test.js.
const BASE_URL = __ENV.BASE_URL || "http://localhost:3000";

export const options = {
    stages: [
        { duration: "30s", target: 50 },  // Warm up to 50 virtual users
        { duration: "1m", target: 200 },  // Ramp to the target load
        { duration: "2m", target: 200 },  // Hold the target load
        { duration: "1m", target: 500 },  // Stress beyond the target
        { duration: "30s", target: 0 },   // Ramp down
    ],
    thresholds: {
        http_req_failed: ["rate<0.01"],                   // Under 1% of requests may fail
        http_req_duration: ["p(95)<500", "p(99)<1000"],   // The 95th and 99th percentile latency budgets
    },
};

export default function () {
    // A JSON read endpoint with a small simulated delay.
    const hello = http.get(`${BASE_URL}/api/hello?name=k6`);
    check(hello, {
        "hello is 200": (r) => r.status === 200,
        "hello is json": (r) => (r.headers["Content-Type"] || "").includes("application/json"),
    });

    // A write endpoint that echoes the posted body back.
    const payload = JSON.stringify({ ping: Date.now() });
    const echo = http.post(`${BASE_URL}/api/echo`, payload, {
        headers: { "Content-Type": "application/json" },
    });
    check(echo, { "echo is 200": (r) => r.status === 200 });

    sleep(1);
}
```

## ▶️ Run it

```bash
k6 run stress-test.js

# Point at a remote server
k6 run -e BASE_URL=http://192.168.0.10:3000 stress-test.js

# Quick smoke check before a full run
k6 run --vus 5 --duration 30s stress-test.js
```

## 📊 Read the results

At the end, k6 prints a summary. The metrics that matter most for a stress test:

| Metric | Meaning |
|--------|---------|
| `http_reqs` | Total requests and requests per second |
| `http_req_duration` | Response time. Watch the `p(95)` and `p(99)` percentiles rather than the average |
| `http_req_failed` | Share of failed requests, the first sign the server is saturated |
| `vus` | Active virtual users at the moment the summary was taken |

A `THRESHOLD` line marked failed means the server crossed a budget defined in `options.thresholds`, and `k6` exits non-zero so it fails a CI step.

## ⚙️ Tune the server for load

The server runs one event loop per process, so the main scaling knob is the number of processes. Set `VARN_WORKERS` to the core count to spread connections across all cores:

```bash
VARN_WORKERS=8 ./build/bin/varn your_server.lua
```

The `listen` options shape how a single worker absorbs concurrency. Raise them when the test shows queuing or rejected connections.

```lua
server:listen({
    host = "0.0.0.0",
    port = 3000,
    maxQueued = 1024,             -- Accept backlog before new connections are refused
    requestTimeoutMs = 15000,     -- Per-request deadline
    keepAliveTimeoutSeconds = 30, -- Idle and slow-connection reaping
})
```

| Knob | Effect under load |
|------|-------------------|
| `VARN_WORKERS` | One process per core. On Linux the kernel load-balances connections across them |
| `maxQueued` | Longer accept backlog before new connections are refused |
| `requestTimeoutMs` | Bounds how long a slow request stays open |
| `keepAliveTimeoutSeconds` | Closes idle keep-alive and slow-read connections |

## 💡 Tips

- Run k6 from a different machine than the server so the load generator does not compete for the same CPU.
- Raise the open-file limit on the client (`ulimit -n`) when driving thousands of virtual users.
- Test one route per scenario when you need to attribute latency, then combine them for a realistic mix.
- For HTTPS, point `BASE_URL` at the `https://` server from `https_json_server.lua` and pass `--insecure-skip-tls-verify` for a self-signed certificate.
