#include "utilities.hpp"
#include "waterfall.cuh"

#include <array>
#include <cmath>
#include <cstddef>
#include <cuComplex.h>
#include <cuda_runtime.h>
#include <cufft.h>
#include <numbers>
#include <vector_types.h>

// Data from a radio is typically received as int16 complex numbers, but FFT's
// operate on complex floating point numbers. This scaling factor will convert
// the int16 magnitude to a floating point magnitude between -1.0 and 1.0.
static constexpr float INT16_SCALE = 1.0f / 32767.0f;

// The FFT sums over each sample so it's necessary to divide by the FFT size
// to restore the original amplitude. Multiplying by 2.0 is needed since the
// Hann window reduces the peak amplitude by half.
static constexpr float MAGNITUDE_SCALE = 2.0f / FFT_SIZE;
static constexpr float POWER_SCALE = MAGNITUDE_SCALE * MAGNITUDE_SCALE;

// Prevents taking a log of 0
static constexpr float MIN_POWER = 1.0e-20f;

// Lookup table that maps a normalized dB value to a color
__constant__ uchar4 c_colormap[COLORMAP_SIZE]; // NOLINT

//!
//! @brief Converts a complex int16 input signal to complex floating point and
//! applies a Hann windowing function.
//!
//! @param[in]  input  The int16 complex input signal
//! @param[out] output The floating point complex output signal with a window
//! applied
//! @param[in]  num_samps The number of input samples
//!
__global__ void convert_and_window(const short2 *__restrict__ input,
                                   cufftComplex *__restrict__ output,
                                   std::size_t num_samps) {
  const std::size_t thread_idx =
      static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;

  if (thread_idx >= num_samps) {
    return;
  }

  // Get the sample number within the FFT length
  const int n = static_cast<int>(thread_idx % FFT_SIZE);

  // Apply a Hann window which tapers the ends of the signal to 0. This is
  // needed since an FFT assumes a signal is periodic where both ends of the
  // signal are close in value. This is often not the case for a real signal.
  // Reference: https://pysdr.org/content/frequency_domain#windowing
  // Formula: https://en.wikipedia.org/wiki/Window_function#Hann_window
  const float w = 0.5f * (1.0f - std::cosf(2.0f * std::numbers::pi_v<float> *
                                           static_cast<float>(n) / FFT_SIZE));

  // Scale the int16 value to the range -1.0 to 1.0 and multiply by the
  // windowing function
  output[thread_idx] = make_cuFloatComplex(
      static_cast<float>(input[thread_idx].x) * INT16_SCALE * w,
      static_cast<float>(input[thread_idx].y) * INT16_SCALE * w);
}

//!
//! @brief Calculates the magnitude in dB of each frequency bin.
//!
//! @param[in]  spectrum      The complex FFT output
//! @param[out] magnitude_db  The magnitudes of each frequency bin in dB
//! @param[in]  count         The number of frequency bins
//!
__global__ void compute_magnitude_db(const cufftComplex *__restrict__ spectrum,
                                     float *__restrict__ magnitude_db,
                                     std::size_t count) {
  const std::size_t thread_idx =
      static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;

  if (thread_idx >= count) {
    return;
  }

  // Power is just the real component squared + the imaginary component squared
  const float power = (spectrum[thread_idx].x * spectrum[thread_idx].x +
                       spectrum[thread_idx].y * spectrum[thread_idx].y) *
                      POWER_SCALE;
  // dB formula is 10log10 of the power
  // https://en.wikipedia.org/wiki/Decibel#Power_quantities
  magnitude_db[thread_idx] = 10.0f * std::log10f(std::fmaxf(power, MIN_POWER));
}

void Waterfall::set_colormap(
    const std::array<uchar4, COLORMAP_SIZE> &colormap) {
  checkCudaErrors(cudaMemcpyToSymbol(c_colormap, colormap.data(),
                                     colormap.size() * sizeof(uchar4)));
  for (const auto color : colormap) {
    // Sanity check:
    // First value: R: 68, G: 1, B: 84, A: 255 (Deep purple)
    // Last value: R: 253, G: 231, B: 37, A: 255 (Bright Yellow)
    // ...this matches the Viridis color map
    std::cout << std::format("R: {}, G: {}, B: {}, A: {}\n", color.x, color.y,
                             color.z, color.w);
  }
}
