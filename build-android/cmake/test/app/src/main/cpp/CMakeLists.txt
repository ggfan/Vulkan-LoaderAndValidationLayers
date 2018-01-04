cmake_minimum_required(VERSION 3.4.1)

add_subdirectory(../../../../../layerlib ./layerLibBin)

add_library(native-lib SHARED native-lib.cpp)
target_link_libraries(native-lib log)

