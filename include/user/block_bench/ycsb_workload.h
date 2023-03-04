#ifndef YCSB_C_CORE_WORKLOAD_H_
#define YCSB_C_CORE_WORKLOAD_H_

#include <memory>
#include <string>
#include "common/random_generator.h"
#include "common/fnv_hash.h"
#include "common/random.h"

namespace BlockBench {

    enum class Operation {
        INSERT,
        READ,
        UPDATE,
        SCAN,
        READ_MODIFY_WRITE
    };

    class YCSBWorkload {
    public:
        YCSBWorkload();
        explicit YCSBWorkload(int seed);
        virtual ~YCSBWorkload() = default;

    public:
        inline Operation nextOperation() { return opChooser.next(); }

        inline std::string nextSequenceKey() {
            uint64_t keyNum = keyGenerator->next();
            return buildKeyName(keyNum);
        }

        inline std::string nextTransactionKey() {
            uint64_t key_num;
            do {
                key_num = keyChooser->next();
            } while (key_num > insertKeySequence.last());
            return buildKeyName(key_num);
        }

        inline std::string nextFieldName() {
            return std::string("field").append(std::to_string(fieldChooser->next()));
        }

        using KVPair = std::pair<std::string, std::string>;
        inline void generateRandomValues(std::vector<KVPair> &values) {
            for (int i = 0; i < fieldCount; ++i) {
                auto length = fieldLenGenerator->next();
                values.emplace_back("field" + std::to_string(i), randomHelper.a_string(length, length));
            }
        }

        inline void generateRandomUpdate(std::vector<KVPair> &update) {
            auto length = fieldLenGenerator->next();
            update.emplace_back(nextFieldName(), randomHelper.a_string(length, length));
        }

        inline std::string getNextTableName() { return tableName; }
        [[nodiscard]] inline bool getReadAllFields() const { return readAllFields; }
        [[nodiscard]] inline bool getWriteAllFields() const { return writeAllFields; }
        [[nodiscard]] inline size_t getNextScanLength() { return scanLenChooser->next(); }

    protected:
        [[nodiscard]] inline std::string buildKeyName(uint64_t keyNum) const {
            if (!orderedInserts) {
                // hash it!
                return std::string("user").append(std::to_string(FNVHash64(keyNum)));
            } else {
                return std::string("user").append(std::to_string(keyNum));
            }
        }

    protected:
        Random randomHelper;
        std::string tableName;
        int fieldCount;
        bool readAllFields;
        bool writeAllFields;
        std::unique_ptr<Generator<uint64_t>> keyGenerator;
        std::unique_ptr<Generator<uint64_t>> fieldLenGenerator;
        std::unique_ptr<Generator<uint64_t>> keyChooser;
        std::unique_ptr<Generator<uint64_t>> fieldChooser;
        std::unique_ptr<Generator<uint64_t>> scanLenChooser;
        DiscreteGenerator<Operation> opChooser;
        CounterGenerator insertKeySequence;
        bool orderedInserts;
        size_t recordCount;
    };

}

#endif // YCSB_C_CORE_WORKLOAD_H_
