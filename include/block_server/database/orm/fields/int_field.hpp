//
// Created by peng on 3/5/21.
//

#ifndef NEUBLOCKCHAIN_INT_FIELD_H
#define NEUBLOCKCHAIN_INT_FIELD_H

#include "orm_field.h"

namespace AriaORM {
    class IntField : public ORMFieldBase {
    public:
        explicit IntField(std::string column_name, bool nil = false)
                :ORMFieldBase(ORMFieldType::IntegerFiled, std::move(column_name)){
            this->setNull(nil);
        }
    };
}
#endif //NEUBLOCKCHAIN_INT_FIELD_H
