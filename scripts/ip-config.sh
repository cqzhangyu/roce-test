#!/usr/bin/sh

ID=${1}
if [ ! ${ID} ]; then
    echo "Usage: ip_config.sh workerID"
    exit 0
fi

# sudo ip addr add 192.168.0.${ID}/24 dev ibs10f0
# sudo ip link set ibs10f0 up

sudo ip addr flush dev ens10f1np1
sudo ip addr add 192.168.1.${ID}/24 dev ens10f1np1
sudo ip link set ens10f1np1 up

sudo arp -s 192.168.1.1 10:70:fd:19:00:95 -i ens10f1np1
sudo arp -s 192.168.1.2 10:70:fd:2f:d8:51 -i ens10f1np1
sudo arp -s 192.168.1.3 10:70:fd:2f:e4:41 -i ens10f1np1
sudo arp -s 192.168.1.4 10:70:fd:2f:d4:21 -i ens10f1np1
sudo arp -s 192.168.1.100 00:02:00:00:03:00 -i ens10f1np1
