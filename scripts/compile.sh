#!/bin/bash

scrp_path=$(cd `dirname $0`; pwd)
. ${scrp_path}/utils.sh

################################################################

THIS_PATH="scripts/compile.sh"

show_usage() {
    appname=$0
    echo_info "Usage: ${appname} [Options]"
    echo_info "     clean          clean generated files"
}

do_compile() {
    echo_back "mkdir -p ${build_path}; cd ${build_path}"
    echo_back "cmake ${proj_path}"
    echo_back "make -j 8"
}

do_clean() {
    echo_back "rm -rf ${build_path}"
}

# echo_back "cmake .."
# echo_back "make"

if [ $# -eq 0 ]; then
    do_compile
else
    case $1 in
        clean)
            do_clean
            ;;
        \?)
            show_usage
            ;;
    esac
fi
