//
// Created by peng on 2021/9/9.
//

#ifndef NEUBLOCKCHAIN_CONSENSUS_MANAGER_H
#define NEUBLOCKCHAIN_CONSENSUS_MANAGER_H

#include <brpc/server.h>
#include <bthread/bthread.h>
#include "common/concurrent_queue/blocking_concurrent_queue.hpp"
#include "common/timer.h"
#include "common/thread_pool.h"
#include "chain.h"
#include "chain_client.h"

using QueueType = moodycamel::BlockingConcurrentQueue<std::string>;

template<class T>
class ConsensusManager {
public:
    ~ConsensusManager();

    void print_status() { client->print_status(); }

protected:
    template<typename U>
    inline void get_message_from_raft(U &item) { consensus_request_queue.wait_dequeue(item); }

    template<typename U>
    inline void push_message_to_raft(U &&item) { request_queue.enqueue(std::forward<U>(item)); }

private:
    friend T;

    ConsensusManager(const std::string &initial_conf, const std::string &ip, int raft_port, int batch_size = 1);

    static void *receiver_from_raft(void *ptr);

    static void *receiver_from_user(void *ptr);

    static void *send_to_raft(void *ptr);

private:
    int batch_size;
    std::unique_ptr<brpc::Server> server;
    std::unique_ptr<raft::Chain> chain;
    std::unique_ptr<raft::ChainClient> client;
    std::unique_ptr<raft::ChainServiceImpl> service;
    bthread_t user_receiver_tid{};
    bthread_t raft_receiver_tid{};
    bthread_t raft_sender_tid{};
    QueueType consensus_request_queue;
    QueueType request_queue;
};

template<class T>
ConsensusManager<T>::ConsensusManager(const std::string &initial_conf, const std::string &ip, int raft_port, int batch_size)
        :batch_size(batch_size) {
    // Generally you only need one Server.
    server = std::make_unique<brpc::Server>();
    chain = std::make_unique<raft::Chain>();
    service = std::make_unique<raft::ChainServiceImpl>(chain.get());
    // Add your service into RPC server
    if (server->AddService(service.get(), brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add service";
        CHECK(false);
    }
    // raft can share the same RPC server. Notice the second parameter, because
    // adding services into a running server is not allowed and the listen
    // address of this server is impossible to get before the server starts. You
    // have to specify the address of the server.
    if (braft::add_service(server.get(), raft_port) != 0) {
        LOG(ERROR) << "Fail to add raft service";
        CHECK(false);
    }
    // It's recommended to start the server before Chain is started to avoid
    // the case that it becomes the leader while the service is unreachable by clients.
    // Notice the default options of server is used here. Check out details from
    // the doc of brpc if you would like change some options;
    if (server->Start(raft_port, nullptr) != 0) {
        LOG(ERROR) << "Fail to start Server";
        CHECK(false);
    }
    // It's ok to start Chain;
    if (!chain->start_service(initial_conf, ip, raft_port)) {
        LOG(ERROR) << "Fail to start Chain";
        CHECK(false);
    }
    LOG(INFO) << "Chain service is running on " << server->listen_address();
    // start client
    client = std::make_unique<raft::ChainClient>();
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

template<class T>
ConsensusManager<T>::~ConsensusManager() {
    LOG(INFO) << "Chain service is going to quit";
    bthread_join(user_receiver_tid, nullptr);
    bthread_join(raft_receiver_tid, nullptr);
    bthread_join(raft_sender_tid, nullptr);
    // Stop counter before server
    chain->shutdown();
    server->Stop(0);
    // Wait until all the processing tasks are over.
    chain->join();
    server->Join();
}

template<class T>
void *ConsensusManager<T>::send_to_raft(void *ptr) {
    auto *self = static_cast<ConsensusManager<T> *>(ptr);
    // get leader func for client to send request
    const auto get_leader_by_rpc = [self]() -> braft::PeerId {
        braft::PeerId leader = self->chain->get_leader_from_node();
        // obtain leader
        while (butil::ip2str(leader.addr.ip).c_str() == std::string("0.0.0.0")) {
            LOG(INFO) << "Electing leader, waiting...";
            BlockBench::Timer::sleep(0.1);
            leader = self->chain->get_leader_from_node();
        }
        raft::ChainClient::update_leader_in_route_table(leader);
        return leader;
    };
    self->chain->get_user_request_handler = [&self](const std::string &user_request_raw) {
        // we get consensus user_request_raw here
        self->consensus_request_queue.enqueue(user_request_raw);
    };
    // obtain leader for the first time
    braft::PeerId leader = get_leader_by_rpc();
    ThreadPool clientsPool(5);
    std::atomic<int> cnt = 0;
    while (!brpc::IsAskedToQuit()) {
        // we get a request first
        std::vector<std::string> requestRawList;
        // first reserve max size
        requestRawList.resize(self->batch_size);
        // we then get the actual message count
        int message_count = 0;
        while (message_count += self->request_queue.template wait_dequeue_bulk(requestRawList.begin() + message_count, self->batch_size - message_count), message_count < self->batch_size);
        CHECK(message_count == self->batch_size);
        requestRawList.resize(message_count);
        DLOG(INFO) << "batch size: " << self->batch_size << ", dequeue: " << message_count << ", queue size: " << self->request_queue.size_approx();
        // then get content
        auto* requestPtr = new raft::PushRequest;
        for (const auto &requestRaw: requestRawList) {
            requestPtr->add_user_request(requestRaw);
        }
        DLOG(INFO) << "send a proposal to raft, flying proposal  count: " << ++cnt;
        clientsPool.execute([self, requestPtr, leaderAddr=leader, &get_leader_by_rpc, &cnt]() {
            raft::ChainResponse response;
            braft::PeerId leader = leaderAddr;
            std::unique_ptr<raft::PushRequest> request(requestPtr);
            do {
                while (!self->client->push_user_request(leader, request.get(), &response)) {
                    LOG(WARNING) << "request send failed, refresh leader and wait 0.1 sec.";
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
