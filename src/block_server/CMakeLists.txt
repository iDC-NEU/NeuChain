CMAKE_MINIMUM_REQUIRED(VERSION 3.6 FATAL_ERROR)

ADD_EXECUTABLE(block_server
        "server.cpp"
        ${SRC_COMMON_DATA_STRUCTURE}
        ${SRC_COMMON_MERKLE_TREE}
        ${SRC_COMMON_MSP}
        ${SRC_COMMON_TX_PAYLOAD}
        ${SRC_COMMON_ZMQ}
        ${SRC_CLIENT_PROXY_MOCK}
        ${SRC_QUERY}
        ${SRC_BLOCK_SERVER_COORDINATOR}
        ${SRC_BLOCK_SERVER_WORKER}
        ${SRC_BLOCK_SERVER_DATABASE}
        ${SRC_BLOCK_SERVER_TRANSACTION}
        ${SRC_BLOCK_SERVER_RESERVE_TABLE})

# Link the project libraries to the executable
TARGET_LINK_LIBRARIES(block_server
        ${LIBRARY_DEPENDENCIES}
        ${PROTOBUF_LIBRARY_DEBUG})

ADD_EXECUTABLE(block_server_test_comm
        "server.cpp"
        "../epoch_server/raft/chain_closure.cpp"
        "../epoch_server/raft/chain.cpp"
        "../epoch_server/raft/chain_client.cpp"
        "../epoch_server/ev_consensus_manager.cpp"
        "../user/block_bench/ycsb_workload.cpp"
        ${SRC_COMMON_DATA_STRUCTURE}
        ${SRC_COMMON_MERKLE_TREE}
        ${SRC_COMMON_MSP}
        ${SRC_COMMON_TX_PAYLOAD}
        ${SRC_COMMON_ZMQ}
        ${SRC_CLIENT_PROXY}
        ${SRC_QUERY}
        ${SRC_BLOCK_SERVER_COORDINATOR}
        ${SRC_BLOCK_SERVER_WORKER}
        ${SRC_BLOCK_SERVER_DATABASE}
        ${SRC_BLOCK_SERVER_TRANSACTION}
        ${SRC_BLOCK_SERVER_RESERVE_TABLE})

TARGET_LINK_LIBRARIES(block_server_test_comm
        ${LIBRARY_DEPENDENCIES}
        ${PROTOBUF_LIBRARY_DEBUG})