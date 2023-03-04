CMAKE_MINIMUM_REQUIRED(VERSION 3.6 FATAL_ERROR)
ADD_EXECUTABLE(
        reserve_table_test
        mvcc_reserve_table_test.cpp
        reserve_table_impl_test.cpp
        reserve_table_provider_test.cpp
)

TARGET_LINK_LIBRARIES(
        reserve_table_test
        gtest
        gtest_main
        lib_block_server
        ${LIBRARY_DEPENDENCIES}
)
