//
// Created by peng on 3/2/21.
//

#ifndef NEUBLOCKCHAIN_TABLE_IMPL_HPP
#define NEUBLOCKCHAIN_TABLE_IMPL_HPP

#include "block_server/database/orm/table_definition.h"
#include "block_server/database/orm/fields/orm_field.h"
#include <vector>

namespace AriaORM {

    class ORMTableImpl: virtual public ORMTableDBDefinition{
    public:
        explicit ORMTableImpl(std::string _tableName): tableName(std::move(_tableName)) {}

        bool addField(const std::shared_ptr<ORMFieldBase>& fieldBase) override {
            fieldList.push_back(fieldBase);
            return true;
        }

        size_t fieldSize() override { return fieldList.size(); }

        ORMFieldBase* getField(size_t id) override {
            if(id >= fieldSize()) {
                return nullptr;
            } else {
                return fieldList[id].get();
            }
        }

        const std::string& getTableName() override { return tableName; }

        TableSetting* setTableSetting() override { return tableSetting; }

        const TableSetting* getSetting() override { return tableSetting; }

    private:
        std::vector<std::shared_ptr<ORMFieldBase>> fieldList;
        TableSetting* tableSetting{};
        const std::string tableName;
    };

}
#endif //NEUBLOCKCHAIN_TABLE_IMPL_HPP
