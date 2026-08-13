cmake_minimum_required(VERSION 3.21)

project(PawnPP CXX)

add_library(pawnpp INTERFACE)
target_include_directories(pawnpp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(pawnpp INTERFACE cxx_std_20)

option(PAWNPP_BUILD_EXAMPLE "Build the PawnPP example" ${PROJECT_IS_TOP_LEVEL})
option(PAWNPP_BUILD_TESTS "Build the PawnPP tests" ${PROJECT_IS_TOP_LEVEL})

if(PAWNPP_BUILD_EXAMPLE)
  add_subdirectory(Example)
endif()

if(PAWNPP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(Test)
endif()
