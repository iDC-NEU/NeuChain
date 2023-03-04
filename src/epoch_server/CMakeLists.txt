CMAKE_MINIMUM_REQUIRED(VERSION 3.6 FATAL_ERROR)

ADD_EXECUTABLE(epoch_server
        "epoch_server.cpp"
        "raft/chain_closure.cpp"
        "raft/chain.cpp"
        "raft/chain_client.cpp"
        "ev_consensus_manager.cpp"
        ${SRC_COMMON_DATA_STRUCTURE}
        ${SRC_COMMON_MSP}
        ${SRC_COMMON_ZMQ})

TARGET_LINK_LIBRARIES(epoch_server
        ${LIBRARY_DEPENDENCIES}
        ${PROTOBUF_LIBRARY_DEBUG})