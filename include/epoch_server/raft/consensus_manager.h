//
// Created by peng on 2021/9/9.
//

#ifndef NEUBLOCKCHAIN_CONSENSUS_MANAGER_H
#define NEUBLOCKCHAIN_CONSENSUS_MANAGER_H

#include <brpc/server.h>
#include <bthread/bthread.h>
#include <common/yaml_config.h>
#include "common/concurrent_queue/blocking_concurrent_queue.hpp"
#include "common/concurrent_queue/blocking_mpmc_queue.h"
#include "common/timer.h"
#include "common/thread_pool.h"
#include "common/msp/crypto_helper.h"
#include "chain.h"
#include "chain_client.h"

struct consensus_param {
    std::string initial_conf;
    std::string raft_ip;
    double duration;  // only need for epoch server
    int batch_size;
    int raft_port;
    int group_id;
    int request_receiver_port;  // only need for epoch server
    brpc::Server* server;   // the unique server pointer
};

template<class T>
class ConsensusManager {
public:
    ~ConsensusManager();

    void print_status() { client->print_status(); }

    // construct init config
    static consensus_param GenerateEpochServerConsensusParam(const std::string &raft_ip, int raft_port, int batch_size, double duration, int request_receiver_port, int group_id = 0) {
        std::string initial_conf;   // 127.0.1.1:8100:0,127.0.1.1:8101:0,127.0.1.1:8102:0
        for (const auto& ip : YAMLConfig::getInstance()->getEpochServerIPs()) {
            initial_conf += ip + ":" + std::to_string(raft_port) + ":0,";
        }
        initial_conf.pop_back();
        LOG(INFO) << "initial_conf " << initial_conf << ".";
        consensus_param param;
        param.initial_conf = initial_conf;
        param.duration = duration;
        param.batch_size = batch_size;
        param.raft_ip = raft_ip;
        param.raft_port = raft_port;
        param.group_id = group_id;
        param.request_receiver_port = request_receiver_port;
        return param;
    }

    // construct init config
    static consensus_param GenerateBlockServerConsensusParam(const std::string &raft_ip, int raft_port, int group_id = 0) {
        std::string initial_conf;   // 127.0.1.1:8100:0,127.0.1.1:8101:0,127.0.1.1:8102:0
        for (const auto& ip : YAMLConfig::getInstance()->getBlockServerIPs()) {
            initial_conf += ip + ":" + std::to_string(raft_port) + ":" + std::to_string(group_id) + ",";
        }
        initial_conf.pop_back();
        LOG(INFO) << "initial_conf " << initial_conf << ".";
        consensus_param param;
        param.initial_conf = initial_conf;
        param.duration = -1;
        param.batch_size = 1;
        param.raft_ip = raft_ip;
        param.raft_port = raft_port;
        param.group_id = group_id;
        param.request_receiver_port = -1;
        return param;
    }

    static void StartBrpcServer(brpc::Server* server, int raft_port) {
        // It's recommended to start the server before Chain is started to avoid
        // the case that it becomes the leader while the service is unreachable by clients.
        // Notice the default options of server is used here. Check out details from
        // the doc of brpc if you would like change some options;
        if (server->Start(raft_port, nullptr) != 0) {
            LOG(ERROR) << "Fail to start Server";
            CHECK(false);
        }
        LOG(INFO) << "Brpc service is running on " << server->listen_address();
    }

