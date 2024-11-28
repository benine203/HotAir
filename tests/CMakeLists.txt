enable_testing()

find_package(GTest CONFIG REQUIRED)

add_executable(test-mmapped test-mmapped.cpp)
target_link_libraries(test-mmapped PRIVATE GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)

gtest_discover_tests(test-mmapped)