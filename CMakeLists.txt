project(spectrum_analyzer CXX)
cmake_minimum_required(VERSION 3.1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsuggest-override")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wfatal-errors")

set(THREADS_PREFER_PTHREAD_FLAG ON)
FIND_PACKAGE(Threads REQUIRED)

FIND_LIBRARY(FFTW_LIBRARY fftw)
FIND_LIBRARY(RFFTW_LIBRARY rfftw)

FIND_PACKAGE(ALSA REQUIRED)
FIND_PACKAGE(Boost REQUIRED)

FIND_PACKAGE(QCustomPlot REQUIRED)
FIND_PACKAGE(Qt5PrintSupport REQUIRED)
FIND_PACKAGE(Qt5Widgets REQUIRED)

INCLUDE_DIRECTORIES(${Qt5Widgets_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR} ${QCustomPlot_INCLUDE_DIR})

ADD_DEFINITIONS(${Qt5Widgets_DEFINITIONS} -DQCUSTOMPLOT_USE_LIBRARY)

SET(CMAKE_AUTOMOC ON)
QT5_WRAP_UI(UI_HEADERS src/ui/mainwindow.ui)

add_executable(spectrum_analyzer
	src/main.cpp
	src/alsa.cpp
	src/fft.cpp
	src/utils/logger.cpp
	src/ui/ui.cpp
	src/ui/mainwindow.cpp
	${UI_HEADERS})
	
QT5_USE_MODULES(spectrum_analyzer Widgets)
target_link_libraries(spectrum_analyzer
	Threads::Threads
	${ALSA_LIBRARY}
	${RFFTW_LIBRARY}
	${FFTW_LIBRARY}
	${Qt5Widgets_LIBRARIES}
	${QCustomPlot_LIBRARIES}
	Qt5::PrintSupport)

add_executable(list_pcm_devices
	src/utils/list_pcm_devices.cpp)

target_link_libraries(list_pcm_devices
	${ALSA_LIBRARY})
