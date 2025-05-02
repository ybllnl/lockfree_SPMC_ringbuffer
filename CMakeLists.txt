cmake_minimum_required(VERSION 3.12)

project(SPMC_RingBuffer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_library(SPMCRingBuffer INTERFACE)

target_include_directories(SPMCRingBuffer INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)


option(BUILD_TESTING "Build tests" ON)

if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)

    add_executable(SPMCRingBufferTests tests/testSPMCRingBuffer.cpp)
    target_include_directories(SPMCRingBufferTests PRIVATE include)
    target_link_libraries(SPMCRingBufferTests PRIVATE GTest::gtest GTest::gtest_main SPMCRingBuffer Threads::Threads)

    include(GoogleTest)
    gtest_discover_tests(SPMCRingBufferTests)
endif()

option(BUILD_BENCHMARKS "Build simple benchmarks" ON)
if(BUILD_BENCHMARKS)
    add_executable(SPMCBenchmark benchmarks/benchmark.cpp)
    target_link_libraries(SPMCBenchmark PRIVATE SPMCRingBuffer Threads::Threads)
    target_compile_options(SPMCBenchmark PRIVATE -O3 -march=native)
    find_package(Threads REQUIRED)
endif()

# Installation
install(TARGETS SPMCRingBuffer DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)