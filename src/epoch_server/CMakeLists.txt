CMAKE_MINIMUM_REQUIRED(VERSION 3.6 FATAL_ERROR)

ADD_EXECUTABLE(epoch_server
        "epoch_server.cpp"
        "raft/chain_closure.cpp"
        "raft/chain.cpp"
        "raft/chain_client.cpp"
        "oe_consensus_manager.cpp"
        ${SRC_COMMON_DATA_STRUCTURE}
        ${SRC_COMMON_MSP}
        ${SRC_COMMON_ZMQ}
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

TARGET_LINK_LIBRARIES(epoch_server
        ${LIBRARY_DEPENDENCIES}
        ${PROTOBUF_LIBRARY_DEBUG})