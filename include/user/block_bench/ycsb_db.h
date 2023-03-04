//
//  db.h
//  YCSB-C
//
//  Created by Jinglei Ren on 12/10/14.
//  Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>.
//

#ifndef YCSB_C_DB_H_
#define YCSB_C_DB_H_

#include <vector>
#include "db_user_base.h"

namespace BlockBench {
    class YCSB_DB: public DBUserBase {
    public:
        static std::unique_ptr<BlockBench::YCSB_DB> CreateDB(const std::string& type, const std::vector<std::string>& args);
        static void ClientThread(std::unique_ptr<YCSB_DB> db, uint32_t benchmarkTime, uint32_t txRate);

    public:
        using KVPair = std::pair<std::string, std::string>;
        ///
        /// Reads a record from the database.
        /// Field/value pairs from the result are stored in a vector.
        ///
        /// @param table The name of the table.
        /// @param key The key of the record to read.
        /// @param fields The list of fields to read, or NULL for all of them.
        /// @param result A vector of field/value pairs for the result.
        /// @return true on success, or false on error/record-miss.
        ///
        virtual int Read(const std::string &table, const std::string &key,
                         const std::vector<std::string> *fields,
                         std::vector<KVPair> &result) = 0;
        ///
        /// Performs a range scan for a set of records in the database.
        /// Field/value pairs from the result are stored in a vector.
        ///
        /// @param table The name of the table.
        /// @param key The key of the first record to read.
        /// @param record_count The number of records to read.
        /// @param fields The list of fields to read, or NULL for all of them.
        /// @param result A vector of vector, where each vector contains field/value
        ///        pairs for one record
        /// @return true on success, or false on error.
        ///
        virtual int Scan(const std::string &table, const std::string &key,
                         int record_count, const std::vector<std::string> *fields,
                         std::vector<std::vector<KVPair>> &result) = 0;
        ///
        /// Updates a record in the database.
        /// Field/value pairs in the specified vector are written to the record,
        /// overwriting any existing values with the same field names.
        ///
        /// @param table The name of the table.
        /// @param key The key of the record to write.
        /// @param values A vector of field/value pairs to update in the record.
        /// @return true on success, false on error.
        ///
        virtual int Update(const std::string &table, const std::string &key,
                           std::vector<KVPair> &values) = 0;
        ///
        /// Inserts a record into the database.
        /// Field/value pairs in the specified vector are written into the record.
        ///
        /// @param table The name of the table.
        /// @param key The key of the record to insert.
        /// @param values A vector of field/value pairs to insert in the record.
        /// @return true on success, false on error.
        ///
        virtual int Insert(const std::string &table, const std::string &key,
                           std::vector<KVPair> &values) = 0;
        ///
        /// Deletes a record from the database.
        ///
        /// @param table The name of the table.
        /// @param key The key of the record to delete.
        /// @return true on success, false on error.
        ///
        virtual int Delete(const std::string &table, const std::string &key) = 0;
    };

}

#endif  // YCSB_C_DB_H_
