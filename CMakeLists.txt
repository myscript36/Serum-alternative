cmake_minimum_required(VERSION 3.22.1)
project(waveforge CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(waveforge SHARED
    engine.cpp
    wavetables.cpp
    audio_android.cpp
    jni_bridge.cpp)

target_compile_options(waveforge PRIVATE -O3 -Wall)
find_library(log-lib log)
target_link_libraries(waveforge aaudio android ${log-lib})
