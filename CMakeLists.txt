cmake_minimum_required(VERSION 2.6)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb3 -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wfatal-errors")

set(GTEST_INCLUDES "/opt/google/googletest-1.8.0/googletest/include")
set(GTEST_LIBS "/opt/google/googletest-1.8.0/googlemock/gtest/libgtest.a")

include_directories(${GTEST_INCLUDES})
 
add_executable(spectrum_analyzer
	src/main.cpp
	src/alsa.cpp
	src/logger.cpp)
	
target_link_libraries(spectrum_analyzer pthread asound)