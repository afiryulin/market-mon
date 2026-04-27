FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    gcc-13 g++-13 ninja-build git pkg-config libssl-dev libgflags-dev zlib1g-dev curl \
    && rm -rf /var/lib/apt/lists/*

RUN curl -L https://github.com/Kitware/CMake/releases/download/v4.3.2/cmake-4.3.2-linux-x86_64.tar.gz | \
    tar --strip-components=1 -xz -C /usr/local

WORKDIR /grpc-build
RUN git clone --recurse-submodules -b v1.80.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc .

RUN cmake -B build -G Ninja \
    -DCMAKE_CXX_COMPILER=g++-13 \
    -DCMAKE_C_COMPILER=gcc-13 \
    -DCMAKE_BUILD_TYPE=Release \
    -DgRPC_INSTALL=ON \
    -DgRPC_BUILD_TESTS=OFF \
    -DgRPC_SSL_PROVIDER=package \
    -DgRPC_ZLIB_PROVIDER=package \
    -DgRPC_BUILD_GRPC_CLI=ON \
    -DCMAKE_INSTALL_PREFIX=/export \
    && cmake --build build --target install

FROM ubuntu:24.04

COPY --from=builder /export/ /usr/local/

RUN apt-get update && apt-get install -y \
    gcc-13 g++-13 libssl3 zlib1g pkg-config \
    && rm -rf /var/lib/apt/lists/* \
    && ldconfig \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 100

ENV CC=gcc CXX=g++
WORKDIR /work
