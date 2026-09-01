#!/usr/bin/env bash
# builds varn for linux inside docker and runs the comparison benchmark there against the real deploy target
# the repo and the CPM source cache are mounted so dep sources and the build dir persist across runs
#   THREADS=4 CONNECTIONS=200 DURATION=10s WORKERS=1 bash bench/docker-bench.sh
set -euo pipefail
cd "$(dirname "$0")/.."

command -v docker >/dev/null 2>&1 || { echo "docker not found"; exit 1; }

THREADS=${THREADS:-4}
CONNECTIONS=${CONNECTIONS:-200}
DURATION=${DURATION:-10s}
WORKERS=${WORKERS:-1}
JOBS=${JOBS:-4}

docker build -t varn-bench -f bench/Dockerfile bench/

mkdir -p "$HOME/.cache/CPM"
docker run --rm \
    -v "$PWD":/varn \
    -v "$HOME/.cache/CPM":/root/.cache/CPM \
    -e THREADS="$THREADS" -e CONNECTIONS="$CONNECTIONS" -e DURATION="$DURATION" -e WORKERS="$WORKERS" -e JOBS="$JOBS" \
    varn-bench bash -c '
        set -e
        cmake -B build-linux -S . -G Ninja -DCMAKE_BUILD_TYPE=Release >/tmp/cfg.log 2>&1 || { tail -30 /tmp/cfg.log; exit 1; }
        echo "building varn for linux (-j$JOBS) ..."
        cmake --build build-linux -j"$JOBS" --target varn 2>&1 | tail -3
        echo
        VARN=build-linux/bin/varn bash bench/run.sh
    '
