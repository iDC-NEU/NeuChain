//
// Created by peng on 3/4/21.
//

#ifndef NEUBLOCKCHAIN_QUERY_H
#define NEUBLOCKCHAIN_QUERY_H

#include <string>
#include <vector>
#include <memory>

class KVRWSet;

namespace AriaORM {
    class ResultsIterator;

    enum class NumFilter {
        ALL = 0x0000,
        EQU = 0x0001,
        GT = 0x0010,
        LT = 0x0020,
    };

    enum class StrFilter {
        EQU = 0x0001,
        CONTAINS = 0x0002,
        START_WITH = 0x0100,
        END_WITH = 0x0200,
    };

    class ORMQuery {    //init using cc helper
    public:
        virtual ~ORMQuery() = default;
        // int filter
        virtual void filter(const std::string &attr, NumFilter range, int arg1, int arg2) = 0;

        // float filter
        virtual void filter(const std::string &attr, NumFilter range, double arg1, double arg2) = 0;

        // str filter
        virtual void filter(const std::string &attr, StrFilter range, const std::string &arg) = 0;

        // order by
        virtual void orderBy(std::vector<std::string> orderAttr) = 0;

        // get raw query string
        [[nodiscard]] virtual std::string raw() const = 0;

        // ExecuteQuery executes the given query and returns an iterator that contains results of type *VersionedKV.
        virtual ResultsIterator* executeQuery(const std::string &query) = 0;    //ResultsIterator = pqcc::result + cc helper + result iterator

        virtual ResultsIterator* executeQuery() = 0;
    };

    class ResultsIterator {
    public:
        virtual void init(KVRWSet* _kvRWSet, const std::string& _table) = 0;
        virtual ~ResultsIterator() = default;

        virtual bool next() = 0;
        virtual bool hasNext() = 0;

        virtual int getInt(const std::string& attr) = 0;
        virtual std::string getString(const std::string& attr) = 0;
    };
}

#endif //NEUBLOCKCHAIN_QUERY_H
