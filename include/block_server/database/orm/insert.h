//
// Created by peng on 3/5/21.
//

#ifndef NEUBLOCKCHAIN_INSERT_H
#define NEUBLOCKCHAIN_INSERT_H

#include <string>

namespace AriaORM {
    class ORMInsert {
    public:
        virtual ~ORMInsert() = default;
        virtual bool set(const std::string& attr, const std::string& value) = 0;
        virtual bool set(const std::string& attr, int value) = 0;

    };
}
#endif //NEUBLOCKCHAIN_INSERT_H
