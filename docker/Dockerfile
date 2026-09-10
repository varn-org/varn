# syntax=docker/dockerfile:1

# small production base image that ships only the stripped varn binary with the full core statically linked

FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build git curl ca-certificates pkg-config perl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# release build with every core module enabled then strip the debug symbols
RUN cmake -B build -S . -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DVARN_ENABLE_TLS=ON \
        -DVARN_ENABLE_ZIP=ON \
        -DVARN_ENABLE_STATIC_FILES=ON \
    && cmake --build build --target varn -j"$(nproc)" \
    && strip build/bin/varn


FROM debian:trixie-slim AS runtime

# ca-certificates for outbound tls and the c++ runtime the binary links against
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates libstdc++6 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --uid 10001 varn

COPY --from=builder /src/build/bin/varn /usr/local/bin/varn

USER varn
WORKDIR /app
ENTRYPOINT ["varn"]
