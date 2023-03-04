//
// Created by peng on 2021/4/23.
//

#ifndef NEUBLOCKCHAIN_ZMQ_SERVER_LISTENER_H
#define NEUBLOCKCHAIN_ZMQ_SERVER_LISTENER_H

#include "common/zmq/zmq_client.h"
#include <thread>
#include <set>
#include <vector>

/*
 * This class is used for receiving multi server broadcast messages
 * The class create multi client threads, each connect to a server in the list.
 *
 * The usage is as follow:
 * ServerListener(&ClassName::funcName, instance, ipSet, port);
 *
 * The callable funcName must be implemented as follow:
 * className::funcName(const std::string& ip, ZMQClient *client);
 *
 */

class ServerListener {
public:
    explicit ServerListener(std::string _port);

    template<typename ClassName, typename ClassInstance>
    inline ServerListener(ClassName&& f, ClassInstance&& args,
                   const std::vector<std::string>& ips, const std::string& _port) :ServerListener(_port) {
        for(const auto& ip: ips) {
            if(ip == localIP) {
                continue;
            }
            addServerListener(std::forward<ClassName>(f), std::forward<ClassInstance>(args), ip);
        }
    }

    ~ServerListener();

    template<typename ClassName, typename ClassInstance>
    inline bool addServerListener(ClassName&& f, ClassInstance&& args, const std::string& ip) {
        if (remoteIPs.count(ip))
            return false;
        remoteIPs.insert(ip);

        auto* client = new ZMQClient(ip, port, zmq::socket_type::sub);
        clients.push_back(client);
        remoteReceiverThreads.push_back(
                new std::thread(std::forward<ClassName>(f), std::forward<ClassInstance>(args), ip, client)
        );
        return true;
    }

private:
    const std::string port;
    const std::string localIP;
    std::set<std::string> remoteIPs;
    std::vector<std::thread*> remoteReceiverThreads;
    std::vector<ZMQClient*> clients;

};


#endif //NEUBLOCKCHAIN_ZMQ_SERVER_LISTENER_H
