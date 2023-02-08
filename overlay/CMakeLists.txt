cmake_minimum_required(VERSION 3.14)
project(overlay C)
FIND_PACKAGE(PkgConfig REQUIRED)
PKG_CHECK_MODULES(GTK3 REQUIRED gtk+-3.0)

INCLUDE_DIRECTORIES(${GTK3_INCLUDE_DIRS})
LINK_DIRECTORIES(${GTK3_LIBRARY_DIRS})
ADD_DEFINITIONS(${GTK3_CFLAGS_OTHER})

find_library(CAIRO cairo)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_C_STANDARD 11)

add_executable(overlay main.c utils.h)
target_link_libraries(overlay ${GTK3_LIBRARIES} ${CAIRO} pthread rt m Xfixes X11 Xrandr)
