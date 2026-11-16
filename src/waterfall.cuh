//!
//! @file waterfall.cuh
//! @brief Converts I/Q samples into rows of an RF waterfall that are written
//! directly into an OpenGL texture
//!

#ifndef WATERFALL_CUH
#define WATERFALL_CUH

#include <GL/gl.h>
#include <cstddef>
#include <cufft.h>

// Number of rows in the waterfall texture
static constexpr int HISTORY_ROWS = 1024;

// Number of colors in the lookup table used to color each frequency bin
static constexpr int COLORMAP_SIZE = 256; // 8-bit

class Waterfall {
public:
  Waterfall();
  ~Waterfall();

  Waterfall(const Waterfall &) = delete;
  Waterfall &operator=(const Waterfall &) = delete;
  Waterfall(Waterfall &&) = delete;
  Waterfall &operator=(Waterfall &&) = delete;

  //!
  //! @brief Allocates the buffers, FFT plan and texture for a given FFT size.
  //! Any existing waterfall history is cleared.
  //!
  //! @param[in] fft_size       The number of samples per FFT
  //! @param[in] hop_size       The number of samples between the start of
  //! consecutive FFT frames
  //! @param[in] frames_per_row The number of FFT frames averaged into each row
  //!
  void configure(int fft_size, int hop_size, int frames_per_row);

  //!
  //! @brief Sets the colors used to map a dB value to a pixel
  //!
  //! @param[in] colormap Colors (RGBA) ordered from the lowest to highest dB
  //!
  static void set_colormap(const std::array<uchar4, COLORMAP_SIZE> &colormap);

  //!
  //! @brief Processes the samples in the input buffer and writes them as a new
  //! row at the top of the waterfall.
  //!
  //! @param[in] min_db The dB value mapped to the lowest color
  //! @param[in] max_db The dB value mapped to the highest color
  //!
  void process_row(float min_db, float max_db);

  //!
  //! @return The magnitude in dB of each frequency bin in the latest row,
  //! ordered from the lowest to highest frequency
  //!
  [[nodiscard]] const float *magnitude_db() const { return m_h_magnitude_db; }

  //!
  //! @return Pinned host buffer that holds row_samples() samples for the next
  //! call to process_row()
  //!
  [[nodiscard]] short2 *input_buffer() const {
    return m_h_input + overlap_samples();
  }

  //!
  //! @return The OpenGL texture containing the waterfall
  //!
  [[nodiscard]] GLuint texture() const { return m_texture; }

  //!
  //! @return The texture row containing the latest spectrum
  //!
  [[nodiscard]] int head_row() const { return m_head_row; }

  //!
  //! @return The currently configured FFT size in samples
  //!
  [[nodiscard]] int fft_size() const { return m_fft_size; }

  //!
  //! @return The number of samples between the start of consecutive FFT frames
  //!
  [[nodiscard]] int hop_size() const { return m_hop_size; }

  //!
  //! @return The number of FFT frames averaged into each row
  //!
  [[nodiscard]] int frames_per_row() const { return m_frames_per_row; }

  //!
  //! @return The number of samples averaged into each row
  //!
  [[nodiscard]] std::size_t row_samples() const {
    return static_cast<std::size_t>(m_hop_size) * m_frames_per_row;
  }

private:
  //!
  //! @return The number of samples at the end of a row that are reused by the
  //! first frame of the next row
  //!
  [[nodiscard]] std::size_t overlap_samples() const {
    return static_cast<std::size_t>(m_fft_size - m_hop_size);
  }

  //!
  //! @return The total number of samples spanned by the frames of a row
  //!
  [[nodiscard]] std::size_t input_samples() const {
    return overlap_samples() + row_samples();
  }

  //!
  //! @brief Frees the buffers, FFT plan and texture
  //!
  void cleanup();

  cudaStream_t m_stream = nullptr;
  cufftHandle m_plan = 0;
  short2 *m_h_input = nullptr;
  short2 *m_d_input = nullptr;
  cufftComplex *m_d_spectrum = nullptr;
  float *m_d_magnitude_db = nullptr;
  float *m_h_magnitude_db = nullptr;
  GLuint m_texture = 0;
  cudaGraphicsResource_t m_texture_resource = nullptr;
  int m_fft_size = 0;
  int m_hop_size = 0;
  int m_frames_per_row = 0;
  int m_head_row = 0;
};

#endif
