#include <cuda_runtime.h>
#include <cufft.h>
#include <format>
#include <iostream>
#include <source_location>

void checkCudaErrors(cudaError_t result, std::source_location loc) {
  if (result != cudaSuccess) {
    std::cerr << std::format(
        "CUDA Runtime Error: {}:{}:{} = {}\n", loc.file_name(), loc.line(),
        static_cast<int>(result), cudaGetErrorString(result));
    cudaDeviceReset();
    exit(EXIT_FAILURE);
  }
}

void checkCufftErrors(cufftResult result, std::source_location loc) {
  if (result != CUFFT_SUCCESS) {
    std::cerr << std::format("cuFFT Error: {}:{}:{}\n", loc.file_name(),
                             loc.line(), static_cast<int>(result));
    cudaDeviceReset();
    exit(EXIT_FAILURE);
  }
}