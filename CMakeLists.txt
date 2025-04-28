cmake_minimum_required(VERSION 3.10)

project(SPMC_RingBuffer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_library(SPMCRingBuffer STATIC src/SPMCRingBuffer.cpp)

target_include_directories(SPMCRingBuffer PUBLIC include)

target_compile_options(SPMCRingBuffer PRIVATE 
    -O3
    -march=native
)

# Installation
install(TARGETS SPMCRingBuffer DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)