//
// Created by peng on 3/2/21.
//

#ifndef NEUBLOCKCHAIN_ORM_FIELD_H
#define NEUBLOCKCHAIN_ORM_FIELD_H

#include <string>

namespace AriaORM {

    // https://blog.csdn.net/sunt2018/article/details/88534529
    enum class ORMFieldType {
        AutoField = 0,
        BinaryField = 1,
        BooleanField = 2,
        IntegerFiled = 3,
        PositiveIntegerFiled = 4,
        CharFiled = 5,
        TextFiled = 6,
        DateTimeField = 7,
        FloatFiled = 8,
        DecimalFiled = 9,
        ForeignKey = 10,
        OneToOneField = 11, // ForeignKey == true, on_delete method
        ManyToManyField = 12 // ForeignKey == true
    };

    class ORMFieldBase {
    public:
        virtual ~ORMFieldBase() = default;

        [[nodiscard]] ORMFieldType getFieldType() const { return fieldType; }

        [[nodiscard]] std::string getColumnName() const { return columnName; }

        void setPrimary(bool _primary) { this->primary = _primary; }

        [[nodiscard]] bool isPrimary() const { return primary; }

        void setUnique(bool _unique) { this->unique = _unique; }

        [[nodiscard]] bool isUnique() const { return unique; }

        void setNull(bool _nil) { this->nil = _nil; }

        [[nodiscard]] bool isNull() const { return nil; }

        void setIndex(bool _index) { this->index = _index; }

        [[nodiscard]] bool isIndex() const { return index; }

    protected:
        ORMFieldBase(ORMFieldType _fieldType, std::string _columnName)
                : fieldType(_fieldType), columnName(std::move(_columnName)) {}

    protected:
        const ORMFieldType fieldType;
        const std::string columnName;
        bool primary = false;
        bool unique = false;
        bool nil = false;
        bool index = false;
    };
}
#endif //NEUBLOCKCHAIN_ORM_FIELD_H
