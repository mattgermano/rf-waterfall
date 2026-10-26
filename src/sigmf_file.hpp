//!
//! @file sigmf_file.hpp
//! @brief Reads complex int16 I/Q samples from a SigMF recording
//!

#ifndef SIGMF_FILE_HPP
#define SIGMF_FILE_HPP

#include <fstream>
#include <string>
#include <string_view>
#include <vector_types.h>

// A SigMF recording is a pair of files where one is JSON metadata and the other
// is a binary file containing the samples. The metadata includes things like
// the sample rate and center frequency.
static constexpr std::string_view SIGMF_META_EXT = ".sigmf-meta";
static constexpr std::string_view SIGMF_DATA_EXT = ".sigmf-data";

class SigmfFile {
public:
  SigmfFile() = default;
  ~SigmfFile() = default;

  SigmfFile(const SigmfFile &) = delete;
  SigmfFile &operator=(const SigmfFile &) = delete;
  SigmfFile(SigmfFile &&) = delete;
  SigmfFile &operator=(SigmfFile &&) = delete;

  //!
  //! @brief Opens a SigMF recording and parses its metadata
  //!
  //! @param[in] path The path to either the .sigmf-meta or .sigmf-data file of
  //! the recording
  //! @return true if the recording contains 16-bit I/Q samples, else false
  //!
  bool open(const std::string &path);

  //!
  //! @brief Opens the binary file that holds the samples once the metadata has
  //! been parsed
  //!
  //! @param[in] path           The path to the file containing the samples
  //! @param[in] data_start     The byte offset of the first sample
  //! @param[in] data_end       The byte offset after the last sample
  //! @param[in] sample_rate    The sample rate in samples per second
  //! @param[in] center_freq_hz The frequency the radio was tuned to in Hz
  //! @return true if the file contains at least one sample, else false
  //!
  bool open_samples(const std::string &path, std::streamoff data_start,
                    std::streamoff data_end, double sample_rate,
                    double center_freq_hz);

  //!
  //! @brief Reads I/Q samples from the file and wraps around to the first
  //! sample when the end of the file is reached
  //!
  //! @param[out] dst       The buffer to store the samples in
  //! @param[in]  num_samps The number of samples to read
  //!
  void read(short2 *dst, std::size_t num_samps);

  //!
  //! @return The sample rate of the file in samples per second
  //!
  [[nodiscard]] double sample_rate_sps() const { return m_sample_rate_sps; }

  //!
  //! @return The frequency the radio was tuned to in Hz or 0 if the file does
  //! not contain it
  //!
  [[nodiscard]] double center_freq_hz() const { return m_center_freq_hz; }

private:
  std::ifstream m_file;
  std::streamoff m_data_start = 0;
  std::streamoff m_data_end = 0;
  std::streamoff m_position = 0;
  double m_sample_rate_sps = 0.0;
  double m_center_freq_hz = 0.0;
};

#endif
