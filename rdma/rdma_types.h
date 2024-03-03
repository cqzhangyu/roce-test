#pragma once

#include <cstdio>
#include <iostream>
#include <algorithm>
#include <memory>
#include <map>
#include <vector>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <endian.h>
#include <fstream>
#include <thread>
#include <csignal>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sched.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <ifaddrs.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <sys/ioctl.h>
#include <net/if.h>


#include <infiniband/verbs.h>

#define MAX_NUM_ENDPOINT        8
#define MAX_NUM_QP              16
#define MAX_Q_SIZE              1024
#define MAX_SOCKET_BUFSIZE      4096

struct endpoint_addr {
    int rank;
    int lid;
    int psn;
    int qpn;
    uint32_t rkey;
    void *addr;
    union ibv_gid gid;
};