    static void BrpcAddService(brpc::Server* server, int raft_port, google::protobuf::Service* service) {
        // Add your service into RPC server
        if (server->AddService(service, brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
            LOG(ERROR) << "Fail to add service";
            CHECK(false);
        }
        // raft can share the same RPC server. Notice the second parameter, because
        // adding services into a running server is not allowed and the listen
        // address of this server is impossible to get before the server starts. You
        // have to specify the address of the server.
        if (braft::add_service(server, raft_port) != 0) {
            LOG(ERROR) << "Fail to add raft service";
            CHECK(false);
        }

    }

    void startChainInstance(const std::string &initial_conf, const std::string &ip, int raft_port, const char *group_name, int group_id) {
        // It's ok to start Chain;
        if (!chain->add_chain_instance(initial_conf, ip, raft_port, group_name, group_id)) {
            LOG(ERROR) << "Fail to start Chain";
            CHECK(false);
        }
        // start client
        client = std::make_unique<raft::ChainClient>(group_name);
        client->init_config(initial_conf);
        if (bthread_start_background(&raft_sender_tid, nullptr, send_to_raft, this) != 0) {
            LOG(ERROR) << "Fail to create sender thread " << raft_sender_tid;
            CHECK(false);
        }

        if (bthread_start_background(&raft_receiver_tid, nullptr, ConsensusManager::receiver_from_raft, this) != 0) {
            LOG(ERROR) << "Fail to create block construction thread " << raft_receiver_tid;
            CHECK(false);
        }
        if (bthread_start_background(&user_receiver_tid, nullptr, ConsensusManager::receiver_from_user, this) != 0) {
            LOG(ERROR) << "Fail to create user receiver thread " << user_receiver_tid;
            CHECK(false);
        }
    }

protected:
    template<typename U>
    inline void get_message_from_raft(U &item) { consensus_request_queue.template pop(item); }

    template<typename U>
    inline void push_message_to_raft(U &&item) { request_queue.template push(std::forward<U>(item)); }
private:
    friend T;

    ConsensusManager(const std::string &initial_conf, const std::string &ip, int raft_port, const char *group_name, brpc::Server* server, int group_id, int batch_size = 1);

    // multi-raft version
    ConsensusManager(raft::Chain* chainPtr, raft::ChainService* servicePtr, int group_id, int batch_size = 1);

    static void *receiver_from_raft(void *ptr);

    static void *receiver_from_user(void *ptr);

    static void *send_to_raft(void *ptr);

private:
    int batch_size;
    int leader_group_id;

protected:
    raft::Chain* chain;
    raft::ChainService* service;

private:
    std::unique_ptr<raft::ChainClient> client;
    bthread_t user_receiver_tid{};
    bthread_t raft_receiver_tid{};
    bthread_t raft_sender_tid{};
    BlockingMPMCQueue<std::string> consensus_request_queue;
    BlockingMPMCQueue<std::string> request_queue;
};

template<class T>
ConsensusManager<T>::ConsensusManager(const std::string &initial_conf, const std::string &ip, int raft_port, const char *group_name, brpc::Server* server, int group_id, int batch_size)
        :batch_size(batch_size), leader_group_id(group_id) {
    chain = new raft::Chain;
    chain->setLeaderGroupId(group_id);
    service = new raft::ChainServiceImpl(chain);
    // TODO: free memory
    // Generally you only need one Server.
    // server = std::make_unique<brpc::Server>();

    BrpcAddService(server,  raft_port, service);

    StartBrpcServer(server, raft_port);

    startChainInstance(initial_conf, ip, raft_port, group_name, group_id);
}

template<class T>
ConsensusManager<T>::ConsensusManager(raft::Chain* chainPtr, raft::ChainService* servicePtr, int group_id, int batch_size)
        :batch_size(batch_size), leader_group_id(group_id), chain(chainPtr), service(servicePtr) {
    // BrpcAddService(server,  raft_port, service.get());

    // StartBrpcServer(server.get(), raft_port);

    // startChainInstance(initial_conf, ip, raft_port, group_name);
}

template<class T>
ConsensusManager<T>::~ConsensusManager() {
    LOG(INFO) << "Chain service is going to quit";
    bthread_join(user_receiver_tid, nullptr);
    bthread_join(raft_receiver_tid, nullptr);
    bthread_join(raft_sender_tid, nullptr);
    // Stop counter before server
    chain->shutdown();
    // Wait until all the processing tasks are over.
    chain->join();
}

template<class T>
void *ConsensusManager<T>::send_to_raft(void *ptr) {
    auto *self = static_cast<ConsensusManager<T> *>(ptr);
    // get leader func for client to send request
    std::function<braft::PeerId()> get_leader_by_rpc;
    if (CryptoHelper::getServerType() == CryptoHelper::ServerType::epochServer) {
        get_leader_by_rpc = [self]() -> braft::PeerId {
            braft::PeerId leader = self->chain->get_leader_from_node(self->leader_group_id);
            // obtain leader
            while (butil::ip2str(leader.addr.ip).c_str() == std::string("0.0.0.0")) {
                LOG(INFO) << "Electing leader, waiting...";
                BlockBench::Timer::sleep(0.1);
                leader = self->chain->get_leader_from_node(self->leader_group_id);
            }
            // raft::ChainClient::update_leader_in_route_table(leader);
            return leader;
        };
    } else {
        get_leader_by_rpc = [self]() -> braft::PeerId {
            butil::EndPoint addr;
            std::string ipStr = YAMLConfig::getInstance()->getBlockServerIPs()[self->leader_group_id] + ":" + std::to_string(18100);
            str2endpoint(ipStr.data(), &addr);
            return braft::PeerId(addr, self->leader_group_id);
        };
    }
    if (self->chain->get_user_request_handler == nullptr) {
        self->chain->get_user_request_handler = [&self](const std::string &user_request_raw) {
            // we get consensus user_request_raw here
            self->consensus_request_queue.template push(std::string(user_request_raw));
        };
    }
    // obtain leader for the first time
    braft::PeerId leader = get_leader_by_rpc();
    ThreadPool clientsPool(5, "raft_cli_worker");  // the pending txn size is 5, same as fabric
    std::atomic<int> cnt = 0;
    while (!brpc::IsAskedToQuit()) {
        // we get a request first
        std::vector<std::string> requestRawList;
        // first reserve max size
        requestRawList.resize(self->batch_size);
        // we then get the actual message count
        size_t message_count;
        while (message_count = self->request_queue.wait_dequeue_bulk_timed(requestRawList.begin(), self->batch_size, 500), message_count == 0);
        requestRawList.resize(message_count);
        // then get content
        auto* requestPtr = new raft::PushRequest;
        for (const auto &requestRaw: requestRawList) {
            requestPtr->add_user_request(requestRaw);
        }
        requestPtr->set_id(self->leader_group_id);
        clientsPool.execute([self, requestPtr, leaderAddr=leader, &get_leader_by_rpc, &cnt]() {
            DLOG(INFO) << "send a proposal to raft, flying proposal  count: " << ++cnt;
            raft::ChainResponse response;
            braft::PeerId leader = leaderAddr;
            std::unique_ptr<raft::PushRequest> request(requestPtr);
            do {
                while (!self->client->push_user_request(leader, request.get(), &response)) {
                    DLOG(WARNING) << "request send failed, refresh leader and wait 0.1 sec.";
                    leader = get_leader_by_rpc();
                    BlockBench::Timer::sleep(0.1);
                }
            } while (!self->client->process_response(&response));    // resend request
            DLOG(INFO) << "finished sent a proposal to raft, flying proposal count: " << --cnt;
        });
    }
    return nullptr;
}

template<class T>
void *ConsensusManager<T>::receiver_from_user(void *ptr) {
    T *derived = static_cast<T *>(ptr);
    derived->receiver_from_user();
    return nullptr;
}

template<class T>
void *ConsensusManager<T>::receiver_from_raft(void *ptr) {
    T *derived = static_cast<T *>(ptr);
    derived->receiver_from_raft();
    return nullptr;
}

#endif //NEUBLOCKCHAIN_CONSENSUS_MANAGER_H
