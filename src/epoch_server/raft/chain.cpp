//
// Created by peng on 2021/9/8.
//

#include "epoch_server/raft/chain.h"
#include "epoch_server/raft/chain_closure.h"
#include <braft/storage.h>
#include <braft/route_table.h>
#include <brpc/closure_guard.h>
#include <fstream>

raft::Chain::Chain() : _node(nullptr), _leader_term(-1) {

}

raft::Chain::~Chain() {
    delete _node;
}

void raft::Chain::push(const raft::PushRequest *request, raft::ChainResponse *response, google::protobuf::Closure *done) {
    brpc::ClosureGuard done_guard(done);
    // Serialize request to the replicated write-ahead-log so that all the
    // peers in the group receive this request as well.
    // Notice that _value can't be modified in this routine otherwise it
    // will be inconsistent with others in this group.

    // Serialize request to IOBuf
    const int64_t term = _leader_term.load(butil::memory_order_relaxed);
    if (term < 0) {
        LOG(ERROR) << "i'm not a leader.";
        response->set_success(false);
        return;
    }
    // the data in log will be swapped, dus no mem problem
    butil::IOBuf log;
    butil::IOBufAsZeroCopyOutputStream wrapper(&log);
    if (!request->SerializeToZeroCopyStream(&wrapper)) {
        LOG(ERROR) << "Fail to serialize request";
        response->set_success(false);
        return;
    }
    // Apply this log as a braft::Task
    braft::Task task;
    task.data = &log;
    // This callback would be invoked when the task actually executed or fail
    task.done = new ChainClosure(this, request, response, done_guard.release());
    // ABA problem can be avoid if expected_term is set
    task.expected_term = term;
    // Now the task is applied to the group, waiting for the result.
    _node->apply(task);
}

void raft::Chain::on_apply(braft::Iterator &iter) {
    const auto requestHandler = [this](const PushRequest *request) {
        DLOG(INFO) << "received a msg from raft.";
        for (const auto &userRequest :request->user_request()) {
            get_user_request_handler(userRequest);
        }
        // Remove these logs in performance-sensitive servers.
        DLOG(INFO) << "Handled operation " << request->id();
    };
    // A batch of tasks are committed, which must be processed through |iter|
    for (; iter.valid(); iter.next()) {
        // This guard helps invoke iter.done()->Run() asynchronously to avoid that callback blocks the StateMachine.
        braft::AsyncClosureGuard done_guard(iter.done());

        if (ChainClosure *closure; iter.done() && (closure = dynamic_cast<ChainClosure *>(iter.done()), closure)) {
            // we have closure already
            // This task is applied by this node, get value from this closure to avoid additional parsing.
            const auto *request = dynamic_cast<const PushRequest *>(closure->request());
            // push it to local log
            requestHandler(request);
            // prepare response
            auto *response = closure->response();
            response->set_success(true);
            response->set_id(request->id());
        } else {
            // as a follower
            butil::IOBufAsZeroCopyInputStream wrapper(iter.data());
            PushRequest request;
            CHECK(request.ParseFromZeroCopyStream(&wrapper));
            // we have request now, push it to local log
            requestHandler(&request);
            // no response
        }
    }
}

void raft::Chain::on_snapshot_save(braft::SnapshotWriter *writer, braft::Closure *done) {
    // Save current StateMachine in memory and starts a new bthread to avoid
    // blocking StateMachine since it's a bit slow to write data to disk file.
    auto *sc = new SnapshotClosure;
    sc->writer = writer;
    sc->done = done;
    // TODO: copy your data to sc
//    for (auto it = _value_map.begin(); it != _value_map.end(); ++it) {
//        sc->values.emplace_back(it->first, it->second);
//    }
    bthread_t tid;
    bthread_start_urgent(&tid, nullptr, save_snapshot, sc);
}

int raft::Chain::on_snapshot_load(braft::SnapshotReader *reader) {
    CHECK_EQ(-1, _leader_term) << "Leader is not supposed to load snapshot";
    // TODO: 1. clear your data in memory
    // 2. load data
    std::string snapshot_path = reader->get_path();
    snapshot_path.append("/data");
    std::ifstream is(snapshot_path.c_str());
//    int64_t id = 0;
//    int64_t value = 0;
//    while (is >> id >> value) {
//        _value_map[id] = std::to_string(value);
//    }
    return 0;
}

void *raft::Chain::save_snapshot(void *arg) {
    auto *sc = (SnapshotClosure *) arg;
    std::unique_ptr<SnapshotClosure> sc_guard(sc);
    brpc::ClosureGuard done_guard(sc->done);
    std::string snapshot_path = sc->writer->get_path();
    snapshot_path.append("/data");
    std::ofstream os(snapshot_path.c_str());
//    for (auto &value : sc->values) {
//        os << value.first << ' ' << value.second << '\n';
//    }
    CHECK_EQ(0, sc->writer->add_file("data"));
    return nullptr;
}

// initial_conf: Initial configuration of the replication group
bool raft::Chain::start_service(const std::string &initial_conf, const std::string &ip, int port) {
    butil::EndPoint addr(butil::my_ip(), port);
    braft::NodeOptions node_options;
    if (node_options.initial_conf.parse_from(initial_conf) != 0) {
        LOG(ERROR) << "Fail to parse configuration `" << initial_conf << '\'';
        return false;
    }
    node_options.election_timeout_ms = election_timeout_ms;
    node_options.fsm = this;
    node_options.node_owns_fsm = false;
    node_options.snapshot_interval_s = snapshot_interval;
    std::string prefix = "local://./raft_data";    //Path of data stored on
    node_options.log_uri = prefix + "/log";
    node_options.raft_meta_uri = prefix + "/raft_meta";
    node_options.snapshot_uri = prefix + "/snapshot";
    node_options.disable_cli = false;   // Don't allow raft_cli access this node
    // Id of the replication group
    butil::EndPoint point;
    str2endpoint(ip.data(), port, &point);
    auto *node = new braft::Node(group_name, braft::PeerId(point));
    if (node->init(node_options) != 0) {
        LOG(ERROR) << "Fail to init raft node";
        delete node;
        return false;
    }
    _node = node;
    return true;
}

const char *raft::Chain::group_name = "chain_group";