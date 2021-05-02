cmake_minimum_required(VERSION 3.16)

project(minimal_spectrum_analyzer)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(PulseAudio REQUIRED)

add_executable(analyzer main.cpp)

add_subdirectory(external/muFFT)
target_include_directories(analyzer PUBLIC "${CMAKE_SOURCE_DIR}/external/muFFT")
target_link_directories(analyzer PUBLIC "${CMAKE_BINARY_DIR}/external/muFFT")

target_link_libraries(analyzer ${PULSEAUDIO_LIBRARIES} ${PULSEAUDIOSIMPLE_LIBRARIES} muFFT muFFT-avx muFFT-sse muFFT-sse3)
