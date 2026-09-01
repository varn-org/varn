#!/usr/bin/env bash
# fair comparison of the same /plaintext and /json routes on varn, node and python (uvicorn/asgi) under the same wrk load
# single-core by default, set WORKERS=N for multi-core
#
#   THREADS=4 CONNECTIONS=200 DURATION=10s WORKERS=1 bash bench/run.sh
#
# env knobs are THREADS, CONNECTIONS, DURATION, WORKERS, PORT_BASE and VARN
set -u
cd "$(dirname "$0")/.."

VARN=${VARN:-build/bin/varn}
THREADS=${THREADS:-2}
CONNECTIONS=${CONNECTIONS:-50}
DURATION=${DURATION:-5s}
WORKERS=${WORKERS:-1}
PORT_BASE=${PORT_BASE:-38010}
ROUTES=${ROUTES:-"/plaintext /json /db /cache"}

# database and cache endpoints inherited by every server process
export MYSQL_HOST=${MYSQL_HOST:-127.0.0.1}
export MYSQL_PORT=${MYSQL_PORT:-3306}
export MYSQL_USER=${MYSQL_USER:-bench}
export MYSQL_PASSWORD=${MYSQL_PASSWORD:-benchpass}
export MYSQL_DB=${MYSQL_DB:-bench}
export REDIS_HOST=${REDIS_HOST:-127.0.0.1}
export REDIS_PORT=${REDIS_PORT:-6379}
export POOL_SIZE=${POOL_SIZE:-16}

command -v wrk >/dev/null 2>&1 || { echo "wrk not found (brew install wrk / apt install wrk)"; exit 1; }

cleanup() { pkill -f "bench/servers/varn_server.lua" 2>/dev/null; pkill -f "bench/servers/node_server.js" 2>/dev/null; pkill -f "asgi_server:app" 2>/dev/null; }
trap cleanup EXIT
cleanup; sleep 0.3

bench_server() {
  local name="$1" port="$2" marker="$3"; shift 3
  "$@" >"/tmp/bench_${name}.log" 2>&1 &
  local ready=0
  for _ in $(seq 1 60); do
    if curl -fs -o /dev/null "http://127.0.0.1:${port}/plaintext"; then ready=1; break; fi
    sleep 0.1
  done
  if [ "$ready" -ne 1 ]; then printf "%-10s %-11s %14s   (did not start — /tmp/bench_%s.log)\n" "$name" "-" "FAIL" "$name"; pkill -f "$marker" 2>/dev/null; return; fi
  sleep 0.5
  for route in $ROUTES; do
    local out; out=$(wrk -t"$THREADS" -c"$CONNECTIONS" -d"$DURATION" --latency "http://127.0.0.1:${port}${route}" 2>&1)
    local rps lat p99
    rps=$(echo "$out" | awk '/Requests\/sec/{print $2}')
    lat=$(echo "$out" | awk '/^[[:space:]]*Latency/{print $2; exit}')
    p99=$(echo "$out" | awk '/99%/{print $2; exit}')
    printf "%-10s %-11s %14s %12s %12s\n" "$name" "$route" "${rps:-FAIL}" "${lat:-?}" "${p99:-?}"
  done
  pkill -f "$marker" 2>/dev/null; sleep 0.4
}

echo "load: wrk -t${THREADS} -c${CONNECTIONS} -d${DURATION}  |  workers=${WORKERS}"
printf "%-10s %-11s %14s %12s %12s\n" "runtime" "route" "req/s" "lat(avg)" "lat(p99)"
echo "-----------------------------------------------------------------"
bench_server varn   $((PORT_BASE+0)) "bench/servers/varn_server.lua" env PORT=$((PORT_BASE+0)) VARN_WORKERS="$WORKERS" "$VARN" bench/servers/varn_server.lua
bench_server node   $((PORT_BASE+1)) "bench/servers/node_server.js"  env PORT=$((PORT_BASE+1)) WORKERS="$WORKERS" node bench/servers/node_server.js
bench_server python $((PORT_BASE+2)) "asgi_server:app"               uvicorn --app-dir bench/servers asgi_server:app --host 127.0.0.1 --port $((PORT_BASE+2)) --workers "$WORKERS" --log-level warning
