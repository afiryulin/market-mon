FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential \
    ninja-build \
    git \
    pkg-config \
    libssl-dev \
    zlib1g-dev \
    wget \
    && rm -rf /var/lib/apt/lists/*


RUN wget -qO- https://github.com/Kitware/CMake/releases/download/v4.3.2/cmake-4.3.2-linux-x86_64.tar.gz | \
    tar --strip-components=1 -xz -C /usr/local

RUN cmake --version

WORKDIR /tmp/grpc-build

RUN git clone --recurse-submodules -b v1.80.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc .

RUN cmake -B build -G Ninja \
    -DgRPC_INSTALL=ON \
    -DgRPC_BUILD_TESTS=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DgRPC_ABSL_PROVIDER=module \
    -DgRPC_PROTOBUF_PROVIDER=module \
    -DgRPC_ZLIB_PROVIDER=package \
    -DgRPC_CARES_PROVIDER=module \
    -DgRPC_RE2_PROVIDER=module \
    -DgRPC_SSL_PROVIDER=package

RUN cmake --build build --target install -- -j$(nproc)

RUN ldconfig && cd / && rm -rf /tmp/grpc-build

# ENV CMAKE_PREFIX_PATH="/usr/local/lib/cmake/grpc:/usr/local/lib/cmake/protobuf:${CMAKE_PREFIX_PATH}"
# ENV PATH="/usr/local/bin:${PATH}"

WORKDIR /work