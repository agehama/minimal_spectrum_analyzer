cmake_minimum_required(VERSION 3.16)

project(minimal_spectrum_analyzer)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(PulseAudio REQUIRED)

add_executable(analyzer main.cpp)

target_link_libraries(analyzer ${PULSEAUDIO_LIBRARIES} ${PULSEAUDIOSIMPLE_LIBRARIES})
