add_executable(test_crypto test_crypto.cpp)
target_link_libraries(test_crypto GTest::gtest_main)
add_test(NAME CryptoTests COMMAND test_crypto)

add_executable(test_utils test_utils.cpp)
target_link_libraries(test_utils GTest::gtest_main)
add_test(NAME UtilsTests COMMAND test_utils)

add_executable(test_web_utils test_web_utils.cpp)
target_link_libraries(test_web_utils GTest::gtest_main)
add_test(NAME WebUtilsTests COMMAND test_web_utils)

add_executable(test_cover_logic test_cover_logic.cpp)
target_link_libraries(test_cover_logic GTest::gtest_main)
add_test(NAME CoverLogicTests COMMAND test_cover_logic)

add_executable(test_light_logic test_light_logic.cpp)
target_link_libraries(test_light_logic GTest::gtest_main)
add_test(NAME LightLogicTests COMMAND test_light_logic)

add_executable(test_dispatch_logic test_dispatch_logic.cpp)
target_link_libraries(test_dispatch_logic GTest::gtest_main)
add_test(NAME DispatchLogicTests COMMAND test_dispatch_logic)

add_executable(test_poll_logic test_poll_logic.cpp)
target_link_libraries(test_poll_logic GTest::gtest_main)
add_test(NAME PollLogicTests COMMAND test_poll_logic)

add_executable(test_golden_vectors test_golden_vectors.cpp)
target_link_libraries(test_golden_vectors GTest::gtest_main)
add_test(NAME GoldenVectorTests COMMAND test_golden_vectors)

include(GoogleTest)
gtest_discover_tests(test_crypto)
gtest_discover_tests(test_utils)
gtest_discover_tests(test_web_utils)
gtest_discover_tests(test_cover_logic)
gtest_discover_tests(test_light_logic)
gtest_discover_tests(test_dispatch_logic)
gtest_discover_tests(test_poll_logic)
gtest_discover_tests(test_golden_vectors)
