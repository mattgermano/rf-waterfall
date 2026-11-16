#ifndef UTILITIES_HPP
#define UTILITIES_HPP

#include <cuda_runtime.h>
#include <cufft.h>
#include <source_location>

//!
//! @brief Helper function for checking for CUDA errors
//!
//! @param[in] result The result of a CUDA API call
//! @param[in] loc    The source code location of the function call
//!
void checkCudaErrors(cudaError_t result, std::source_location loc =
                                             std::source_location::current());

//!
//! @brief Helper function for checking for cuFFT errors
//!
//! @param[in] result The result of a cuFFT API call
//! @param[in] loc    The source code location of the function call
//!
void checkCufftErrors(cufftResult result, std::source_location loc =
                                              std::source_location::current());

#endif
