# License: Apache 2.0. See LICENSE file in root directory.
# Copyright(c) 2019 Intel Corporation. All Rights Reserved.
cmake_minimum_required(VERSION 3.1.0)

project(motion_detect)

set(Boost_USE_MULTITHREADED TRUE)
add_definitions(-DBOOST_LOG_DYN_LINK)

# Find librealsense2 installed package
find_package(realsense2 REQUIRED)
find_package(OpenCV REQUIRED)
find_package(Boost REQUIRED COMPONENTS log program_options)
find_package(libmongocxx REQUIRED)

# Enable C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# Add the application sources to the target
add_executable(${PROJECT_NAME} motion_detect.cpp MongoStream.cpp utils.cpp)

# Link librealsense2 to the target
target_link_libraries(${PROJECT_NAME} ${realsense2_LIBRARY}  ${Boost_LIBRARIES}  ${LIBMONGOCXX_LIBRARIES} ${OpenCV_LIBRARIES})
target_include_directories(${PROJECT_NAME}  PUBLIC ${Boost_INCLUDE_DIRS} "./" ${LIBMONGOCXX_INCLUDE_DIRS} ${OpenCV_INCLUDE_DIRS}) 
