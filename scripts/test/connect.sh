#!/bin/bash

scrp_path=$(cd `dirname $0`; cd ..; pwd)
. ${scrp_path}/utils.sh

################################################################

MASTER_IP=10.0.0.100
MASTER_PORT=12345
ENDPOINT_IP_PREFIX=192.168.1
ENDPOINTS=(1)
N_ENDPOINT=${#ENDPOINTS[@]}
IB_PORT=1
GID_INDEX=3
PSN=0
N_CORE=1
Q_SIZE=64
MR_SIZE=$((1024*1024*1024))
MTU=1024

LOG_LEVEL=DEBUG

run_master() {
    echo_back "${build_path}/switchd/shuffle_master \
                --p4_name=shuffle \
                --master_ip=${MASTER_IP} \
                --master_port=${MASTER_PORT} \
                --n_endpoint=${N_ENDPOINT} \
                --log_level=${LOG_LEVEL}"
}

run_endpoint() {
    
    echo_back "${build_path}/endpoint/shuffle_endpoint \
                --master_ip=${MASTER_IP} \
                --master_port=${MASTER_PORT} \
                --bind_ip=${ENDPOINT_IP_PREFIX}.$1 \
                --ib_port=${IB_PORT} \
                --gid_index=${GID_INDEX} \
                --psn=${PSN} \
                --n_core=${N_CORE} \
                --q_size=${Q_SIZE} \
                --mr_size=${MR_SIZE} \
                --mtu=${MTU} \
                --log_level=${LOG_LEVEL}"
}

case $1 in
    master)
        run_master
        ;;
    endpoint)
        run_endpoint $2
        ;;
esac
