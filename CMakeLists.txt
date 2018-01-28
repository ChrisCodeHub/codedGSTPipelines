cmake_minimum_required(VERSION 3.5)
project(fileDecoder)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")


set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/home/chris/dev_gst/gst-build/build/subprojects/gstreamer/pkgconfig/")
set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/home/chris/dev_gst/gst-build/build/subprojects/gst-plugins-bad/pkgconfig/")

#Bring in the headers (note I sit inside src for IDEs to find them!)
include_directories(./src/include)

#Manually add the sources using the set command as follows:
set(SOURCES ../src/FileDecoder.cpp 
            ../src/watchDog.cpp)
add_executable(fileDecoder ${SOURCES})


# the below section of "magic" acts to replace the lines
# `pkg-config --cflags --libs gstreamer-1.0 gstreamer-mpegts-1.0` from the gcc stuff
# essntially tell cmake how to pull the gstreamer code into the build

find_package(PkgConfig)
#pkg_check_modules(GSTREAMER "gstreamer-1.0" "gstreamer-mpegts-1.0")
pkg_check_modules(GSTREAMER "gstreamer-1.0-uninstalled" "gstreamer-mpegts-1.0-uninstalled")
target_link_libraries(fileDecoder ${GSTREAMER_LIBRARIES})
add_definitions(${GSTREAMER_CFLAGS} ${GSTREAMER_CFLAGS_OTHER})
