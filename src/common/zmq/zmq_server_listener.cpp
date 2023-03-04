//
// Created by peng on 2021/4/23.
//

#include "common/zmq/zmq_server_listener.h"
#include "common/yaml_config.h"
#include <string>

ServerListener::ServerListener(std::string _port)
        :port(std::move(_port)), localIP(YAMLConfig::getInstance()->getLocalBlockServerIP()) { }

ServerListener::~ServerListener() {
    // must wait until all client destroyed
    for(auto* client: remoteReceiverThreads) {
        client->join();
        delete client;
    }
    for (auto* client: clients) {
        delete client;
    }
}
