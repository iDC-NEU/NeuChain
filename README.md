# NeuChain: A Fast Permissioned Blockchain System with Deterministic Ordering
This is the source code of our published paper at VLDB 22.
* Peng, Zeshun, Yanfeng Zhang, Qian Xu, Haixu Liu, Yuxiao Gao, Xiaohua Li, and Ge Yu. "NeuChain: a fast permissioned blockchain system with deterministic ordering." Proceedings of the VLDB Endowment 15, no. 11 (2022): 2585-2598.
# Download
```sh
git clone https://github.com/iDC-NEU/NeuChain.git
```
## Branch description
* ev branch: our system
* eov branch: the eov variation
* oe branch: the oe variation
* oepv branch: the oepv version

# Dependencies
The following is the build guide in the ubuntu20.04 environment (in docker or in a host machine).
You can also use the DockerFile to init a build environment, and build the binary inside the container.
You may need to change the CMakeLists.txt to build the binaries.
```sh
# in home folder
sudo apt update;
sudo apt install -y unzip;
unzip -q NeuChain.zip;
cd NeuChain/;
sudo sh ./install_deps.sh;
```
Dependency installation scripts are optimized for docker.
When executing on a host or a VPS, you can remove the **ssh installation and configuration part** of the script.

# Build
* cmake version 3.16.3
* gcc version 9.4.0 (Ubuntu 9.4.0-1ubuntu1~20.04.1)
```sh
cmake -B build -D CMAKE_BUILD_TYPE=Release
# build the block server
cmake --build build --target block_server_test_comm -j$(nproc)
# build the epoch server
cmake --build build --target epoch_server -j$(nproc)
# build the user
cmake --build build --target user -j$(nproc)
```

# Before Run
We recommend using the following tools for a quick deployment.
You can use the following methods to manually compile the installation package, or download the package through the following link.
```sh
wget https://github.com/iDC-NEU/neuchain_deployer/releases/download/v1.1/release_0.30_v5.zip
```

## 1. build the binary of the deployer
```sh
# in home folder
git clone https://github.com/iDC-NEU/neuchain_deployer.git
cd neuchain_deployer/
cmake -B build -D CMAKE_BUILD_TYPE=Release
cmake --build build --target block_server_test_comm -j$(nproc)
cmake --build build --target protobuf -j$(nproc)
# build the server, each server is on a separate machine.
cmake --build build --target aria_deliver_server -j$(nproc)
# build the client, The client controls the start and stop of all nodes
cmake --build build --target deliver_client -j$(nproc)
```
## 2. Create the app folder
```sh
# in home folder
mkdir -p "neuchain_release/bin/crypto"
cp neuchain_deployer/build/aria_deliver_server ./neuchain_release/
cp neuchain_deployer/build/deliver_client ./neuchain_release/
cp NeuChain/build/src/block_server/block_server_test_comm ./neuchain_release/bin/
cp NeuChain/build/src/epoch_server/epoch_server ./neuchain_release/bin/
cp NeuChain/build/src/user/user ./neuchain_release/bin/
```
* This is a 4 server cluster config.yaml example for **the deployer**
**Please do not confuse with NeuChain's configuration.**
```sh
cp NeuChain/doc/4_servers_example.yaml ./neuchain_release/config.yaml
cp NeuChain/doc/install.sh ./neuchain_release/install.sh
chmod +x ./neuchain_release/install.sh
```
* Copy all dynamic libraries to the so folder
```sh
# in home folder
mkdir -p "neuchain_release/so"
cp /usr/local/lib/*.so neuchain_release/so/
```

* Copy **NeuChain's configuration** to bin folder.
```sh
# in home folder
cp NeuChain/doc/config_template_4_servers.yaml ./neuchain_release/bin/config-template.yaml
cp NeuChain/doc/config_template_local.yaml ./neuchain_release/bin/config_local.yaml
cp NeuChain/doc/init_crypto.yaml ./neuchain_release/bin/config_crypto.yaml
```
## 2.2 Init the database
* Uncomment line [48-49 in src/block_server/server.cpp](src/block_server/server.cpp) to initialize the database.
* In order to ensure that the databases of each node are the same, the generated databases are the same each time the initialization is performed.
* So the database only needs to be initialized once.
```sh
# in home folder
# Modify src/block_server/server.cpp
cat src/block_server/server.cpp
# Recompile block_server_test_comm
cd NeuChain
cmake --build build --target block_server_test_comm -j$(nproc)
cd ..
# copy binary
cp NeuChain/build/src/block_server/block_server_test_comm ./neuchain_release/bin/db_init
cd ./neuchain_release/bin/
# in home/neuchain_release/bin/ folder
cp config_local.yaml config.yaml  # use local config to init db
./db_init
# backup database
mv small_bank small_bank_bk
mv ycsb ycsb_bk
```
## 2.2 Init pub&pri keys for peers
* edit **neuchain_release/bin/config.yaml**
```sh
# in home/neuchain_release/bin/ folder
cp config_crypto.yaml config.yaml  # use crypto config to init crypto
./user -b 1 1 1 # Can be terminated when prompted "all worker started"
# the public and private keys can be seen in ./crypto folder
```
## 2.3 Compress the whole folder and send to all peers.
* Distribute compressed packages using scp or other software.

# Spin up the system

## 3.1 Spin up the deployment tool (in all servers and client).
Unzip your archive on each peer, the example shows the zip file downloaded from the url.
* Open a terminal on each server:
```shell
apt update;
apt install unzip;
unzip release_0.XX.zip;
cd release_0.XX;
./install.sh; # install so files
./aria_deliver_server # start the deployer server
```
## 3.2 Modify and update ips on deployer clients' shell.
* Open a new terminal in deployer client:
* Replace the IP in release_0.XX/config.yaml file. [example](doc/4_servers_example.yaml)

```shell
   ./deliver_client
```
* press c+ENTER # Update configuration files on all servers

## 3.3 Spin up the servers
* On the terminal of deployer client:
```shell
   ./deliver_client
```
* press o+ENTER # for spin up all peers
* press d+ENTER # for tear down all servers and clean the environment

## 3.4 Spin up the user
* Open a new terminal on the peer machine as the user
```shell
# in release_0.XX or neuchain_release
cd bin
./user [worker count] [tps of each worker] [duration]
```
* You can also uncomment [line 75-78 in doc/4_servers_example.yaml](doc/4_servers_example.yaml) to let the user start along with other peers.
* It is better to start all client proxies at the same time to reduce the probability of errors.
* Please start the test after the system is stable.
