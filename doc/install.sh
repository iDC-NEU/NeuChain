echo installing...
apt update;
apt install -y unzip sshpass bmon;
kill -9 $(pidof aria_deliver_server);
sudo cp so/lib* /usr/lib/
chmod 755 bin/block_server_test_comm
chmod 755 bin/epoch_server
chmod 755 bin/user
./aria_deliver_server