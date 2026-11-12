FROM ubuntu:24.04

WORKDIR /app

RUN apt-get update && apt-get install -y \
  g++-14 \
  cmake \
  wget \
  pkg-config \
  python3 \
  sudo \
  && rm -rf /var/lib/apt/lists/* \
  && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100 --slave /usr/bin/g++ g++ /usr/bin/g++-14

RUN wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2404/x86_64/cuda-keyring_1.1-1_all.deb \
  && dpkg -i cuda-keyring_1.1-1_all.deb

ARG CONAN_VERSION=2.34.0
RUN wget https://github.com/conan-io/conan/releases/download/$CONAN_VERSION/conan-$CONAN_VERSION-amd64.deb \
  && dpkg -i conan-$CONAN_VERSION-amd64.deb

COPY src ./src
COPY Makefile meson.build conanfile.py conan.lock ./
ENV PATH=/usr/local/cuda/bin:$PATH

RUN conan profile detect --force && \
  make build

RUN useradd app
USER app

ENTRYPOINT ["/app/build/rf-waterfall"]
