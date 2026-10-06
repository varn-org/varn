# The tests of the scheduler link only the runtime without Lua, so their build proves that a C++ program uses the scheduler, the log, data views and the HTTP client with no Lua linked, while no source of that runtime can reach a header of Lua.
# They take googletest from the list file of the C++ tests, which the root project adds first.

# A pumped event loop watches no socket and its pools may run inside the pump, so its build runs the tests of the pumped loop, while the tests of the pools need their threads and the loop of libuv.
set(VARN_SCHEDULER_TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/scheduler-test.cpp" "${CMAKE_CURRENT_LIST_DIR}/workledger-test.cpp")
if(VARN_EVENT_LOOP_DRIVER STREQUAL "PUMP")
    list(APPEND VARN_SCHEDULER_TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/pumped-loop-test.cpp")
else()
    list(APPEND VARN_SCHEDULER_TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/task-pool-test.cpp" "${CMAKE_CURRENT_LIST_DIR}/task-pool-allocation-test.cpp")
endif()

add_executable(varn-scheduler-tests ${VARN_SCHEDULER_TEST_SOURCES})
target_link_libraries(varn-scheduler-tests PRIVATE varn-runtime GTest::gtest_main)

add_test(NAME varn-scheduler-tests COMMAND varn-scheduler-tests)
