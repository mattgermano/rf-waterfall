# RF Waterfall

This project contains the source code for the Introduction to GPU programming
final project.

## Prerequisites

1. Linux system
1. [NVIDIA Container Toolkit](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/install-guide.html)
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
