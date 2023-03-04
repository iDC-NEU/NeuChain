//
// Created by peng on 2021/9/8.
//

#ifndef NEUBLOCKCHAIN_CHAIN_H
#define NEUBLOCKCHAIN_CHAIN_H


#include <braft/raft.h>
#include "chain.pb.h"

namespace raft {
    // Implementation of raft::Chain as a braft::StateMachine.
    class Chain : public braft::StateMachine {
    public:
        Chain();

        ~Chain() override;

        bool start_service(const std::string &initial_conf, const std::string &ip, int port);

        // Shut this node down.
        void shutdown() { _node->shutdown(nullptr); }

        // Blocking this thread until the node is eventually down.
        void join() { _node->join(); }

        [[nodiscard]] braft::PeerId get_leader_from_node() const { return _node->leader_id(); }

        std::function<void(const std::string &)> get_user_request_handler;

        // client call push method filed for two reasons:
        // 1. current node is not a leader
        // 2. deserialize message failed
        // default called by proto service
        void push(const PushRequest *request, ChainResponse *response, google::protobuf::Closure *done);

    private:
        // @braft::StateMachine
        void on_apply(braft::Iterator &iter) override;

        void on_snapshot_save(braft::SnapshotWriter *writer, braft::Closure *done) override;

        int on_snapshot_load(braft::SnapshotReader *reader) override;

        void on_leader_start(int64_t term) override {
            _leader_term.store(term, butil::memory_order_release);
            LOG(INFO) << "Node becomes leader";
        }

        void on_leader_stop(const butil::Status &status) override {
            _leader_term.store(-1, butil::memory_order_release);
            LOG(INFO) << "Node stepped down : " << status;
        }

        void on_shutdown() override {
            LOG(INFO) << "This node is down";
        }

        void on_error(const ::braft::Error &e) override {
            LOG(ERROR) << "Met raft error " << e;
        }

        void on_configuration_committed(const ::braft::Configuration &conf) override {
            LOG(INFO) << "Configuration of this group is " << conf;
        }

        void on_stop_following(const ::braft::LeaderChangeContext &ctx) override {
            LOG(INFO) << "Node stops following " << ctx;
        }

        void on_start_following(const ::braft::LeaderChangeContext &ctx) override {
            LOG(INFO) << "Node start following " << ctx;
        }
        // end of @braft::StateMachine

        static void *save_snapshot(void *arg);

        struct SnapshotClosure {
            // put all your data here
            // std::vector<std::string> dataList;
            braft::SnapshotWriter *writer{};
            braft::Closure *done{};
        };

        braft::Node *volatile _node;
        butil::atomic<int64_t> _leader_term;

    private:
        static const int election_timeout_ms = 5000;    // Start election in such milliseconds if disconnect with the leader
        static const int snapshot_interval = 30;    // Interval between each snapshot
        static const char *group_name;

    };

    // Implements raft::AtomicService if you are using brpc.
    class ChainServiceImpl : public ChainService {
    public:
        explicit ChainServiceImpl(Chain *chain) : _chain(chain) {}

        ~ChainServiceImpl() override = default;

        void push(::google::protobuf::RpcController *controller,
                  const ::raft::PushRequest *request,
                  ::raft::ChainResponse *response,
                  ::google::protobuf::Closure *done) override {
            return _chain->push(request, response, done);
        }

    private:
        Chain *_chain;
    };

}  // namespace raft



#endif //NEUBLOCKCHAIN_CHAIN_H
