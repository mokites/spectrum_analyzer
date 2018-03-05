cmake_minimum_required(VERSION 2.6)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb3 -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wfatal-errors")
 
add_executable(spectrum_analyzer
	src/main.cpp
	src/alsa.cpp
	src/fft.cpp
	src/utils/logger.cpp)
	
target_link_libraries(spectrum_analyzer pthread asound rfftw fftw)

add_executable(list_pcm_devices
	src/utils/list_pcm_devices.cpp)

target_link_libraries(list_pcm_devices asound)