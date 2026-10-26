#include "waterfall.cuh"

#include <cufft.h>
#include <numbers>

// Number of I/Q samples per FFT. This maps to the number of output frequency
// bins. A higher value produces better frequency resolution at the cost of
// less time resolution.
static constexpr int FFT_SIZE = 1024;

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
  std::size_t thread_idx =
      static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;

  if (thread_idx >= num_samps) {
    return;
  }

  // Get the sample number within the FFT length
  int n = static_cast<int>(thread_idx % FFT_SIZE);

  // Apply a Hann window which tapers the ends of the signal to 0. This is
  // needed since an FFT assumes a signal is periodic where both ends of the
  // signal are close in value. This is often not the case for a real signal.
  // Reference: https://pysdr.org/content/frequency_domain#windowing
  // Formula: https://en.wikipedia.org/wiki/Window_function#Hann_window
  float w = 0.5f * (1.0f - std::cosf(2.0f * std::numbers::pi_v<float> *
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
  std::size_t thread_idx =
      static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;

  if (thread_idx >= count) {
    return;
  }

  // Power is just the real component squared + the imaginary component squared
  float power = (spectrum[thread_idx].x * spectrum[thread_idx].x +
                 spectrum[thread_idx].y * spectrum[thread_idx].y) *
                POWER_SCALE;
  // dB formula is 10log10 of the power
  // https://en.wikipedia.org/wiki/Decibel#Power_quantities
  magnitude_db[thread_idx] = 10.0f * std::log10f(std::fmaxf(power, MIN_POWER));
}
