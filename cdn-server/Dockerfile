FROM ubuntu:22.04 AS builder
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential cmake git \
    libssl-dev \
    libsqlite3-dev \
    libpqxx-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc)

FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    libssl3 libsqlite3-0 libpqxx-6.4 \
    netcat iputils-ping \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/build/myscdn_node /app/build/myscdn_node
COPY --from=builder /app/scripts /app/scripts

RUN mkdir -p /app/data/db /app/data/storage

RUN chmod +x /app/scripts/*.sh

EXPOSE 8000-8020
ENTRYPOINT ["/app/scripts/run_docker.sh"]