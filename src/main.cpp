//!
//! @file main.cpp
//! @brief Renders an RF waterfall from recorded RF data
//!

// GLFW / OpenGL setup referenced from the implot example:
// https://github.com/epezent/implot/blob/master/example/main.cpp

#include "sigmf_file.hpp"
#include "waterfall.cuh"

#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>
#include <iostream>
#include <memory>
#include <string>
#include <vector_functions.h>
#include <vector_types.h>

// Selectable number of samples per FFT
static constexpr std::array<int, 7> FFT_SIZES = {256,  512,  1024, 2048,
                                                 4096, 8192, 16384};
static constexpr std::array<const char *, FFT_SIZES.size()> FFT_SIZE_LABELS = {
    "256", "512", "1024", "2048", "4096", "8192", "16384"};
static constexpr int DEFAULT_FFT_SIZE_IDX = 2;

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

// Callback to handle GLFW errors
void glfw_error_callback(int error, const char *description) {
  std::cerr << "GLFW Error " << error << ": " << description << '\n';
}

//!
//! @brief Samples an ImPlot colormap into a lookup table
//!
//! @param[in] cmap The ImPlot colormap
//! @return The colors (RGBA) ordered from the lowest to highest value
//!
std::array<uchar4, COLORMAP_SIZE> build_colormap(ImPlotColormap cmap) {
  std::array<uchar4, COLORMAP_SIZE> colormap{};

  for (int i = 0; i < COLORMAP_SIZE; ++i) {
    // SampleColormap needs a value normalized between 0 and 1
    const ImVec4 color = ImPlot::SampleColormap(
        static_cast<float>(i) / (COLORMAP_SIZE - 1), cmap);

    // Need 4 values to represent red, green, blue, and alpha (RGBA)
    colormap[i] =
        make_uchar4(static_cast<unsigned char>(color.x * 255.0f),
                    static_cast<unsigned char>(color.y * 255.0f),
                    static_cast<unsigned char>(color.z * 255.0f), 255);
  }

  return colormap;
}

//!
//! @brief Configures the waterfall for the selected FFT size and row rate
//!
//! @param[in]     iq_file   The input file
//! @param[in,out] waterfall The waterfall to configure
//! @param[in,out] state     The application state
//!
void configure_waterfall(const SigmfFile &iq_file, Waterfall &waterfall,
                         AppState &state) {
  const int fft_size = FFT_SIZES[state.fft_size_idx];
  const double sample_rate = iq_file.sample_rate_sps();

  const int frames_per_row = static_cast<int>(std::max(
      1L, std::lround(sample_rate / (fft_size * state.rows_per_second))));
  waterfall.configure(fft_size, frames_per_row);

  state.bin_width_mhz = sample_rate / fft_size / 1.0e6;
  state.first_bin_mhz =
      iq_file.center_freq_hz() / 1.0e6 - 0.5 * fft_size * state.bin_width_mhz;

  state.image_min_mhz = state.first_bin_mhz - state.bin_width_mhz / 2.0;
  state.image_max_mhz =
      state.first_bin_mhz + (fft_size - 0.5) * state.bin_width_mhz;
  state.freq_min_mhz = state.image_min_mhz;
  state.freq_max_mhz = state.image_max_mhz;
}

//!
//! @brief Places a label to the left of the next control on the current line
//!
//! @param[in] label The text of the label
//! @param[in] width The width of the next control in pixels
//!
void left_label(const char *label, float width) {
  const float line_end_x =
      ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
  const float group_width = CONTROL_GROUP_SPACING +
                            ImGui::CalcTextSize(label).x +
                            ImGui::GetStyle().ItemSpacing.x + width;
  if (ImGui::GetItemRectMax().x + group_width <= line_end_x) {
    ImGui::SameLine(0.0f, CONTROL_GROUP_SPACING);
  }
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(label);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(width);
}

