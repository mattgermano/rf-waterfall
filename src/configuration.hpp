//!
//! @file configuration.hpp
//! @brief Contains constant configuration settings and data structures
//!

#ifndef CONFIGURATION_HPP
#define CONFIGURATION_HPP

#include <array>
#include <implot.h>

// Number of rows in the waterfall texture
static constexpr int HISTORY_ROWS = 1024;

// Number of colors in the lookup table used to color each frequency bin
static constexpr int COLORMAP_SIZE = 256; // 8-bit

// Selectable number of samples per FFT
static constexpr std::array<int, 7> FFT_SIZES = {256,  512,  1024, 2048,
                                                 4096, 8192, 16384};
static constexpr std::array<const char *, FFT_SIZES.size()> FFT_SIZE_LABELS = {
    "256", "512", "1024", "2048", "4096", "8192", "16384"};
static constexpr int DEFAULT_FFT_SIZE_IDX = 2;

// Selectable overlap between consecutive FFT frames in percent. Applying the
// windowing function has a side effect of attenuating samples at the beginning
// and end of each frame. Overlapping the samples between frames ensures they
// are not lost.
static constexpr std::array<int, 4> OVERLAPS = {0, 25, 50, 75};
static constexpr std::array<const char *, OVERLAPS.size()> OVERLAP_LABELS = {
    "0%", "25%", "50%", "75%"};
static constexpr int DEFAULT_OVERLAP_IDX = 2;

// Selectable colormaps used to color the waterfall
static constexpr std::array<int, 6> COLORMAPS = {
    ImPlotColormap_Viridis, ImPlotColormap_Plasma, ImPlotColormap_Hot,
    ImPlotColormap_Cool,    ImPlotColormap_Jet,    ImPlotColormap_Twilight};
static constexpr std::array<const char *, COLORMAPS.size()> COLORMAPS_LABELS = {
    "Viridis", "Plasma", "Hot", "Cool", "Jet", "Twilight"};
static constexpr ImPlotColormap DEFAULT_COLORMAP_IDX = 0;

// Target number of waterfall rows per second of signal
static constexpr int DEFAULT_ROWS_PER_SECOND = 50;
static constexpr int MIN_ROWS_PER_SECOND = 1;
static constexpr int MAX_ROWS_PER_SECOND = 500;

// Limits the rows processed in a single UI frame so the app doesn't fall
// further behind if a frame is delayed (e.g., while the window is dragged)
static constexpr int MAX_ROWS_PER_FRAME = 16;

// Initial range of dB values mapped to the colormap
static constexpr float DEFAULT_MIN_DB = -90.0f;
static constexpr float DEFAULT_MAX_DB = -10.0f;

// Fraction of the window height used by the spectrum plot
static constexpr float SPECTRUM_HEIGHT_RATIO = 0.3f;

// Width of the colormap scale beside the waterfall in pixels
static constexpr float COLORMAP_SCALE_WIDTH = 90.0f;

// Horizontal gap between each control and the label of the next in pixels
static constexpr float CONTROL_GROUP_SPACING = 20.0f;

// State shared between the processing loop and the user interface
struct AppState {
  int fft_size_idx = DEFAULT_FFT_SIZE_IDX;
  int overlap_idx = DEFAULT_OVERLAP_IDX;
  int rows_per_second = DEFAULT_ROWS_PER_SECOND;
  int colormap_idx = DEFAULT_COLORMAP_IDX;
  float min_db = DEFAULT_MIN_DB;
  float max_db = DEFAULT_MAX_DB;
  bool paused = false;
  // Frequency of the first bin and the spacing between bins in MHz
  double first_bin_mhz = 0.0;
  double bin_width_mhz = 0.0;
  // Frequency range covered by the waterfall texture in MHz
  double image_min_mhz = 0.0;
  double image_max_mhz = 0.0;
  // Frequency axis limits shared by the spectrum and waterfall plots
  double freq_min_mhz = 0.0;
  double freq_max_mhz = 0.0;
};

#endif
