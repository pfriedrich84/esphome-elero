add_executable(test_crypto test_crypto.cpp)
target_link_libraries(test_crypto GTest::gtest_main)
add_test(NAME CryptoTests COMMAND test_crypto)

add_executable(test_utils test_utils.cpp)
target_link_libraries(test_utils GTest::gtest_main)
add_test(NAME UtilsTests COMMAND test_utils)

add_executable(test_web_utils test_web_utils.cpp)
target_link_libraries(test_web_utils GTest::gtest_main)
add_test(NAME WebUtilsTests COMMAND test_web_utils)

include(GoogleTest)
gtest_discover_tests(test_crypto)
gtest_discover_tests(test_utils)
gtest_discover_tests(test_web_utils)
