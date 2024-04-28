cmake_minimum_required(VERSION 3.16)
project(overlay_v3 C)

set(CMAKE_C_STANDARD 11)
include_directories(/usr/include/freetype2)

add_library(overlay SHARED overlay.c)
target_link_libraries(overlay X11 Xrender fontconfig Xft Xrandr m Xfixes)