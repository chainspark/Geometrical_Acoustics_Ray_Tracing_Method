include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

find_package(Threads REQUIRED)

add_executable(gart_tests
  test_geometry.cpp
  test_tracer.cpp
  test_validation.cpp)
target_link_libraries(gart_tests PRIVATE gart::core GTest::gtest_main Threads::Threads)
gart_set_warnings(gart_tests)

include(GoogleTest)
gtest_discover_tests(gart_tests DISCOVERY_TIMEOUT 60)
