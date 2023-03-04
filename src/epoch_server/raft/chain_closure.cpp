//
// Created by peng on 2021/9/8.
//

#include "epoch_server/raft/chain_closure.h"
#include "epoch_server/raft/chain.h"
#include <brpc/closure_guard.h>

void raft::ChainClosure::Run() {
    std::unique_ptr<ChainClosure> self_guard(this);
    brpc::ClosureGuard done_guard(_done);
    if (status().ok()) {
        return;
    }
}
