//
// Created by peng on 2021/3/18.
//

#ifndef NEUBLOCKCHAIN_ZMQ_CLIENT_H
#define NEUBLOCKCHAIN_ZMQ_CLIENT_H

#include <string>
#include <optional>
#include <zmq.hpp>

class ZMQClient {
public:
    ZMQClient(const std::string& ip, const std::string& port, zmq::socket_type type = zmq::socket_type::req);
    virtual ~ZMQClient();

    std::optional<size_t> sendRequest();
    std::optional<size_t> sendRequest(zmq::const_buffer& request);
    std::optional<size_t> sendRequest(const std::string& request);
    std::optional<size_t> getReply();
    std::optional<size_t> getReply(zmq::message_t& reply);
    std::optional<size_t> getReply(std::string& reply);

private:
    zmq::context_t* context;
    zmq::socket_t* socket;
};


#endif //NEUBLOCKCHAIN_ZMQ_CLIENT_H
