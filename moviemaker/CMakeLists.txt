CMAKE_MINIMUM_REQUIRED(VERSION 3.10)

PROJECT(CODE)

set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING
            "Default build type: Release" FORCE)
endif()

SET(CMAKE_INSTALL_RPATH "")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
SET(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)


SET(CMAKE_CXX_FLAGS "-O3 -funroll-loops -ftree-vectorize -ftree-slp-vectorize -fopenmp -std=gnu++17 -lstdc++ -lm ")

FIND_PACKAGE(OpenMP)

FIND_PACKAGE(VTK)
find_package(nlohmann_json 3.2.0 REQUIRED HINTS "/cfd/users/mturner/sw/json/build")

add_subdirectory(src)
