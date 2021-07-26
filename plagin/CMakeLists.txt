cmake_minimum_required( VERSION 3.0 )
project(overlay2 C)

add_library(overlay2 SHARED overlay.c deadbeef/deadbeef.h)
target_link_libraries(overlay2 pthread rt)

