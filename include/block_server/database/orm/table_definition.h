//
// Created by peng on 3/2/21.
//

#ifndef NEUBLOCKCHAIN_TABLE_DEFINITION_H
#define NEUBLOCKCHAIN_TABLE_DEFINITION_H

#include <string>
#include <memory>

namespace AriaORM {
    class ORMFieldBase;
    class TableSetting;

    class ORMTableDefinition {
    public:
        virtual ~ORMTableDefinition() = default;
        virtual bool addField(const std::shared_ptr<ORMFieldBase>& fieldBase) = 0;
        virtual TableSetting* setTableSetting() = 0;
    };

    // this interface is use for database_impl. commit update.
    class ORMTableDBDefinition: virtual public ORMTableDefinition {
    public:
        virtual size_t fieldSize() = 0;
        virtual ORMFieldBase* getField(size_t id) = 0;
        virtual const std::string& getTableName() = 0;
        virtual const TableSetting* getSetting() = 0;
    };

}

#endif //NEUBLOCKCHAIN_TABLE_DEFINITION_H
