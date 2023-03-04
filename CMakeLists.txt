cmake_minimum_required(VERSION 2.8.10)
project(neu_blockchain)

option(LINK_SO "Whether examples are linked dynamically" ON)
option(LINK_TCMALLOC "Link tcmalloc if possible" ON)

set(CMAKE_PREFIX_PATH ${OUTPUT_PATH})

INCLUDE_DIRECTORIES(include)

include(FindThreads)
include(FindProtobuf)

if (NOT PROTOBUF_PROTOC_EXECUTABLE)
    get_filename_component(PROTO_LIB_DIR ${PROTOBUF_LIBRARY} DIRECTORY)
    set(PROTOBUF_PROTOC_EXECUTABLE "${PROTO_LIB_DIR}/../bin/protoc")
endif ()

PROTOBUF_GENERATE_CPP(PROTO_SRC PROTO_HEADER
        ./include/proto/transaction.proto
        ./include/proto/chaincode.proto
        ./include/proto/common.proto
        ./include/proto/kv_rwset.proto
        ./include/proto/comm.proto
        ./include/proto/eov.proto
        ./include/proto/tpc-c.proto
        ./include/proto/db_test_chaincode.proto
        ./include/proto/block.proto
        ./include/proto/chain.proto
        )
# include PROTO_HEADER
include_directories(${CMAKE_CURRENT_BINARY_DIR})

find_path(BRPC_INCLUDE_PATH NAMES brpc/server.h)
if (LINK_SO)
    find_library(BRPC_LIB NAMES brpc)
    find_library(BRAFT_LIB NAMES braft)
else ()
    find_library(BRPC_LIB NAMES libbrpc.a brpc)
    find_library(BRAFT_LIB NAMES libbraft.a braft)
endif ()

#3. yaml
FIND_PACKAGE(yaml-cpp REQUIRED)
FIND_PATH(YAML_INCLUDE_PATH NAMES yaml-cpp/yaml.h)
FIND_LIBRARY(YAML_LIB NAMES yaml-cpp)
if ((NOT YAML_INCLUDE_PATH) OR (NOT YAML_LIB))
    MESSAGE(FATAL_ERROR "Fail to find yaml-cpp")
endif ()
INCLUDE_DIRECTORIES(${YAML_INCLUDE_DIRS})

# 1. zmq
FIND_PACKAGE(cppzmq)
FIND_PATH(ZMQ_INCLUDE_PATH zmq.hpp)
FIND_LIBRARY(ZMQ_LIB NAMES libzmq.a zmq)
if ((NOT ZMQ_INCLUDE_PATH) OR (NOT ZMQ_LIB))
    MESSAGE(FATAL_ERROR "Fail to find zmq")
endif ()
INCLUDE_DIRECTORIES(${ZMQ_INCLUDE_PATH})

# brpc
if ((NOT BRPC_INCLUDE_PATH) OR (NOT BRPC_LIB))
    message(FATAL_ERROR "Fail to find brpc")
endif ()
include_directories(${BRPC_INCLUDE_PATH})

# braft
find_path(BRAFT_INCLUDE_PATH NAMES braft/raft.h)
if ((NOT BRAFT_INCLUDE_PATH) OR (NOT BRAFT_LIB))
    message(FATAL_ERROR "Fail to find braft")
endif ()
include_directories(${BRAFT_INCLUDE_PATH})

# we do not link gflag library, it has already included in protobuf
find_path(GFLAGS_INCLUDE_PATH gflags/gflags.h)
find_library(GFLAGS_LIBRARY NAMES gflags libgflags)
if ((NOT GFLAGS_INCLUDE_PATH) OR (NOT GFLAGS_LIBRARY))
    message(FATAL_ERROR "Fail to find gflags")
endif ()
include_directories(${GFLAGS_INCLUDE_PATH})

# glog
find_package(glog REQUIRED)

if (NOT LINK_SO)
    execute_process(
            COMMAND bash -c "grep \"namespace [_A-Za-z0-9]\\+ {\" ${GFLAGS_INCLUDE_PATH}/gflags/gflags_declare.h | head -1 | awk '{print $2}' | tr -d '\n'"
            OUTPUT_VARIABLE GFLAGS_NS
    )
    if (${GFLAGS_NS} STREQUAL "GFLAGS_NAMESPACE")
        execute_process(
                COMMAND bash -c "grep \"#define GFLAGS_NAMESPACE [_A-Za-z0-9]\\+\" ${GFLAGS_INCLUDE_PATH}/gflags/gflags_declare.h | head -1 | awk '{print $3}' | tr -d '\n'"
                OUTPUT_VARIABLE GFLAGS_NS
        )
    endif ()

    set(CMAKE_CPP_FLAGS "-DGFLAGS_NS=${GFLAGS_NS}")
    SET(LIBRARY_DEPENDENCIES
            ${LIBRARY_DEPENDENCIES}
            ${GFLAGS_LIBRARY}
            libgflags.a
            )
endif ()

if (LINK_TCMALLOC)
    find_path(GPERFTOOLS_INCLUDE_DIR NAMES gperftools/heap-profiler.h)
    find_library(GPERFTOOLS_LIBRARIES NAMES tcmalloc_and_profiler)
    if (GPERFTOOLS_INCLUDE_DIR AND GPERFTOOLS_LIBRARIES)
        set(CMAKE_CXX_FLAGS "-DBRPC_ENABLE_CPU_PROFILER")
        include_directories(${GPERFTOOLS_INCLUDE_DIR})
    else ()
        set(GPERFTOOLS_LIBRARIES "")
    endif ()
