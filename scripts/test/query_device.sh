#!/bin/bash

scrp_path=$(cd `dirname $0`; cd ..; pwd)
. ${scrp_path}/utils.sh

################################################################

MASTER_IP=192.168.1.1
MASTER_PORT=12345
ENDPOINT_IP_PREFIX=192.168.1
IB_PORT=1
GID_INDEX=2


if [ $# -eq 0 ]; then
    echo_erro "Require the worker id"
else
    echo_back "${build_path}/endpoint/query_device \
                --master_ip=${MASTER_IP} \
                --master_port=${MASTER_PORT} \
                --bind_ip=${ENDPOINT_IP_PREFIX}.$1 \
                --ib_port=${IB_PORT} \
                --gid_index=${GID_INDEX}"
fi
