cmake_minimum_required(VERSION 3.14)
project(libczim VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---------- Options ----------
option(CZIM_BUILD_TESTS "Build tests" ON)
option(BUILD_SHARED_LIBS "Build shared library" OFF)

# ---------- Dependencies ----------
find_package(zstd REQUIRED)
find_package(LibLZMA REQUIRED)
add_subdirectory(deps/uobject)

# ---------- Library ----------
file(GLOB CZIM_SOURCES src/*.c)
file(GLOB CZIM_HEADERS src/*.h)

set(CZIM_PUBLIC_HEADERS
    src/czim_archive.h
    src/czim_archive_ext.h
    src/czim_entry.h
    src/czim_types.h
)

add_library(czim ${CZIM_SOURCES})
target_include_directories(czim
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
            $<INSTALL_INTERFACE:include>
)
target_link_libraries(czim PUBLIC uobject zstd::libzstd LibLZMA::LibLZMA)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(czim PUBLIC pthread)
endif()

if(MSVC)
    target_compile_options(czim PRIVATE /W4)
else()
    target_compile_options(czim PRIVATE -Wall -Wextra -pedantic)
endif()

# ---------- Tests ----------
if(CZIM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------- Install ----------
include(GNUInstallDirs)

install(TARGETS czim
    EXPORT czimTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES ${CZIM_PUBLIC_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/czim)

install(EXPORT czimTargets
    FILE czimTargets.cmake
    NAMESPACE czim::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/czim
)
