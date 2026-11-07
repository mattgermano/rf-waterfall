#include "utilities.hpp"
#include "waterfall.cuh"

#include <array>
#include <cmath>
#include <cstddef>
#include <cuComplex.h>
#include <cuda/cmath>
#include <cuda_gl_interop.h>
#include <cuda_runtime.h>
#include <cuda_runtime_api.h>
#include <cufft.h>
#include <driver_types.h>
#include <vector>
#include <vector_types.h>

// Number of threads per block for each kernel
static constexpr int BLOCK_SIZE = 256;

// Data from a radio is typically received as int16 complex numbers, but FFT's
// operate on complex floating point numbers. This scaling factor will convert
// the int16 magnitude to a floating point magnitude between -1.0 and 1.0.
static constexpr float INT16_SCALE = 1.0f / 32767.0f;

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
//! @param[in]  fft_size      The number of frequency bins
/// @param[in]  num_frames    The number of FFT frames to average
//!
__global__ void compute_magnitude_db(const cufftComplex *__restrict__ spectrum,
                                     float *__restrict__ magnitude_db,
                                     int fft_size, int num_frames) {
  const int bin = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);

  if (bin >= fft_size) {
    return;
  }

  // The FFT places 0 Hz at the first bin followed by the positive frequencies
  // and then the negative frequencies. Need to swap the two halves to put the
  // frequencies in ascending order (aka an FFT shift).
  const int src = (bin + fft_size / 2) % fft_size;

  // Power is just the real component squared + the imaginary component
  // squared. Averaging the power across frames reduces the noise in each row.
  float power = 0.0f;
  for (int f = 0; f < num_frames; ++f) {
    const cufftComplex iq =
        spectrum[static_cast<std::size_t>(f) * fft_size + src];
    power += iq.x * iq.x + iq.y * iq.y;
  }

  // The FFT sums over each sample so it's necessary to divide by the FFT size
  // to restore the original amplitude. Multiplying by 2.0 is needed since the
  // Hann window reduces the peak amplitude by half.
  const float magnitude_scale = 2.0f / static_cast<float>(fft_size);
  power *= magnitude_scale * magnitude_scale / static_cast<float>(num_frames);

  // dB formula is 10log10 of the power
  // https://en.wikipedia.org/wiki/Decibel#Power_quantities
  magnitude_db[bin] = 10.0f * std::log10f(std::fmaxf(power, MIN_POWER));
}

//!
//! @brief Maps the magnitude of each frequency bin to a color and writes it to
//! a row of the waterfall texture.
//!
//! @param[in]  magnitude_db The magnitude of each frequency bin in dB
//! @param[out] surface      The surface of the waterfall texture
//! @param[in]  fft_size     The number of frequency bins
//! @param[in]  row          The texture row to write
//! @param[in]  min_db       The dB value mapped to the lowest color
//! @param[in]  db_range     The range of dB values mapped to colors
//!
__global__ void apply_colormap(const float *__restrict__ magnitude_db,
                               cudaSurfaceObject_t surface, int fft_size,
                               int row, float min_db, float db_range) {
  int bin = static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);

  if (bin >= fft_size) {
    return;
  }

  // Normalize the magnitude to the range 0.0 to 1.0 and look up its color
  float t = __saturatef((magnitude_db[bin] - min_db) / db_range);
  int color_idx = __float2int_rn(t * (COLORMAP_SIZE - 1));

  surf2Dwrite(c_colormap[color_idx], surface,
              static_cast<int>(bin * sizeof(uchar4)), row);
}

Waterfall::Waterfall(int frames_per_row) : m_frames_per_row(frames_per_row) {
  checkCudaErrors(cudaStreamCreateWithFlags(&m_stream, cudaStreamNonBlocking));

  checkCudaErrors(cudaMallocHost(&m_h_input, row_samples() * sizeof(short2)));
  checkCudaErrors(cudaMallocHost(&m_h_magnitude_db, FFT_SIZE * sizeof(float)));
  checkCudaErrors(cudaMalloc(&m_d_input, row_samples() * sizeof(short2)));
  checkCudaErrors(
      cudaMalloc(&m_d_spectrum, row_samples() * sizeof(cufftComplex)));
  checkCudaErrors(cudaMalloc(&m_d_magnitude_db, FFT_SIZE * sizeof(float)));
  checkCufftErrors(cufftPlan1d(&m_plan, FFT_SIZE, CUFFT_C2C, frames_per_row));
  checkCufftErrors(cufftSetStream(m_plan, m_stream));

  // Start with an empty waterfall
  std::vector<uchar4> blank(static_cast<std::size_t>(FFT_SIZE) * HISTORY_ROWS,
                            make_uchar4(0, 0, 0, 255));

  glGenTextures(1, &m_texture);
  glBindTexture(GL_TEXTURE_2D, m_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, FFT_SIZE, HISTORY_ROWS, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, blank.data());
  glBindTexture(GL_TEXTURE_2D, 0);

  // Register the texture with CUDA so the kernels can write to it directly
  // without copying through the host
  checkCudaErrors(
      cudaGraphicsGLRegisterImage(&m_texture_resource, m_texture, GL_TEXTURE_2D,
                                  cudaGraphicsRegisterFlagsSurfaceLoadStore));
}

