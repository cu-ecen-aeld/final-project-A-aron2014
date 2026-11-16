cmake_minimum_required(VERSION 3.16)
project(aesd_lidar_node LANGUAGES C CXX)

if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 11)
endif()
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(Threads REQUIRED)

add_library(aesd_lidar STATIC
  src/ld_parser.c
  src/ld_scan.c
  src/ld_serial.c
  src/ring_buffer.c
  src/serial_reader.c
)
target_include_directories(aesd_lidar PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(aesd_lidar PUBLIC Threads::Threads)

add_executable(lidar_node src/lidar_node.cpp)
target_link_libraries(lidar_node PRIVATE
  aesd_lidar
  rclcpp::rclcpp
  ${sensor_msgs_TARGETS}
)

add_executable(lidar_dump tools/lidar_dump.c)
target_link_libraries(lidar_dump PRIVATE aesd_lidar)

install(TARGETS lidar_node lidar_dump
  DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
