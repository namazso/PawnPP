include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
  URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(pawnpp_test test.cpp)
target_link_libraries(pawnpp_test PRIVATE pawnpp GTest::gtest_main)

configure_file(test16.amx test16.amx COPYONLY)
configure_file(test32.amx test32.amx COPYONLY)
configure_file(test64.amx test64.amx COPYONLY)
configure_file(../Example/test.amx ../Example/test.amx COPYONLY)

include(GoogleTest)
gtest_discover_tests(pawnpp_test)
