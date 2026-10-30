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

// Colormap used to color the waterfall
static constexpr ImPlotColormap COLORMAP = ImPlotColormap_Viridis;

// Fraction of the window height used by the spectrum plot
static constexpr float SPECTRUM_HEIGHT_RATIO = 0.3f;

// Target number of waterfall rows per second of signal
static constexpr double ROWS_PER_SECOND = 50.0;

// Limits the rows processed in a single UI frame so the app doesn't fall
// further behind if a frame is delayed (e.g., while the window is dragged)
static constexpr int MAX_ROWS_PER_FRAME = 16;

// Initial range of dB values mapped to the colormap
static constexpr float DEFAULT_MIN_DB = -90.0f;
static constexpr float DEFAULT_MAX_DB = -10.0f;

// Width of the colormap scale beside the waterfall in pixels
static constexpr float COLORMAP_SCALE_WIDTH = 90.0f;

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
//! @brief Draws the controls, spectrum plot and waterfall
//!
void draw_ui(const SigmfFile &iq_file, const Waterfall &waterfall) {
  // Fill the entire window
  // Ref: https://github.com/ocornut/imgui/issues/3541#issuecomment-712248014
  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::Begin("RF Waterfall");

  const ImVec2 avail = ImGui::GetContentRegionAvail();
  const float plot_width =
      avail.x - COLORMAP_SCALE_WIDTH - ImGui::GetStyle().ItemSpacing.x;
  const float spectrum_height = avail.y * SPECTRUM_HEIGHT_RATIO;
  const float waterfall_height =
      avail.y - spectrum_height - ImGui::GetStyle().ItemSpacing.y;

  const double bin_width_mhz = iq_file.sample_rate_sps() / FFT_SIZE / 1.0e6;
  double freq_min_mhz =
      iq_file.center_freq_hz() / 1.0e6 - 0.5 * FFT_SIZE * bin_width_mhz;
  double freq_max_mhz =
      iq_file.center_freq_hz() / 1.0e6 + 0.5 * FFT_SIZE * bin_width_mhz;

  // Spectrum of the latest row
  if (ImPlot::BeginPlot("##spectrum", ImVec2(plot_width, spectrum_height),
                        ImPlotFlags_NoLegend)) {
    ImPlot::SetupAxes("Frequency (MHz)", "Power (dB)");
    ImPlot::SetupAxisLinks(ImAxis_X1, &freq_min_mhz, &freq_max_mhz);
    ImPlot::SetupAxisLimits(ImAxis_Y1, DEFAULT_MIN_DB, DEFAULT_MAX_DB,
                            ImPlotCond_Always);
    // TODO: Remove dummy data generation
    std::array<float, FFT_SIZE> points{};
    for (std::size_t i = 0; i < points.size(); ++i) {
      points[i] = static_cast<float>(i) *
                      ((DEFAULT_MAX_DB - DEFAULT_MIN_DB) / FFT_SIZE) +
                  DEFAULT_MIN_DB;
    }
    ImPlot::PlotLine("##magnitude", waterfall.magnitude_db(), FFT_SIZE,
                     bin_width_mhz, freq_min_mhz);
    ImPlot::EndPlot();
  }

  if (ImPlot::BeginPlot("##waterfall", ImVec2(plot_width, waterfall_height),
                        ImPlotFlags_NoLegend)) {
    // TODO: Need to dynamically calculate this
    const double history_s = 20.0;

    ImPlot::SetupAxes("Frequency (MHz)", "Time (s)");
    ImPlot::SetupAxisLinks(ImAxis_X1, &freq_min_mhz, &freq_max_mhz);
    ImPlot::SetupAxisLimits(ImAxis_Y1, -history_s, 0.0, ImPlotCond_Always);

    ImPlot::PlotImage("##image", static_cast<ImTextureID>(0),
                      ImPlotPoint(freq_min_mhz, -history_s),
                      ImPlotPoint(freq_max_mhz, 0.0));
    ImPlot::EndPlot();
  }

  // Place the colormap beside the waterfall
  ImGui::SameLine();
  ImPlot::ColormapScale("##scale", DEFAULT_MIN_DB, DEFAULT_MAX_DB,
                        ImVec2(COLORMAP_SCALE_WIDTH, waterfall_height), "%g dB",
                        ImPlotColormapScaleFlags_None, COLORMAP);

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

  std::cout << std::format("Center Freq: {} MHz, Sample Rate: {} Sps\n",
                           iq_file->center_freq_hz() / 1e6,
                           iq_file->sample_rate_sps());

  // Temporary test to confirm reading some samples from the file
  std::array<short2, 1024> iq{};
  iq_file->read(iq.data(), iq.size());

  for (const auto samp : iq) {
    std::cout << std::format("I: {}, Q: {}\n", samp.x, samp.y);
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
  glfwSwapInterval(0); // Disable vsync

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

  const int frames_per_row =
      static_cast<int>(std::max(1L, std::lround(iq_file->sample_rate_sps() /
                                                (FFT_SIZE * ROWS_PER_SECOND))));

  // Scoped block is so the waterfall is constructed/destructed on each frame
  {
    Waterfall waterfall(frames_per_row);
    Waterfall::set_colormap(build_colormap(COLORMAP));

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
      pending_samps += elapsed_s * iq_file->sample_rate_sps();
      const auto row_samps = static_cast<double>(waterfall.row_samples());
      int rows = 0;
      while (pending_samps >= row_samps && rows < MAX_ROWS_PER_FRAME) {
        iq_file->read(waterfall.input_buffer(), waterfall.row_samples());
        waterfall.process_row();
        pending_samps -= row_samps;
        ++rows;
      }
      if (rows == MAX_ROWS_PER_FRAME) {
        pending_samps = std::min(pending_samps, row_samps);
      }

      // Start frame
      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      draw_ui(*iq_file, waterfall);

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
