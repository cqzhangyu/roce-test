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
#include <chrono>

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

#define MAX_NUM_ENDPOINT        8
#define MAX_NUM_QP              16
#define MAX_Q_SIZE              1024
#define MAX_SOCKET_BUFSIZE      4096

#define RDMA_OP_SEND_FIRST          0x00
#define RDMA_OP_SEND_MIDDLE         0x01
#define RDMA_OP_SEND_LAST           0x02
#define RDMA_OP_SEND_LAST_WITH_IMM  0x03
#define RDMA_OP_SEND_ONLY           0x04
#define RDMA_OP_SEND_ONLY_WITH_IMM  0x05
#define RDMA_OP_WRITE_FIRST         0x06
#define RDMA_OP_WRITE_MIDDLE        0x07
#define RDMA_OP_WRITE_LAST          0x08
#define RDMA_OP_WRITE_LAST_WITH_IMM 0x09
#define RDMA_OP_WRITE_ONLY          0x0a // WRITE_ONLY occurs when the message is shorter than RDMA_MTU
#define RDMA_OP_WRITE_ONLY_WITH_IMM 0x0b
#define RDMA_OP_READ_REQ            0x0c
#define RDMA_OP_READ_RES_FIRST      0x0d
#define RDMA_OP_READ_RES_MIDDLE     0x0e
#define RDMA_OP_READ_RES_LAST       0x0f
#define RDMA_OP_READ_RES_ONLY       0x10
#define RDMA_OP_ACK                 0x11
#define RDMA_OP_CNP                 0x81


struct shuffle_qp_info {
    int req_qpn;
    int dst_qpn;
    int src_qpn[MAX_NUM_ENDPOINT];
    int nor_qpn[MAX_NUM_ENDPOINT];
};

struct endpoint_info {
    int rank;
    int lid;
    int psn;
    int qpn;
    uint32_t rkey;
    void *addr;
    char gid[16];
};

#define MSG_TYPE_ACCEPT     0
#define MSG_TYPE_GATHER     1
#define MSG_TYPE_SCATTER    2
#define MSG_TYPE_FINISH     3
#define MSG_TYPE_CLOSE      4


struct msg_accept {
    int type;
    int n_ep;
    int rank;
};

struct msg_gather {
    int type;
    struct endpoint_info ep_info;
    struct shuffle_qp_info qp_info;
};

struct msg_scatter {
    int type;
    struct endpoint_info ep_infos[MAX_NUM_ENDPOINT];
    struct shuffle_qp_info dqp_info;
};

struct msg_finish {
    int type;
};

struct msg_close {
    int type;
};
