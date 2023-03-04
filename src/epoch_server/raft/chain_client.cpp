//
// Created by peng on 2021/9/9.
//

#include <brpc/channel.h>
#include <bthread/bthread.h>
#include <braft/route_table.h>
#include "epoch_server/raft/chain_client.h"
#include "chain.pb.h"

raft::ChainClient::ChainClient(const char* group_name)
        : latency_recorder(group_name), group_name(group_name) {}

bool
raft::ChainClient::push_user_request(const braft::PeerId &leader, const PushRequest *request, ChainResponse *response) {
    // Now we known who is the leader, construct Stub and then sending rpc
    brpc::Channel channel;
    if (channel.Init(leader.addr, nullptr) != 0) {
        LOG(ERROR) << "Fail to init channel to " << leader;
        return false;
    }
    ChainService::Stub stub(&channel);

    brpc::Controller controller;
    controller.set_timeout_ms(timeout_ms);
    // we have set the request and response
    DLOG(INFO) << "Sending request to " << leader;
    stub.push(&controller, request, response, nullptr);

    if (controller.Failed()) {
        LOG(WARNING) << "Fail to send request to " << leader << " : " << controller.ErrorText();
        // Clear leadership since this RPC failed.
        // braft::rtb::update_leader(group_name, braft::PeerId());
        return true;
    }
    latency_recorder << controller.latency_us();
    DLOG(INFO) << "Received response from " << leader << " latency=" << controller.latency_us();
    return response->success();
}

bool raft::ChainClient::get_leader_from_route_table(const char* group, braft::PeerId &leader) {
    // Select leader of the target group from RouteTable
    if (braft::rtb::select_leader(group, &leader) != 0) {
        // Leader is unknown in RouteTable. Ask RouteTable to refresh leader
        // by sending RPCs.
        butil::Status st = braft::rtb::refresh_leader(group, 1000);
        if (!st.ok()) {
            // Not sure about the leader.
            LOG(WARNING) << "Fail to refresh_leader : " << st;
            return false;
        }
    }
    return true;
}

bool raft::ChainClient::process_response(const ChainResponse *response) {
    // TODO: process response
    if (!response->success()) {
        return false;
    }
    return true;
}

bool raft::ChainClient::update_leader_in_route_table(const char* group, const braft::PeerId &leader) {
    braft::rtb::update_leader(group, leader);
    return true;
}

bool raft::ChainClient::init_config(const std::string &initial_conf) {
    // Register configuration of target group to RouteTable
    if (braft::rtb::update_configuration(group_name, initial_conf) != 0) {
        LOG(ERROR) << "Fail to register configuration " << initial_conf << " of group " << group_name;
        return false;
    }
    return true;
}

raft::ChainClient::~ChainClient() {
    LOG(INFO) << "Counter client is going to quit";
}

void raft::ChainClient::print_status() {
    LOG(INFO) << "Sending Request to " << group_name
              << " at qps=" << latency_recorder.qps(100)
              << " latency=" << latency_recorder.latency(100);
}
