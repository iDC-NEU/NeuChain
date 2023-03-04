//
// Created by peng on 2021/3/18.
//

#include "common/zmq/zmq_client.h"
#include "glog/logging.h"

ZMQClient::ZMQClient(const std::string &ip, const std::string &port, zmq::socket_type type) {
    context = new zmq::context_t;
    socket = new zmq::socket_t(*context, type);
    // init for spec socket type
    // 1. for sub and x_sub
    if (type == zmq::socket_type::sub || type == zmq::socket_type::xsub){
        socket->set(zmq::sockopt::subscribe,"");
        socket->set(zmq::sockopt::rcvhwm,0);
    }
    else if (type == zmq::socket_type::req || type == zmq::socket_type::rep) {

    }
    else if (type == zmq::socket_type::pub || type == zmq::socket_type::xpub) {

    }
    else {
        LOG(WARNING) << "unsupported recv type, may cause unpredictable error!";
    }
    std::string address = "tcp://"+ ip +":" + port;
    DLOG(INFO) << "remote server address: " << address;
    socket->connect(address);
}

ZMQClient::~ZMQClient() {
    delete socket;
    delete context;
}

std::optional<size_t> ZMQClient::sendRequest(zmq::const_buffer& request) {
    return socket->send(request);
}

std::optional<size_t> ZMQClient::sendRequest(const std::string &request) {
    zmq::const_buffer buffer(request.c_str(), request.size());
    return sendRequest(buffer);
}

std::optional<size_t> ZMQClient::getReply(zmq::message_t &reply) {
    return socket->recv(reply);
}

std::optional<size_t> ZMQClient::getReply(std::string &reply) {
    zmq::message_t message;
    std::optional<size_t> size = socket->recv(message);
    reply = message.to_string();
    return size;
}

std::optional<size_t> ZMQClient::sendRequest() {
    std::string drop;
    return sendRequest(drop);
}

std::optional<size_t> ZMQClient::getReply() {
    std::string drop;
    return getReply(drop);
}