//!
//! @brief Draws the controls, spectrum plot and waterfall
//!
void draw_ui(const SigmfFile &iq_file, Waterfall &waterfall, AppState &state) {
  // Fill the entire window
  // Ref: https://github.com/ocornut/imgui/issues/3541#issuecomment-712248014
  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::Begin("RF Waterfall");

  if (ImGui::Button(state.paused ? "Play" : "Pause", ImVec2(60.0f, 0.0f))) {
    state.paused = !state.paused;
  }

  ImGui::SameLine();
  if (ImGui::Button("Reset Zoom")) {
    state.freq_min_mhz = state.image_min_mhz;
    state.freq_max_mhz = state.image_max_mhz;
  }

  left_label("FFT size", 100.0f);
  if (ImGui::Combo("##fft_size", &state.fft_size_idx, FFT_SIZE_LABELS.data(),
                   static_cast<int>(FFT_SIZE_LABELS.size()))) {
    configure_waterfall(iq_file, waterfall, state);
  }

  left_label("Rows/s", 150.0f);
  ImGui::SliderInt("##rows_per_second", &state.rows_per_second,
                   MIN_ROWS_PER_SECOND, MAX_ROWS_PER_SECOND, "%d",
                   ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    configure_waterfall(iq_file, waterfall, state);
  }

  left_label("Colormap", 100.0f);
  if (ImGui::Combo("##colormap", &state.colormap_idx, COLORMAPS_LABELS.data(),
                   static_cast<int>(COLORMAPS_LABELS.size()))) {
    Waterfall::set_colormap(build_colormap(COLORMAPS[state.colormap_idx]));
  }

  left_label("dB range", 250.0f);
  ImGui::DragFloatRange2("##db_range", &state.min_db, &state.max_db, 0.5f,
                         -200.0f, 50.0f, "%.1f dB", "%.1f dB",
                         ImGuiSliderFlags_AlwaysClamp);

  const double row_duration_s =
      static_cast<double>(waterfall.row_samples()) / iq_file.sample_rate_sps();
  const double history_s = row_duration_s * HISTORY_ROWS;

  const std::string info = std::format(
      "Sample rate: {:.2f} Msps | Center frequency: {:.2f} MHz",
      iq_file.sample_rate_sps() / 1.0e6, iq_file.center_freq_hz() / 1.0e6);
  ImGui::TextUnformatted(info.c_str());

  const ImVec2 avail = ImGui::GetContentRegionAvail();
  const float plot_width =
      avail.x - COLORMAP_SCALE_WIDTH - ImGui::GetStyle().ItemSpacing.x;
  const float spectrum_height = avail.y * SPECTRUM_HEIGHT_RATIO;
  const float waterfall_height =
      avail.y - spectrum_height - ImGui::GetStyle().ItemSpacing.y;

  // Spectrum of the latest row
  if (ImPlot::BeginPlot("##spectrum", ImVec2(plot_width, spectrum_height),
                        ImPlotFlags_NoLegend)) {
    ImPlot::SetupAxes("Frequency (MHz)", "Power (dB)");
    ImPlot::SetupAxisLinks(ImAxis_X1, &state.freq_min_mhz, &state.freq_max_mhz);
    ImPlot::SetupAxisLimits(ImAxis_Y1, state.min_db, state.max_db,
                            ImPlotCond_Always);
    ImPlot::PlotLine("##magnitude", waterfall.magnitude_db(),
                     waterfall.fft_size(), state.bin_width_mhz,
                     state.first_bin_mhz);
    ImPlot::EndPlot();
  }

  if (ImPlot::BeginPlot("##waterfall", ImVec2(plot_width, waterfall_height),
                        ImPlotFlags_NoLegend)) {
    ImPlot::SetupAxes("Frequency (MHz)", "Time (s)");
    ImPlot::SetupAxisLinks(ImAxis_X1, &state.freq_min_mhz, &state.freq_max_mhz);
    ImPlot::SetupAxisLimits(ImAxis_Y1, -history_s, 0.0, ImPlotCond_Always);

    const float v_start = static_cast<float>(waterfall.head_row()) /
                          static_cast<float>(HISTORY_ROWS);
    ImPlot::PlotImage("##image", static_cast<ImTextureID>(waterfall.texture()),
                      ImPlotPoint(state.image_min_mhz, -history_s),
                      ImPlotPoint(state.image_max_mhz, 0.0),
                      ImVec2(0.0f, v_start), ImVec2(1.0f, v_start + 1.0f));
    ImPlot::EndPlot();
  }

  // Place the colormap beside the waterfall
  ImGui::SameLine();
  ImPlot::ColormapScale("##scale", state.min_db, state.max_db,
                        ImVec2(COLORMAP_SCALE_WIDTH, waterfall_height), "%g dB",
                        ImPlotColormapScaleFlags_None,
                        COLORMAPS[state.colormap_idx]);

  ImGui::End();
}

