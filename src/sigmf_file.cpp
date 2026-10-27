#include "sigmf_file.hpp"
#include "vector_types.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <string_view>
#include <system_error>

// Currently require complex int16 samples
static constexpr std::string_view SUPPORTED_DATATYPE = "ci16_le";

// Example Sigmf metadata JSON that needs to be parsed:
// clang-format off
// {
//     "global": {
//         "core:datatype": "ci16_le",
//         "core:description": "Multiple channels of broadcast FM music and voice",
//         "core:num_channels": 1,
//         "core:sample_rate": 4000000,
//         "core:version": "1.0.0"
//     },
//     "captures": [
//         {
//             "core:datetime": "2021-12-16T14:23:16.147Z",
//             "core:frequency": 99000000,
//             "core:sample_start": 0
//         }
//     ],
//     "annotations": []
// }
// clang-format on

// Fields needed from the "global" section of the JSON (see example above)
struct SigmfGlobal {
  std::string datatype;
  std::uint64_t num_channels = 1;
  double sample_rate = 0.0;
};

// Fields needed from the "captures" section of the JSON (see example above)
struct SigmfCapture {
  std::uint64_t sample_start = 0;
  double frequency = 0.0;
};

bool SigmfFile::open(const std::string &path) {
  std::filesystem::path meta_path(path);
  meta_path.replace_extension(SIGMF_META_EXT);
  std::filesystem::path data_path(path);
  data_path.replace_extension(SIGMF_DATA_EXT);

  std::ifstream meta_file(meta_path);
  if (!meta_file) {
    std::cerr << std::format("Failed to open {}\n", meta_path.string());
    return false;
  }

  SigmfGlobal global;
  SigmfCapture capture;
  std::size_t num_captures = 0;
  try {
    const auto record = nlohmann::json::parse(meta_file);
    const auto &global_json = record.at("global");
    global.datatype = global_json.at("core:datatype").get<std::string>();
    global.sample_rate =
        global_json.value("core:sample_rate", global.sample_rate);
    global.num_channels =
        global_json.value("core:num_channels", global.num_channels);

    const auto &captures_json = record.at("captures");
    num_captures = captures_json.size();
    if (num_captures > 0) {
      const auto &capture_json = captures_json.at(0);
      capture.sample_start =
          capture_json.value("core:sample_start", capture.sample_start);
      capture.frequency =
          capture_json.value("core:frequency", capture.frequency);
    }
  } catch (const nlohmann::json::exception &e) {
    std::cerr << std::format("Failed to parse {}: {}\n", meta_path.string(),
                             e.what());
    return false;
  }

  // Validate all fields
  if (global.datatype != SUPPORTED_DATATYPE) {
    std::cerr << std::format(
        "{} has datatype \"{}\" but only {} is supported\n", meta_path.string(),
        global.datatype, SUPPORTED_DATATYPE);
    return false;
  }

  if (global.num_channels != 1) {
    std::cerr << std::format(
        "{} has {} channels but only 1 channel is supported\n",
        meta_path.string(), global.num_channels);
    return false;
  }

  if (global.sample_rate <= 0.0) {
    std::cerr << std::format("{} does not have a valid sample rate\n",
                             meta_path.string());
    return false;
  }

  if (num_captures > 1) {
    std::cerr << std::format("{} has {} captures. The frequency of the first "
                             "capture is used for all samples\n",
                             meta_path.string(), num_captures);
  }

  std::error_code error;
  const std::uintmax_t data_size = std::filesystem::file_size(data_path, error);
  if (error) {
    std::cerr << std::format("Failed to open {}: {}\n", data_path.string(),
                             error.message());
    return false;
  }

  const auto data_start = static_cast<std::streamoff>(capture.sample_start);
  const auto data_end = static_cast<std::streamoff>(data_size);

  return open_samples(data_path.string(), data_start, data_end,
                      global.sample_rate, capture.frequency);
}

bool SigmfFile::open_samples(const std::string &path, std::streamoff data_start,
                             std::streamoff data_end, double sample_rate,
                             double center_freq_hz) {
  m_file.open(path, std::ios::binary);
  if (!m_file) {
    std::cerr << std::format("Failed to open {}\n", path);
    return false;
  }

  m_data_start = data_start;
  m_data_end = data_end;

  // Only read whole I/Q samples
  m_data_end -=
      (m_data_end - m_data_start) % static_cast<std::streamoff>(sizeof(short2));
  if (m_data_end <= m_data_start) {
    std::cerr << std::format("{} does not contain any samples\n", path);
    return false;
  }

  m_sample_rate_sps = sample_rate;
  m_center_freq_hz = center_freq_hz;
  m_position = m_data_start;
  m_file.seekg(m_position);

  return true;
}

void SigmfFile::read(short2 *dst, std::size_t num_samps) {
  while (num_samps > 0) {
    if (m_position >= m_data_end) {
      // Loop back to the first sample so playback continues
      m_position = m_data_start;
      m_file.clear();
      m_file.seekg(m_position);
    }

    const auto avail_samps =
        static_cast<std::size_t>(m_data_end - m_position) / sizeof(short2);
    const std::size_t count = std::min(avail_samps, num_samps);
    const auto num_bytes = static_cast<std::streamsize>(count * sizeof(short2));

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    m_file.read(reinterpret_cast<char *>(dst), num_bytes);
    m_position += num_bytes;
    // Need to increment by the dst pointer by the number of samples (not bytes)
    dst += count;
    num_samps -= count;
  }
}
