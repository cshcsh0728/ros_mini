cmake_minimum_required(VERSION 3.20)
project(ros_mini LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
  add_compile_options(/utf-8 /W4)
else()
  add_compile_options(-Wall -Wextra)
endif()

# 核心库：不包含任何 main，可被界面、演示、测试三个程序共同链接
add_library(rosmini_core
  src/Util.cpp
  src/Logger.cpp
  src/Message.cpp
  src/Node.cpp
  src/ROS.cpp
  src/Publisher.cpp
  src/Subscriber.cpp
)
target_include_directories(rosmini_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 图形界面程序（WIN32 子系统：双击后直接出窗口，不弹控制台）
add_executable(rosmini_demo WIN32
  src/main.cpp
  src/GuiApp.cpp
)
target_link_libraries(rosmini_demo PRIVATE rosmini_core)

# 控制台演示程序：一次性刷出全部日志，便于截图与命令行验证
add_executable(rosmini_batch src/batch_main.cpp)
target_link_libraries(rosmini_batch PRIVATE rosmini_core)

# 测试程序
enable_testing()
add_executable(rosmini_tests tests/test_main.cpp)
target_link_libraries(rosmini_tests PRIVATE rosmini_core)
add_test(NAME rosmini_tests COMMAND rosmini_tests)

# Targets: rosmini_core (static library), rosmini_demo (GUI app), rosmini_batch (console demo), rosmini_tests (tests)
