//
// Created by peng on 2021/9/10.
//

#include <thread>
#include "epoch_server/oe_consensus_manager.h"

#include "common/msp/crypto_helper.h"
#include "common/zmq/zmq_server.h"
#include "common/zmq/zmq_client.h"
#include "common/yaml_config.h"
#include "butil/endpoint.h"
#include "oe.pb.h"

/** oe step:
 * 1. leader order a set of transactions into a block
 * 2. leader execute and get the result
 * 3. leader uses pbft or raft protocol, and broadcasts the block to all peers
 * 4. peer receive the transactions (it have to ensure te data is valid as well as validated).
 * 5. peer re-execute it, if:
 * 5.1 if the result is the same as the leader, it vote true to pbft
 * 5.2 else, it vote false to the cluster
 * 6. after collect enough votes (f+1 or 2f+1), leader proceed to the next block
*/

OEConsensusManager::OEConsensusManager(const consensus_param &param)
        : ConsensusManager(param.initial_conf, param.raft_ip, param.raft_port, param.batch_size) {
    block_broadcaster_port = param.block_broadcaster_port;
    request_receiver_port = param.request_receiver_port;
    block_size = param.block_size;
    duration = param.duration;
    recvThread = std::make_unique<std::thread>(&OEConsensusManager::recv_barrier, this);
    // sync phrase block
    chain->get_user_request_handler = [this](const std::string &user_request_raw) {
        // leader just return
        // TODO: we use the back as leader for testing
        if (YAMLConfig::getInstance()->getBlockServerIPs().back() == YAMLConfig::getInstance()->getLocalBlockServerIP()) {
            return;
        }
        // for follower
        auto except = getLatestStoredEpoch() + 1;
        LOG(INFO) << "process epoch " << except;
        eov::EOVBlock block;
        block.ParseFromString(user_request_raw);
        for(const auto& data: block.user_request()) {
            pushTxHandle(data);
        }
        LOG(INFO) << "send epoch increase signal " << except;
        increaseEpochHandle();
        while (except != getLatestStoredEpoch()) {
            // 5.1 if the result is the same as the leader, it vote true to pbft
            // we just check if it re-executed the block
            BlockBench::Timer::sleep(0.001);    // sleep for 1ms
        }
    };
}

void OEConsensusManager::receiver_from_raft() {

}

void OEConsensusManager::receiver_from_user() {
    ZMQServer txReceiver("0.0.0.0", std::to_string(request_receiver_port), zmq::socket_type::sub);
    auto prevIP = butil::ip2str(chain->get_leader_from_node().addr.ip);
    // obtain leader
    while (prevIP.c_str() == std::string("0.0.0.0")) {
        prevIP = butil::ip2str(chain->get_leader_from_node().addr.ip);
    }
    ZMQClient zmqClient(prevIP.c_str(), std::to_string(request_receiver_port), zmq::socket_type::pub);
    BlockBench::Timer timer;
    epoch_size_t execEpoch = 1;
    std::map<epoch_size_t, eov::EOVBlock> blockMap;
    moodycamel::BlockingConcurrentQueue<std::string> remoteQueue;
    std::thread asyncGetFromRemoteThread([&txReceiver, &remoteQueue, this]{
        while (!brpc::IsAskedToQuit()) {
            std::string requestRaw;
            txReceiver.getRequest(requestRaw);
            remoteQueue.enqueue(std::move(requestRaw));
        }
    });
    bool isLocalServerNotLeader = YAMLConfig::getInstance()->getBlockServerIPs().back() != YAMLConfig::getInstance()->getLocalBlockServerIP();
    while (!brpc::IsAskedToQuit()) {
        // we get a request first
        std::string requestRaw;
        // 1 empty the queue first
        while (remoteQueue.try_dequeue(requestRaw)) {
            // make sure that this peer is leader
            // TODO: WE CURRENTLY USE the back AS leader
            if (isLocalServerNotLeader) {
                CHECK(false);
                // follower redirect request and return
                auto nowIP = butil::ip2str(chain->get_leader_from_node().addr.ip);
                if (strcmp(nowIP.c_str(), prevIP.c_str()) != 0) {
                    // TODO: change leader
                    prevIP = nowIP;
                    zmqClient = ZMQClient(nowIP.c_str(), std::to_string(request_receiver_port), zmq::socket_type::pub);
                }
                zmqClient.sendRequest(requestRaw);
                continue;
            }
            // for leader, pass to comm controller
            blockMap[execEpoch].add_user_request(requestRaw);
            pushTxHandle(requestRaw);
            // batch by time or size
            if (blockMap[execEpoch].user_request_size() > block_size) {
                break;
            }
        }
        // 2 if batch too small, wait
        if (blockMap[execEpoch].user_request_size() == 0) {
            continue;
        }
        // batch by time or size
        if (timer.end() < duration) {
            // if (blockMap[execEpoch].user_request_size() < block_size) {
            continue;
        }
        LOG(INFO) << "increase epoch, epoch= " << execEpoch << " txn size= " << blockMap[execEpoch].user_request_size();
        // 3 exec and increase epoch
        increaseEpochHandle();
        timer.start();
        // in another thread
        // when we generate a block, we broadcast it to all node
        while (execEpoch != getLatestStoredEpoch()) {
            std::this_thread::yield();
        }
        LOG(INFO) << "push_message_to_raft, epoch= " << execEpoch << " txn size= " << blockMap[execEpoch].user_request_size();
        push_message_to_raft(blockMap[execEpoch].SerializeAsString());
        // wait to all follower processed the block
        sema.wait();
        LOG(INFO) << "get enough vote for epoch= " << execEpoch;
        blockMap.erase(execEpoch);
        execEpoch++;
    }
}
