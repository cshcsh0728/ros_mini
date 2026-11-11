cmake_minimum_required(VERSION 3.20)
project(ros_mini LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
  add_compile_options(/utf-8 /W4)
endif()

add_executable(rosmini_demo src/main.cpp)