cmake_minimum_required(VERSION 3.16)

project(minimal_spectrum_analyzer)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if (WIN32)
    add_definitions(-DANALYZER_USE_WASAPI)
    set(ANALYZER_SYSTEM_LIBS "Avrt.lib")
else (WIN32)
    add_definitions(-DANALYZER_USE_PULSEAUDIO)
    find_package(PulseAudio REQUIRED)
    set(ANALYZER_SYSTEM_LIBS ${PULSEAUDIO_LIBRARIES} ${PULSEAUDIOSIMPLE_LIBRARIES})
endif (WIN32)

if (MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT /source-charset:utf-8 /execution-charset:utf-8 /Zi /DEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd /source-charset:utf-8 /execution-charset:utf-8 /Zi /DEBUG")
endif (MSVC)

message("ANALYZER_SYSTEM_LIBS: ${ANALYZER_SYSTEM_LIBS}")

add_executable(analyzer src/main.cpp)

target_compile_features(analyzer PUBLIC cxx_std_20)

add_subdirectory(external/muFFT)
target_include_directories(analyzer PUBLIC "${CMAKE_SOURCE_DIR}/external/muFFT")
target_link_directories(analyzer PUBLIC "${CMAKE_BINARY_DIR}/external/muFFT")

add_subdirectory(external/cxxopts)
target_include_directories(analyzer PUBLIC "${CMAKE_SOURCE_DIR}/external/cxxopts/include")


target_link_libraries(analyzer ${ANALYZER_SYSTEM_LIBS} muFFT)
