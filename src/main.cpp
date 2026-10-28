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
#include <array>
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
void draw_ui() {
  // Fill the entire window
  // Ref: https://github.com/ocornut/imgui/issues/3541#issuecomment-712248014
  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::Begin("RF Waterfall");

  // TODO: Will need to dynamically determine the height
  const float colormap_height = 300.0f;
  ImPlot::ColormapScale("##scale", DEFAULT_MIN_DB, DEFAULT_MAX_DB,
                        ImVec2(COLORMAP_SCALE_WIDTH, colormap_height), "%g dB",
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

  Waterfall::set_colormap(build_colormap(COLORMAP));

  // Main loop
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // Start frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    draw_ui();

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

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImPlot::DestroyContext();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
