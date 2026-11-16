# RF Waterfall

This project contains the source code for the Introduction to GPU Programming
final project. The project implements a real-time waterfall and power spectral
density display for visualizing radio frequency data.

The waterfall plot visualizes radio frequency data by plotting time on the
Y-axis and frequency on the X-axis. The third dimension of the plot is displayed
using color to show variations in signal power level.

The working interface with recorded LTE uplink data can be seen in the following
video:

https://github.com/user-attachments/assets/6b742f96-8a20-4ee0-8c88-97d5455d4d55

## Background

Software defined radios are devices that can be dynamically configured to sample
an analog signal at a specific sample rate, center frequency, and gain level.
The signal is passed through an analog to digital converter and then the data is
typically transferred to a computer over Ethernet, USB, or PCIe. The data from
the signal is represented as a vector of complex numbers representing the
in-phase and quadrature components of each sample. The C++ APIs for the most
common software defined radios let you configure the settings of the radio and
then call a receive function which returns the RF data as an array of complex
numbers. Once you have those complex numbers in memory, you can perform a
variety of different digital signal processing algorithms on them (e.g., pulse
detection or frequency domain analysis). To alleviate the need to have a
physical software defined radio, it is also common to replay recorded I/Q data
from a file and there are a variety of different standards (e.g.,
[SigMF](https://sigmf.org/index.html)) that define how to structure both data
and metadata in the file.

## Prerequisites

1. Linux system with an NVIDIA GPU
1. [NVIDIA
   driver](https://docs.nvidia.com/datacenter/tesla/driver-installation-guide/latest/index.html)
   with support for CUDA 13 (v580+)
1. [NVIDIA Container
   Toolkit](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/install-guide.html)
1. [Docker + Docker Compose](https://docs.docker.com/engine/install/)

## Quickstart

The program currently supports reading [SigMF](https://sigmf.org/index.html)
formatted RF data files. Both the SigMF data (`.sigmf-data` extension) and SigMF
metadata (`.sigmf-meta` extension) files are required for playback.

The following commands download recorded LTE uplink SigMF metadata and data. All
data should be stored in the `data/` directory.

```shell
curl --output-dir data -O http://nas.destevez.net/~daniel/LTE/LTE_uplink_847MHz_2022-01-30_30720ksps.sigmf-meta
curl --output-dir data -O http://nas.destevez.net/~daniel/LTE/LTE_uplink_847MHz_2022-01-30_30720ksps.sigmf-data
```

Once the sample data is download, the application can be built and launched via
Docker compose. The `xhost` command is needed so the GUI application can be
forwarded to the host machine from inside the container.

```shell
xhost +local:docker
docker compose up --build; docker compose down
xhost -local:docker
```

## Basic Workflow

The basic process for converting the raw I/Q data to a waterfall plot is
described in the following steps. The project uses the file-based replay
approach and implements the core GPU code using CUDA, OpenGL, and cuFFT.

1. Convert the complex numbers from int16 values to scaled floating point
   values.

1. Apply a windowing function and overlap some of the samples in each bin. A
   windowing function tapers the ends of the signal to 0 since an FFT assumes
   that a signal is periodic where the starting and ending values are near each
   other. This is often not the case for real signals and would cause a large
   spike in the frequency domain if a windowing function was not applied.

1. Convert the windowed bins of data to the frequency domain using a forward
   FFT.

1. Convert each frequency bin in the output FFT to a decibel value.

1. Map the decibel values to a color using a lookup table.

1. Insert the color values into an OpenGL texture.

1. Plot the OpenGL texture.

The project leverages [CUDA’s interoperability with
OpenGL](https://docs.nvidia.com/cuda/cuda-programming-guide/04-special-topics/graphics-interop.html#opengl-interoperability)
to map OpenGL resources directly into the CUDA address space. To render the
OpenGL texture, the project uses the [ImGui](https://github.com/ocornut/imgui)
and [ImPlot](https://github.com/epezent/implot) C++ window and plotting
libraries. These libraries add interactive elements such as sliders, buttons,
and text which lets users control things like the FFT size to see its impact on
the waterfall plot.
