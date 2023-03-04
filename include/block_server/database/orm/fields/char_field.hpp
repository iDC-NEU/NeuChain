//
// Created by peng on 3/2/21.
//

#ifndef NEUBLOCKCHAIN_CHAR_FIELD_HPP
#define NEUBLOCKCHAIN_CHAR_FIELD_HPP

#include "orm_field.h"

namespace AriaORM {
    class CharField : public ORMFieldBase {
    public:
        CharField(std::string column_name, size_t max_length, bool nil = false)
            :ORMFieldBase(ORMFieldType::CharFiled, std::move(column_name)), maxLength(max_length) {
            this->setNull(nil);
        }

        [[nodiscard]] size_t getMaxLength() const { return maxLength; }

    private:
        size_t maxLength;
    };
}
#endif //NEUBLOCKCHAIN_CHAR_FIELD_HPP
