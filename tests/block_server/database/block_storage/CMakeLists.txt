CMAKE_MINIMUM_REQUIRED(VERSION 3.6 FATAL_ERROR)
ADD_EXECUTABLE(
        db_storage_test
        level_db_storage_test.cpp
)

ADD_EXECUTABLE(
        block_server_test
        block_server_test.cpp
        ${SRC_CLIENT_PROXY_MOCK}
)

TARGET_LINK_LIBRARIES(
        db_storage_test
        gtest
        gtest_main
        lib_block_server
        ${LIBRARY_DEPENDENCIES}
)

TARGET_LINK_LIBRARIES(
        block_server_test
        gtest
        gtest_main
        lib_block_server
        ${LIBRARY_DEPENDENCIES}
)
