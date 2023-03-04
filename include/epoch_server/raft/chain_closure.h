//
// Created by peng on 2021/9/8.
//

#ifndef NEUBLOCKCHAIN_CHAIN_CLOSURE_H
#define NEUBLOCKCHAIN_CHAIN_CLOSURE_H

#include <braft/raft.h>
#include "chain.pb.h"


namespace raft {

    class Chain;

    // Implements Closure which encloses RPC stuff
    class ChainClosure : public braft::Closure {
    public:
        // a decorator for rpc closure
        ChainClosure(Chain *chain, const google::protobuf::Message *request, ChainResponse *response,
                     google::protobuf::Closure *done)
                : _chain(chain), _request(request), _response(response), _done(done) {}

        void Run() override;

        [[nodiscard]] const google::protobuf::Message *request() const { return _request; }

        [[nodiscard]] ChainResponse *response() const { return _response; }

    private:
        Chain *_chain;
        const google::protobuf::Message *_request;
        ChainResponse *_response;
        google::protobuf::Closure *_done;
    };

}

#endif //NEUBLOCKCHAIN_CHAIN_CLOSURE_H
