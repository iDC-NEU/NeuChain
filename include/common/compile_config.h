//
// Created by peng on 2021/1/16.
//
#pragma once

// this file must not used in benchmark
// because after changed settings in this file, recompile may takes TOO LONG TIME!

// define: use file to save block
// undef: use db to save block
//#define USING_MEMORY_BLOCK

// define: use level db
// undef: use postgre sql
//#define USING_KV_DB
#define USING_LEVEL_DB

#define MAX_EPOCH 10000

#define RAFT_TIMEOUT_MS 1000

// for calculate time
#define NO_TIME_BENCHMARK
#define TIME_BENCHMARK_MAX_EPOCH 200

// define: use spin lock in reserve table (maybe speed up or not)
// undef: use std::mutex instead
//#define USING_SPIN_LOCK

// define: use mvcc reserve table
// undef: use hash map reserve table
//#define USING_MVCC_RESERVE_TABLE

// the max batch size that tr manager get from comm interface
#define TR_MGR_GET_BATCH_MAX_SIZE 5000
// the min batch size that tr manager get from comm interface
#define TR_MGR_GET_BATCH_MIN_SIZE 500

#ifdef USING_KV_DB
#define DB_STORAGE_TYPE HashMapStorage
#elif defined USING_LEVEL_DB
#define DB_STORAGE_TYPE LevelDBStorage
#else
#define DB_STORAGE_TYPE DBStorageImpl
#endif

#ifdef USING_SPIN_LOCK
#define HASH_MAP_LOCK_TYPE SpinLock
#else
#define HASH_MAP_LOCK_TYPE std::mutex
#endif

#ifdef USING_MVCC_RESERVE_TABLE
#define GET_RESERVE_TABLE_TYPE getMVCCReserveTable
#else
#define GET_RESERVE_TABLE_TYPE getHashMapReserveTable
#endif
