//
// Created by peng on 2021/3/18.
//

#include "common/zmq/zmq_server.h"
#include "glog/logging.h"

ZMQServer::ZMQServer(const std::string &ip, const std::string &port, zmq::socket_type type) {
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
    DLOG(INFO) << "listen address: " << address;
    socket->bind(address);
}

std::optional<size_t> ZMQServer::getRequest(zmq::message_t& request) {
    return socket->recv(request);
}

std::optional<size_t> ZMQServer::sendReply(zmq::const_buffer &reply) {
    return socket->send(reply);
}

std::optional<size_t> ZMQServer::sendReply(const std::string &reply) {
    zmq::const_buffer buffer(reply.c_str(), reply.size());
    return socket->send(buffer);
}

std::optional<size_t> ZMQServer::getRequest(std::string &request) {
    zmq::message_t message;
    std::optional<size_t> size = socket->recv(message);
    request = message.to_string();
    return size;
}

ZMQServer::~ZMQServer() {
    delete socket;
    delete context;
}

std::optional<size_t> ZMQServer::getRequest() {
    std::string drop;
    return getRequest(drop);
}

std::optional<size_t> ZMQServer::sendReply() {
    std::string drop;
    return sendReply(drop);
}
