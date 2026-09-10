# linux toolchain + load generator + node/python baselines for the comparison benchmark.
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build git curl ca-certificates pkg-config \
        wrk nodejs npm python3 python3-pip \
        libmysqlclient21 \
    && pip3 install --break-system-packages --no-cache-dir uvicorn uvloop httptools aiomysql redis \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /varn
