//
// Created by peng on 2021/5/6.
//

#include "user/block_bench/ycsb_workload.h"
#include "common/yaml_config.h"
#include "glog/logging.h"

BlockBench::YCSBWorkload::YCSBWorkload()
    :YCSBWorkload((int)random()){}

BlockBench::YCSBWorkload::YCSBWorkload(int seed) {
    auto ycsbConfigPtr = YAMLConfig::getInstance()->getCCConfig();
    auto ycsbConfig = *ycsbConfigPtr;
    tableName = "ycsb";
    fieldCount = ycsbConfig["field_count"].as<int>();

    auto fieldLenDist = ycsbConfig["field_length_distribution"].as<std::string>();
    auto fieldLen = ycsbConfig["field_length"].as<int>();

    if(fieldLenDist == "constant") {
        fieldLenGenerator = std::make_unique<ConstGenerator>(fieldLen);
    } else if(fieldLenDist == "uniform") {
        fieldLenGenerator = std::make_unique<UniformGenerator>(1, fieldLen, seed);
    } else if(fieldLenDist == "zipfian") {
        fieldLenGenerator = std::make_unique<ZipfianGenerator>(1, fieldLen, seed);
    } else {
        LOG(ERROR) << "no matching generator provided.";
        CHECK(false);
    }

    auto read_proportion = ycsbConfig["read_proportion"].as<double>();
    auto update_proportion = ycsbConfig["update_proportion"].as<double>();
    auto insert_proportion = ycsbConfig["insert_proportion"].as<double>();
    auto scan_proportion = ycsbConfig["scan_proportion"].as<double>();
    auto read_modify_write_proportion = ycsbConfig["read_modify_write_proportion"].as<double>();

    recordCount = ycsbConfig["record_count"].as<size_t>();

    auto request_dist = ycsbConfig["request_distribution"].as<std::string>();
    auto max_scan_len = ycsbConfig["max_scan_len"].as<size_t>();
    auto scan_len_dist = ycsbConfig["scan_len_distribution"].as<std::string>();
    auto insert_start = ycsbConfig["insert_start"].as<size_t>();
    auto zipfian_const = ycsbConfig["zipfian_const"].as<double>();  // default: 0.99

    readAllFields = ycsbConfig["read_all_fields"].as<bool>();
    writeAllFields = ycsbConfig["write_all_fields"].as<bool>();
    orderedInserts = ycsbConfig["ordered_inserts"].as<bool>();
    keyGenerator = std::make_unique<CounterGenerator>(insert_start);
    opChooser.addValue(Operation::READ, read_proportion);
    opChooser.addValue(Operation::UPDATE, update_proportion);
    opChooser.addValue(Operation::INSERT, insert_proportion);
    opChooser.addValue(Operation::SCAN, scan_proportion);
    opChooser.addValue(Operation::READ_MODIFY_WRITE, read_modify_write_proportion);
    insertKeySequence.reset(recordCount);

    if (request_dist == "uniform") {
        keyChooser = std::make_unique<UniformGenerator>(0, recordCount - 1, seed);
    } else if (request_dist == "zipfian") {
        // If the number of keys changes, we don't want to change popular keys.
        // So we construct the scrambled zipfian generator with a keyspace
        // that is larger than what exists at the beginning of the test.
        // If the generator picks a key that is not inserted yet, we just ignore it
        // and pick another key.
        int op_count = ycsbConfig["operation_count"].as<int>();
        int new_keys = static_cast<int>(op_count * insert_proportion * 2); // a fudge factor
        keyChooser = std::make_unique<ScrambledZipfianGenerator>(0, recordCount + new_keys - 1, seed, zipfian_const);
    } else if (request_dist == "latest") {
        keyChooser = std::make_unique<SkewedLatestGenerator>(&insertKeySequence);
    } else {
        CHECK(false);
    }
    fieldChooser = std::make_unique<UniformGenerator>(0, fieldCount - 1, seed);

    if (scan_len_dist == "uniform") {
        scanLenChooser = std::make_unique<UniformGenerator>(1, max_scan_len, seed);
    } else if (scan_len_dist == "zipfian") {
        scanLenChooser = std::make_unique<ZipfianGenerator>(1, max_scan_len, seed);
    } else {
        CHECK(false);
    }
}