Waterfall::~Waterfall() {
  checkCudaErrors(cudaStreamSynchronize(m_stream));
  checkCudaErrors(cudaGraphicsUnregisterResource(m_texture_resource));
  glDeleteTextures(1, &m_texture);
  checkCufftErrors(cufftDestroy(m_plan));
  checkCudaErrors(cudaFreeHost(m_h_input));
  checkCudaErrors(cudaFreeHost(m_h_magnitude_db));
  checkCudaErrors(cudaFree(m_d_input));
  checkCudaErrors(cudaFree(m_d_spectrum));
  checkCudaErrors(cudaFree(m_d_magnitude_db));
  m_texture_resource = nullptr;
  m_texture = 0;
  m_plan = 0;
  m_h_input = nullptr;
  m_h_magnitude_db = nullptr;
  m_d_input = nullptr;
  m_d_spectrum = nullptr;
  m_d_magnitude_db = nullptr;

  checkCudaErrors(cudaStreamDestroy(m_stream));
}

void Waterfall::set_colormap(
    const std::array<uchar4, COLORMAP_SIZE> &colormap) {
  checkCudaErrors(cudaMemcpyToSymbol(c_colormap, colormap.data(),
                                     colormap.size() * sizeof(uchar4)));
}

void Waterfall::process_row(float min_db, float max_db) {
  const dim3 block(BLOCK_SIZE);
  const dim3 sample_grid(static_cast<unsigned int>(
      cuda::ceil_div(row_samples(), static_cast<std::size_t>(BLOCK_SIZE))));
  const dim3 bin_grid(
      static_cast<unsigned int>(cuda::ceil_div(FFT_SIZE, BLOCK_SIZE)));

  // Copy the samples to the device
  checkCudaErrors(cudaMemcpyAsync(m_d_input, m_h_input,
                                  row_samples() * sizeof(short2),
                                  cudaMemcpyHostToDevice, m_stream));

  // Convert the signal to floating point and apply windowing function
  convert_and_window<<<sample_grid, block, 0, m_stream>>>(
      m_d_input, m_d_spectrum, row_samples());
  checkCudaErrors(cudaGetLastError());

  // Execute the FFT on each frame in the row
  checkCufftErrors(
      cufftExecC2C(m_plan, m_d_spectrum, m_d_spectrum, CUFFT_FORWARD));

  // Average the frames and convert each frequency bin to dB
  compute_magnitude_db<<<bin_grid, block, 0, m_stream>>>(
      m_d_spectrum, m_d_magnitude_db, FFT_SIZE, m_frames_per_row);
  checkCudaErrors(cudaGetLastError());

  // Draw the newest row at the top
  m_head_row = (m_head_row + HISTORY_ROWS - 1) % HISTORY_ROWS;

  // Map the texture into CUDA
  checkCudaErrors(cudaGraphicsMapResources(1, &m_texture_resource, m_stream));
  cudaArray_t texture_array = nullptr;
  checkCudaErrors(cudaGraphicsSubResourceGetMappedArray(
      &texture_array, m_texture_resource, 0, 0));

  cudaResourceDesc resource_desc{};
  resource_desc.resType = cudaResourceTypeArray;
  resource_desc.res.array.array = texture_array;
  cudaSurfaceObject_t surface = 0;
  // Need to use a surface object since it is writable
  checkCudaErrors(cudaCreateSurfaceObject(&surface, &resource_desc));

  const float db_range = std::fmaxf(max_db - min_db, 1.0e-3f);
  apply_colormap<<<bin_grid, block, 0, m_stream>>>(
      m_d_magnitude_db, surface, FFT_SIZE, m_head_row, min_db, db_range);
  checkCudaErrors(cudaGetLastError());

  checkCudaErrors(cudaGraphicsUnmapResources(1, &m_texture_resource, m_stream));

  // Copy the dB magnitudes from the device to the host for the spectrum plot
  checkCudaErrors(cudaMemcpyAsync(m_h_magnitude_db, m_d_magnitude_db,
                                  FFT_SIZE * sizeof(float),
                                  cudaMemcpyDeviceToHost, m_stream));

  checkCudaErrors(cudaStreamSynchronize(m_stream));
  checkCudaErrors(cudaDestroySurfaceObject(surface));
}