//!
//! @brief Opens an I/Q recording
//!
//! @param[in] path The path to a SigMF recording
//! @return The opened file, or nullptr if it couldn't be opened
//!
std::unique_ptr<SigmfFile> open_iq_file(const std::string &path) {
  const std::string extension = std::filesystem::path(path).extension();
  std::unique_ptr<SigmfFile> iq_file;
  if (extension == SIGMF_META_EXT || extension == SIGMF_DATA_EXT) {
    iq_file = std::make_unique<SigmfFile>();
  } else {
    std::cerr << std::format("{} is not a {} or {} file\n", path,
                             SIGMF_META_EXT, SIGMF_DATA_EXT);
    return nullptr;
  }

  if (!iq_file->open(path)) {
    return nullptr;
  }

  return iq_file;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << std::format("Usage: {} file{}\n", argv[0], SIGMF_META_EXT);
    return EXIT_FAILURE;
  }

  const std::unique_ptr<SigmfFile> iq_file = open_iq_file(argv[1]);
  if (!iq_file) {
    return EXIT_FAILURE;
  }

  // Setup error callback
  glfwSetErrorCallback(glfw_error_callback);

  // Initialize GLFW
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW\n";
    return -1;
  }

  // Setup OpenGL version
#if defined(__APPLE__)
  // GL 3.2 + GLSL 150 (MacOS)
  const char *glsl_version = "#version 150";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on MacOS
#else
  // GL 3.0 + GLSL 130 (Windows and Linux)
  const char *glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  // glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
#endif

  // Create window
  GLFWwindow *window =
      glfwCreateWindow(1200, 800, "RF Waterfall", nullptr, nullptr);
  if (!window) {
    std::cerr << "Failed to create GLFW window\n";
    glfwTerminate();
    return -1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  // Setup context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();

  // Disable ImGUI generating an imgui.ini file
  ImGui::GetIO().IniFilename = nullptr;

  // Setup style
  ImGui::StyleColorsDark();

  // Setup backend
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Scoped block is so the waterfall is constructed/destructed on each frame
  {
    Waterfall waterfall;
    AppState state;
    Waterfall::set_colormap(build_colormap(COLORMAPS[state.colormap_idx]));
    configure_waterfall(*iq_file, waterfall, state);

    // Number of samples that have elapsed in real time but haven't been
    // processed yet
    double pending_samps = 0.0;
    double last_time = glfwGetTime();

    // Main loop
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();

      const double now = glfwGetTime();
      const double elapsed_s = now - last_time;
      last_time = now;

      // Process the samples that would have been received from a radio since
      // the last frame
      if (!state.paused) {
        pending_samps += elapsed_s * iq_file->sample_rate_sps();
        const auto row_samps = static_cast<double>(waterfall.row_samples());
        int rows = 0;
        while (pending_samps >= row_samps && rows < MAX_ROWS_PER_FRAME) {
          iq_file->read(waterfall.input_buffer(), waterfall.row_samples());
          waterfall.process_row(state.min_db, state.max_db);
          pending_samps -= row_samps;
          ++rows;
        }
        if (rows == MAX_ROWS_PER_FRAME) {
          pending_samps = std::min(pending_samps, row_samps);
        }
      }

      // Start frame
      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      draw_ui(*iq_file, waterfall, state);

      // Render
      ImGui::Render();
      int display_w = 0, display_h = 0;
      glfwGetFramebufferSize(window, &display_w, &display_h);
      glViewport(0, 0, display_w, display_h);
      glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

      // Swap buffers
      glfwSwapBuffers(window);
    }
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImPlot::DestroyContext();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
