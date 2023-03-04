//
// Created by peng on 2021/5/6.
//

#ifndef NEUBLOCKCHAIN_ARIA_YCSB_DB_H
#define NEUBLOCKCHAIN_ARIA_YCSB_DB_H

#include "ycsb_db.h"
#include <memory>
#include "common/hash_map.h"
#include "common/zmq/zmq_client.h"

namespace Utils {
    struct Request;
}

namespace BlockBench {
    class AriaYCSB_DB: public YCSB_DB {

    public:
        AriaYCSB_DB();

        int Read(const std::string &table, const std::string &key,
                 const std::vector<std::string> *fields, std::vector<KVPair> &result) override;

        int Update(const std::string &table, const std::string &key,
                   std::vector<KVPair> &values) override;

        int Scan(const std::string &table, const std::string &key,
                 int record_count, const std::vector<std::string> *fields,
                 std::vector<std::vector<KVPair>> &result) override;

        int Insert(const std::string &table, const std::string &key,
                   std::vector<KVPair> &values) override;

        int Delete(const std::string &table, const std::string &key) override;
    };
}


#endif //NEUBLOCKCHAIN_ARIA_YCSB_DB_H
