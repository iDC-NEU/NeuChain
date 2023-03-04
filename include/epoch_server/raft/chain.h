//
// Created by peng on 2021/9/8.
//

#ifndef NEUBLOCKCHAIN_CHAIN_H
#define NEUBLOCKCHAIN_CHAIN_H


#include <braft/raft.h>
#include "common/compile_config.h"
#include "chain.pb.h"
#include "common/timer.h"
#include "common/yaml_config.h"

namespace raft {
    // Implementation of raft::Chain as a braft::StateMachine.
    class Chain : public braft::StateMachine {
    public:
        Chain();

        ~Chain() override;

        bool add_chain_instance(const std::string &initial_conf, const std::string &ip, int port, const char *group_name, int group_id);

        void setLeaderGroupId(int group_id) {
            leader_group_id=group_id;
        }

        // Shut this node down.
        void shutdown() {
            for (auto& node: node_list) {
                node->shutdown(nullptr);
            }
        }

        // Blocking this thread until the node is eventually down.
        void join() {
            for (auto& node: node_list) {
                node->shutdown(nullptr);
            }
        }

        [[nodiscard]] braft::PeerId get_leader_from_node(int group_id) const { return node_list[group_id]->leader_id(); }


        void refreshLeader() {
            auto* configPtr = YAMLConfig::getInstance();
            const int raft_port = 18100;
            for (int i=0; i<(int)node_list.size(); i++) {
                if (i != leader_group_id) {
                    transferLeader(configPtr->getBlockServerIPs()[i], raft_port, i);
                }
            }
        }


        // user must not call this function
        void transferLeader(const std::string& ip, int port, int group_id) {
            if (!node_list[group_id]->is_leader())
                return;
            LOG(INFO) << "transfer leader to " << ip << " port: " << port << " group:" << group_id;
            butil::EndPoint addr;
            std::string ipStr = ip + ":" + std::to_string(port);
            str2endpoint(ipStr.data(), &addr);
            while(node_list[group_id]->transfer_leadership_to(braft::PeerId(addr, group_id)) != 0) {
                BlockBench::Timer::sleep(0.1);
            }
        }

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
            if (!node_list[leader_group_id]->is_leader())
                return;
            _leader_term.store(term, butil::memory_order_release);
            LOG(INFO) << "Node becomes leader";
        }

        void on_leader_stop(const butil::Status &status) override {
            if (node_list[leader_group_id]->is_leader())
                return;
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

        std::vector<braft::Node*> node_list;
        butil::atomic<int64_t> _leader_term;
        int leader_group_id = -1;

    private:
        static const int election_timeout_ms = RAFT_TIMEOUT_MS;    // Start election in such milliseconds if disconnect with the leader
        static const int snapshot_interval = 1000;    // Interval between each snapshot, sec
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
