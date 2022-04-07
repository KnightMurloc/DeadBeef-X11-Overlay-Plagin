cmake_minimum_required( VERSION 3.0 )
project(overlay C)
find_package(PkgConfig REQUIRED)
find_package(X11 REQUIRED)
PKG_CHECK_MODULES(GTK3 REQUIRED gtk+-3.0)

include_directories(${X11_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${GTK3_INCLUDE_DIRS})
find_library(CAIRO_LIB_RELEASE cairo)
find_path(CAIRO_INCLUDE_DIR cairo/cairo.h)
include_directories(${CAIRO_INCLUDE_DIR}/cairo/)
LINK_DIRECTORIES(${GTK3_LIBRARY_DIRS})

ADD_DEFINITIONS(${GTK3_CFLAGS_OTHER})

add_library(overlay SHARED overlay.c)
#add_executable(start main.c)
#target_link_libraries(start ${X11_LIBRARIES} ${GTK3_LIBRARIES} ${CAIRO_LIB_RELEASE} m)
target_link_libraries(overlay ${X11_LIBRARIES} ${GTK3_LIBRARIES} ${CAIRO_LIB_RELEASE} m)

