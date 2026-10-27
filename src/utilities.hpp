#ifndef UTILITIES_HPP
#define UTILITIES_HPP

#include <cuda_runtime.h>
#include <cufft.h>
#include <format>
#include <iostream>
#include <source_location>

//!
//! @brief Helper function for checking for CUDA errors
//!
//! @param[in] result The result of a CUDA API call
//! @param[in] loc    The source code location of the function call
//!
inline void
checkCudaErrors(cudaError_t result,
                std::source_location loc = std::source_location::current()) {
  if (result != cudaSuccess) {
    std::cerr << std::format(
        "CUDA Runtime Error: {}:{}:{} = {}\n", loc.file_name(), loc.line(),
        static_cast<int>(result), cudaGetErrorString(result));
    cudaDeviceReset();
    exit(EXIT_FAILURE);
  }
}

//!
//! @brief Helper function for checking for cuFFT errors
//!
//! @param[in] result The result of a cuFFT API call
//! @param[in] loc    The source code location of the function call
//!
inline void
checkCufftErrors(cufftResult result,
                 std::source_location loc = std::source_location::current()) {
  if (result != CUFFT_SUCCESS) {
    std::cerr << std::format("cuFFT Error: {}:{}:{}\n", loc.file_name(),
                             loc.line(), static_cast<int>(result));
    cudaDeviceReset();
    exit(EXIT_FAILURE);
  }
}

#endif
