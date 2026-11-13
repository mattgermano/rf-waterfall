# RF Waterfall

This project contains the source code for the Introduction to GPU programming
final project.

## Prerequisites

1. Linux system with an NVIDIA GPU
1. [NVIDIA
   driver](https://docs.nvidia.com/datacenter/tesla/driver-installation-guide/latest/index.html)
   with support for CUDA 13 (v580+)
1. [NVIDIA Container
   Toolkit](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/install-guide.html)
1. [Docker + Docker Compose](https://docs.docker.com/engine/install/)

## Quickstart

```shell
curl --output-dir data -O http://nas.destevez.net/~daniel/LTE/LTE_uplink_847MHz_2022-01-30_30720ksps.sigmf-meta
curl --output-dir data -O http://nas.destevez.net/~daniel/LTE/LTE_uplink_847MHz_2022-01-30_30720ksps.sigmf-data
```

```shell
xhost +local:docker
docker compose up --build; docker compose down
xhost -local:docker
```