endif ()

SET(CMAKE_CXX_FLAGS_DEBUG
        "${CMAKE_CXX_FLAGS_DEBUG} \
    ${CMAKE_CXX_FLAGS_COMMON} \
    -g -O0 -fprofile-arcs -ftest-coverage")

SET(CMAKE_CXX_FLAGS_RELEASE
        "${CMAKE_CXX_FLAGS_RELEASE} \
    ${CMAKE_CXX_FLAGS_COMMON} \
    -O3")

set(CMAKE_CXX_FLAGS "${CMAKE_CPP_FLAGS} ${CMAKE_CXX_FLAGS} -D__const__= -pipe -W -Wall -Wno-unused-parameter -fPIC -fno-omit-frame-pointer")
# release version
# set(CMAKE_CXX_FLAGS "${CMAKE_CPP_FLAGS} ${CMAKE_CXX_FLAGS} -DNDEBUG -O2 -D__const__= -pipe -W -Wall -Wno-unused-parameter -fPIC -fno-omit-frame-pointer")
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # require at least gcc 4.8
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.8)
        message(FATAL_ERROR "GCC is too old, please install a newer version supporting C++11")
    endif ()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # require at least clang 3.3
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.3)
        message(FATAL_ERROR "Clang is too old, please install a newer version supporting C++11")
    endif ()
else ()
    message(WARNING "You are using an unsupported compiler! Compilation has only been tested with Clang and GCC.")
endif ()

if (CMAKE_VERSION VERSION_LESS "3.1.3")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
    endif ()
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
    endif ()
else ()
    SET(CMAKE_CXX_STANDARD 17)
    SET(CMAKE_CXX_STANDARD_REQUIRED on)
endif ()

# leveldb
find_path(LEVELDB_INCLUDE_PATH NAMES leveldb/db.h)
find_library(LEVELDB_LIB NAMES leveldb)
if ((NOT LEVELDB_INCLUDE_PATH) OR (NOT LEVELDB_LIB))
    message(FATAL_ERROR "Fail to find leveldb")
endif ()
include_directories(${LEVELDB_INCLUDE_PATH})

ADD_LIBRARY(aria_protobuf ${PROTO_HEADER} ${PROTO_SRC})

# common file
FILE(GLOB SRC_COMMON_DATA_STRUCTURE
        "src/common/*.cpp"
        "src/common/*.c")

FILE(GLOB SRC_COMMON_MERKLE_TREE
        "src/common/merkle_tree/*.c")

FILE(GLOB SRC_COMMON_MSP
        "src/common/msp/*.cpp")

FILE(GLOB SRC_COMMON_TX_PAYLOAD
        ${SRC_COMMON_DATA_STRUCTURE}
        "src/common/payload/*.cpp"
        "src/block_server/transaction/mock/mock_transaction.cpp")

FILE(GLOB SRC_COMMON_ZMQ
        "src/common/zmq/*.cpp")

# client proxy
FILE(GLOB SRC_CLIENT_PROXY
        "src/block_server/comm/client_proxy/*.cpp")

FILE(GLOB SRC_CLIENT_PROXY_MOCK
        "src/block_server/comm/mock/*.cpp")

# query interface
FILE(GLOB SRC_QUERY
        "src/block_server/comm/query/*.cpp")

# block server
FILE(GLOB SRC_BLOCK_SERVER_COORDINATOR
        "src/block_server/coordinator/*.cpp")

FILE(GLOB SRC_BLOCK_SERVER_WORKER
        "src/block_server/worker/*.cpp"
        "src/block_server/worker/impl/*.cpp"
        "src/block_server/worker/chaincode/*.cpp")

FILE(GLOB SRC_BLOCK_SERVER_DATABASE
        "src/block_server/database/*.cpp"
        "src/block_server/database/impl/*.cpp"
        "src/block_server/database/orm/*.cpp"
        "src/block_server/database/orm/impl/*.cpp")

FILE(GLOB SRC_BLOCK_SERVER_TRANSACTION
        "src/block_server/transaction/impl/*.cpp")

FILE(GLOB SRC_BLOCK_SERVER_TRANSACTION_MOCK
        "src/block_server/transaction/mock/*.cpp")

FILE(GLOB SRC_BLOCK_SERVER_RESERVE_TABLE
        "src/block_server/reserve_table/*.cpp"
        "src/block_server/reserve_table/mock/*.cpp")

FILE(GLOB SRC_BLOCK_BENCH
        "src/user/block_bench/*.cpp")

SET(LIBRARY_DEPENDENCIES
        ${LIBRARY_DEPENDENCIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${GLOG_LIBRARY}
        ${PROTOBUF_LIBRARY}
        ${GPERFTOOLS_LIBRARIES}
        ${LEVELDB_LIB}
        ${BRAFT_LIB}
        ${BRPC_LIB}
        ${YAML_LIB}
        ${ZMQ_LIB}
        glog::glog
        aria_protobuf
        crypto
        pqxx.a
        pq.so
        ssl
        dl
        rt
        z
        )

# runnable src
ADD_SUBDIRECTORY(src/block_server)
ADD_SUBDIRECTORY(src/epoch_server)
ADD_SUBDIRECTORY(src/user)
ADD_SUBDIRECTORY(tests)
