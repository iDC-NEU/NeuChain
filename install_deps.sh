#!/usr/bin/env bash

set -e

# shellcheck disable=SC1090
# source "${PROJECT_TOOLS_DIR}/etc/env.sh"
export DEBIAN_FRONTEND=noninteractive

apt-get update

# Install development requirements
apt-get install -y git make g++ cmake autoconf automake libtool openssl libssl-dev pkg-config m4 libunwind-dev

# install ssh connector
apt-get install -y openssh-server gdb
echo service ssh start >>/root/.bashrc

# install postgre for debugging
apt-get install -y libpq-dev postgresql-server-dev-all #postgresql

# add TCMALLOC(fPIC build)
git clone --recurse-submodules https://github.com/gperftools/gperftools
cd gperftools
git checkout gperftools-2.9.1
# shellcheck disable=SC2046
cmake -B build -DCMAKE_CXX_FLAGS=-fPIC -D CMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc) && cmake --install build
cd ..
rm -rf leveldb

# add leveldb(fPIC build)
git clone --recurse-submodules https://github.com/google/leveldb
cd leveldb
git checkout 1.23
# shellcheck disable=SC2046
cmake -B build -DCMAKE_CXX_FLAGS=-fPIC -D CMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc) && cmake --install build
cd ..
rm -rf leveldb

# add gflags(fPIC build)
git clone --recurse-submodules https://github.com/gflags/gflags
cd gflags
git checkout v2.2.2
# shellcheck disable=SC2046
cmake -B build -DCMAKE_CXX_FLAGS=-fPIC -D CMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc) && cmake --install build
cd ..
rm -rf gflags

# add gtest
git clone --recurse-submodules https://github.com/google/googletest
cd googletest
git checkout release-1.11.0
# shellcheck disable=SC2046
cmake -B build -DCMAKE_CXX_FLAGS=-fPIC -D CMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc) && cmake --install build
cd ..
rm -rf googletest

# add glog
git clone --recurse-submodules https://github.com/google/glog
cd glog
git checkout v0.5.0
# shellcheck disable=SC2046
cmake -B build -D CMAKE_BUILD_TYPE=Release -DWITH_GFLAGS=OFF && cmake --build build -j$(nproc) && cmake --install build
cd ..
rm -rf glog

# add zlib
git clone --recurse-submodules https://github.com/madler/zlib
cd zlib
# shellcheck disable=SC2046
cmake -B build -D CMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc) && cmake --install build
cd ..
rm -rf zlib

# add zlib-ng
git clone  --recurse-submodules https://github.com/zlib-ng/zlib-ng
cd zlib-ng
git checkout 2a9879494b6fc0d9d09686e029f72f304496211c
# shellcheck disable=SC2046
cmake -B build -D CMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc) && cmake --install build
cd ..
rm -rf zlib-ng

# add protobuf (enable gzip, version=v3.16.0)
git clone --recurse-submodules https://github.com/protocolbuffers/protobuf
cd protobuf
git checkout v3.19.4
./autogen.sh
./configure
# shellcheck disable=SC2046
make -j$(nproc)
# shellcheck disable=SC2046
# make check -j$(nproc)
make install
ldconfig
cd ..
rm -rf protobuf

# add brpc
git clone --recurse-submodules https://github.com/apache/incubator-brpc
cd incubator-brpc
git checkout 4839ec2add93b439ccfe2761ce76a8914952992a
# shellcheck disable=SC2046
cmake -B build -D CMAKE_BUILD_TYPE=Release -D WITH_GLOG=ON -D DOWNLOAD_GTEST=OFF && cmake --build build -j$(nproc) && cmake --install build
cd ..
rm -rf incubator-brpc

# add braft
git clone --recurse-submodules https://github.com/baidu/braft
cd braft
git checkout c8e6848aaea69f6750f2fd1618caec84cbef2fae
# shellcheck disable=SC2046
cmake -B build -D CMAKE_BUILD_TYPE=Release -D BRPC_WITH_GLOG=ON && cmake --build build -j$(nproc) && cmake --install build
cd ..
rm -rf braft

# add yaml-cpp
git clone --recurse-submodules https://github.com/jbeder/yaml-cpp
cd yaml-cpp
git checkout yaml-cpp-0.7.0
# shellcheck disable=SC2046
cmake -B build -D CMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc) && cmake --install build
cd ..
rm -rf yaml-cpp

# add libzmq
git clone --recurse-submodules https://github.com/zeromq/libzmq
cd libzmq
git checkout 4b48007927bcd1c91548b1844c142dd5ee04d314
./autogen.sh
# shellcheck disable=SC2046
cmake -B build -D WITH_PERF_TOOL=OFF -D ZMQ_BUILD_TESTS=OFF -D ENABLE_CPACK=OFF -D CMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc) && cmake --install build
cd ..
rm -rf libzmq

# add cppzmq
git clone --recurse-submodules https://github.com/zeromq/cppzmq
cd cppzmq
git checkout v4.8.1
# shellcheck disable=SC2046
cmake -B build -D CMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc) && cmake --install build
cd ..
rm -rf cppzmq

# add libpqxx 7.3.1
git clone --recurse-submodules https://github.com/jtv/libpqxx
cd libpqxx
git checkout 7.3.1
# shellcheck disable=SC2046
cmake -B build -D CMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc) && cmake --install build
cd ..
rm -rf libpqxx

#sed s/PermitRootLogin/#PermitRootLogin/ /etc/ssh/sshd_config
#echo PermitRootLogin yes >> /etc/.ssh/sshd_config
# install public key
mkdir -p /root/.ssh
touch /root/.ssh/authorized_keys
echo ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQDMrEYiFCD/0x37i9dO+iOELPIGZr4k2j9AepPG4jM4qPp4/Zn4+nowEhPsHSMia6Z1rIiy\
ElkdwOchLlYDKRaMdFTT3sd0g7Hl8hrDPLo57oyf71DxKvLE5V1lFjEEO0MCOD8VPR9A9pA0BfmCB7yaYCh/yYzWdLm3aThoKwWYt8FQkwvSiiy4\
f7wLwc1rWLCHm4srQlwORVTGwIIxnLoJI2hw0fZg9fKny7MM1rsSH4rygy1XkfhE34fkwNqjHrV1xsbyPTjsvltuPqTOx9HVz2ceYJuBxD2VuR82\
PLVThUplONd2V5wwUnPx/s+bc9VNmKli5g1lRGtGcoSFnnWL skp-d7odiiias60axgtankmu > /root/.ssh/authorized_keys
