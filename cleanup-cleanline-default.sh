rm -f cmake-build-release/src/block_server/*.bin
rm -rf cmake-build-release/src/block_server/ycsb
rm -rf cmake-build-release/src/block_server/small_bank
rm -rf cmake-build-release/src/block_server/raft_*
rm -rf cmake-build-release/src/epoch_server/raft_*
rm -f cmake-build-debug/src/block_server/*.bin
rm -rf cmake-build-debug/src/block_server/ycsb
rm -rf cmake-build-debug/src/block_server/small_bank
rm -rf cmake-build-debug/src/block_server/raft_*
rm -rf cmake-build-debug/src/epoch_server/raft_*
cp -r cmake-build-debug/src/block_server/ycsb_bk cmake-build-debug/src/block_server/ycsb
cp -r cmake-build-debug/src/block_server/small_bank_bk cmake-build-debug/src/block_server/small_bank
cp -r cmake-build-release/src/block_server/ycsb_bk cmake-build-release/src/block_server/ycsb
cp -r cmake-build-release/src/block_server/small_bank_bk cmake-build-release/src/block_server/small_bank