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

add_executable(test_command_delivery test_command_delivery.cpp)
target_link_libraries(test_command_delivery GTest::gtest_main)
add_test(NAME CommandDeliveryTests COMMAND test_command_delivery)

add_executable(test_group_delivery_policy test_group_delivery_policy.cpp)
target_link_libraries(test_group_delivery_policy GTest::gtest_main)
add_test(NAME GroupDeliveryPolicyTests COMMAND test_group_delivery_policy)

add_executable(test_group_position_logic test_group_position_logic.cpp)
target_link_libraries(test_group_position_logic GTest::gtest_main)
add_test(NAME GroupPositionLogicTests COMMAND test_group_position_logic)

add_executable(test_poll_logic test_poll_logic.cpp)
target_link_libraries(test_poll_logic GTest::gtest_main)
add_test(NAME PollLogicTests COMMAND test_poll_logic)

add_executable(test_radio_state_logic test_radio_state_logic.cpp)
target_link_libraries(test_radio_state_logic GTest::gtest_main)
add_test(NAME RadioStateLogicTests COMMAND test_radio_state_logic)

add_executable(test_runtime_blind_logic test_runtime_blind_logic.cpp)
target_link_libraries(test_runtime_blind_logic GTest::gtest_main)
add_test(NAME RuntimeBlindLogicTests COMMAND test_runtime_blind_logic)

add_executable(test_packet_parser test_packet_parser.cpp)
target_link_libraries(test_packet_parser GTest::gtest_main)
add_test(NAME PacketParserTests COMMAND test_packet_parser)

add_executable(test_packet_replay test_packet_replay.cpp)
target_compile_definitions(test_packet_replay PRIVATE RF_REPLAY_FIXTURE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/../fixtures/rf_replay\")
target_link_libraries(test_packet_replay GTest::gtest_main)
add_test(NAME PacketReplayTests COMMAND test_packet_replay)

add_executable(test_golden_vectors test_golden_vectors.cpp)
target_link_libraries(test_golden_vectors GTest::gtest_main)
add_test(NAME GoldenVectorTests COMMAND test_golden_vectors)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(HOST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../host)
set(ENTITY_FIXTURE_DIR ${CMAKE_CURRENT_BINARY_DIR}/entities)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  ${HOST_DIR}/generate_cover_fixture.py
  ${CMAKE_CURRENT_SOURCE_DIR}/../../components/elero/elero.h
  ${CMAKE_CURRENT_SOURCE_DIR}/../../components/elero/cover/EleroCover.h
  ${CMAKE_CURRENT_SOURCE_DIR}/../../components/elero/cover/EleroCover.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../components/elero_group/EleroGroupCover.h
  ${CMAKE_CURRENT_SOURCE_DIR}/../../components/elero_group/EleroGroupCover.cpp)
execute_process(COMMAND ${Python3_EXECUTABLE} ${HOST_DIR}/generate_cover_fixture.py
  ${CMAKE_CURRENT_SOURCE_DIR}/../.. ${ENTITY_FIXTURE_DIR}
  RESULT_VARIABLE FIXTURE_RESULT)
if(NOT FIXTURE_RESULT EQUAL 0)
  message(FATAL_ERROR "Cannot generate production entity host fixture")
endif()
add_executable(test_cover_delivery test_cover_delivery.cpp
  ${ENTITY_FIXTURE_DIR}/EleroCover.cpp ${ENTITY_FIXTURE_DIR}/EleroGroupCover.cpp)
target_include_directories(test_cover_delivery PRIVATE ${ENTITY_FIXTURE_DIR} ${HOST_DIR})
target_compile_definitions(test_cover_delivery PRIVATE USE_TEXT_SENSOR)
target_link_libraries(test_cover_delivery GTest::gtest_main)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  ${HOST_DIR}/generate_radio_fixture.py
  ${CMAKE_CURRENT_SOURCE_DIR}/../../components/elero/elero_cc1101.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../components/elero/elero.cpp)
execute_process(COMMAND ${Python3_EXECUTABLE} ${HOST_DIR}/generate_radio_fixture.py
  ${CMAKE_CURRENT_SOURCE_DIR}/../.. ${ENTITY_FIXTURE_DIR}
  RESULT_VARIABLE RADIO_FIXTURE_RESULT)
if(NOT RADIO_FIXTURE_RESULT EQUAL 0)
  message(FATAL_ERROR "Cannot generate production radio host fixture")
endif()
add_executable(test_radio_delivery test_radio_delivery.cpp ${ENTITY_FIXTURE_DIR}/radio.cpp)
target_include_directories(test_radio_delivery PRIVATE ${ENTITY_FIXTURE_DIR} ${HOST_DIR})
target_link_libraries(test_radio_delivery GTest::gtest_main)

add_executable(test_rx_fifo test_rx_fifo.cpp)
target_link_libraries(test_rx_fifo GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(test_radio_delivery)
gtest_discover_tests(test_rx_fifo)
gtest_discover_tests(test_cover_delivery)
gtest_discover_tests(test_crypto)
gtest_discover_tests(test_utils)
gtest_discover_tests(test_web_utils)
gtest_discover_tests(test_cover_logic)
gtest_discover_tests(test_light_logic)
gtest_discover_tests(test_command_delivery)
gtest_discover_tests(test_group_delivery_policy)
gtest_discover_tests(test_group_position_logic)
gtest_discover_tests(test_poll_logic)
gtest_discover_tests(test_radio_state_logic)
gtest_discover_tests(test_runtime_blind_logic)
gtest_discover_tests(test_packet_parser)
gtest_discover_tests(test_packet_replay)
gtest_discover_tests(test_golden_vectors)
