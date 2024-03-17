#!/bin/bash

scrp_path=$(cd `dirname $0`; pwd)
. ${scrp_path}/utils.sh

################################################################

APP=$1

p4_path=${proj_path}/p4

common_p4=${p4_path}/common
app_path=${p4_path}/${APP}
p4p4_path=${p4_path}/${APP}/${APP}.p4

if [ ! -e ${p4p4_path} ]; then
    echo_erro "p4 file not found."
    exit 1
fi

p4build_path=${build_path}/p4

echo_info "compling p4 project: ${p4p4_path}"
echo_back "mkdir -p ${p4build_path}; cd ${p4build_path}"

# . $SDE/tools/p4_build.sh  $CURRENT/$APP/$APP.p4 \
#     --with-p4c=bf-p4c \
#     P4_NAME=$APP \
#     P4FLAGS="--create-graphs --no-dead-code-elimination" \
#     P4_VERSION=p4_16 \
#     P4PPFLAGS="-I ${common_p4}" 

cmake ${SDE}/p4studio/                    \
    -DCMAKE_INSTALL_PREFIX=${SDE_INSTALL} \
    -DCMAKE_MODULE_PATH=${SDE}/cmake      \
    -DP4_NAME=${APP}                      \
    -DP4_PATH=${p4p4_path}                \
    -DP4_LANG=p4_16                       \
    -DP4PPFLAGS="-I ${app_path}"         \
    -DP4FLAGS="--verbose 2 --create-graphs -g"

echo_back "make -j 4 && make install"
