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

  void configure(int fft_size, int frames_per_row);

  //!
  //! @brief Sets the colors used to map a dB value to a pixel
  //!
  //! @param[in] colormap Colors (RGBA) ordered from the lowest to highest dB
  //!
  static void set_colormap(const std::array<uchar4, COLORMAP_SIZE> &colormap);

  void process_row(float min_db, float max_db);

  [[nodiscard]] const float *magnitude_db() const { return m_h_magnitude_db; }

  [[nodiscard]] short2 *input_buffer() const { return m_h_input; }

  [[nodiscard]] std::size_t row_samples() const {
    return static_cast<std::size_t>(m_fft_size) * m_frames_per_row;
  }

  [[nodiscard]] GLuint texture() const { return m_texture; }

  [[nodiscard]] int head_row() const { return m_head_row; }

  [[nodiscard]] int fft_size() const { return m_fft_size; }

private:
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
  int m_frames_per_row = 0;
  int m_head_row = 0;
};

#endif
