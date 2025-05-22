# minimum required version of CMake
cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
# avoid building internal tests of the FCPP library
option(FCPP_INTERNAL_TESTS "Build internal tests for FCPP." OFF)
# load the FCPP sub-module
add_subdirectory(./fcpp/src)
# CMake macro setting up the FCPP library
fcpp_setup()

# project description
project(
    examples
    VERSION 0.0.0
    DESCRIPTION "Evaluation of Composable Models and Guarantees in FCPP."
)

# target declaration
fcpp_target(./run/examples.cpp ON)
