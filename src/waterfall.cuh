//!
//! @file waterfall.cuh
//! @brief Converts I/Q samples into rows of an RF waterfall that are written
//! directly into an OpenGL texture
//!

#ifndef WATERFALL_CUH
#define WATERFALL_CUH

// Number of I/Q samples per FFT. This maps to the number of output frequency
// bins. A higher value produces better frequency resolution at the cost of
// less time resolution.
static constexpr int FFT_SIZE = 1024;

// Number of colors in the lookup table used to color each frequency bin
static constexpr int COLORMAP_SIZE = 256; // 8-bit

class Waterfall {
public:
  Waterfall() = default;
  ~Waterfall() = default;

  Waterfall(const Waterfall &) = delete;
  Waterfall &operator=(const Waterfall &) = delete;
  Waterfall(Waterfall &&) = delete;
  Waterfall &operator=(Waterfall &&) = delete;

  //!
  //! @brief Sets the colors used to map a dB value to a pixel
  //!
  //! @param[in] colormap Colors (RGBA) ordered from the lowest to highest dB
  //!
  static void set_colormap(const std::array<uchar4, COLORMAP_SIZE> &colormap);
};

#endif
