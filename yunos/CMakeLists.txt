##############################################
#   CMake
##############################################
cmake_minimum_required(VERSION 3.0)
include(/yuneta/development/yuneta/yunetas/tools/cmake/project.cmake)
project(yunos C)

##############################################
#   Source
##############################################
#add_subdirectory(test_frigo)
add_subdirectory(test_esteriliz)
add_subdirectory(test_sonda)
