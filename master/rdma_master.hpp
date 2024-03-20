#pragma once
#include "../common/logger.hpp"
#include "../common/config.hpp"
#include "../common/types.h"
#include "../common/utils.h"

class RDMAMaster {
public:

    int n_endpoint;
    uint32_t master_ip;
    uint16_t master_port;
    
    struct endpoint_info *ep_infos;
    int *qpns;

    void master_exchange_address() {
        int ret;
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        logassert(fd < 0, "Cannot create socket");
        struct sockaddr_in master_addr = {
            .sin_family = AF_INET,
            .sin_port = htons(master_port),
            .sin_addr = { .s_addr = htonl(master_ip) },
            .sin_zero = {},
        };

        int opt = 1;
        ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));
        logassert(ret < 0, "Cannot setsockopt");

        int fd_list[n_endpoint];
        ret = bind(fd, (struct sockaddr *)&master_addr, sizeof(master_addr));
        logassert(ret < 0, "Cannot bind");

        loginfo("Listening on ", sin_to_str(&master_addr));
        ret = listen(fd, n_endpoint);
        logassert(ret < 0, "Cannot listen");

        ep_infos = new struct endpoint_info[n_endpoint];
        qpns = new int[n_endpoint * (n_endpoint - 1)];

        char socket_buf[MAX_SOCKET_BUFSIZE];
        for (int i = 0; i < n_endpoint; i ++) {
            struct sockaddr conn_addr;
            socklen_t conn_addr_len;
            int conn_fd = accept(fd, &conn_addr, &conn_addr_len);
            logassert(conn_fd < 0, "Cannot accept");
            loginfo("Accepted connection from ", sin_to_str((struct sockaddr_in *)&conn_addr));

            /*
                int n_dst;
                int rank;
            */
            *(int *)socket_buf = n_endpoint - 1;
            *(int *)(socket_buf + sizeof(int)) = i;
            
            ret = write(conn_fd, socket_buf, sizeof(int) * 2);
            logassert(ret < (int)sizeof(int) * 2, "Partial write()");
            loginfo("Endpoint ", sin_to_str((struct sockaddr_in *)&conn_addr), " has rank ", i);

            /*
                struct endpoint_info my_addr;
                int qpns[n_dst];
            */
            int msg_len = sizeof(struct endpoint_info) + sizeof(int) * (n_endpoint - 1);
            ret = read(conn_fd, socket_buf, msg_len);
            logassert(ret < msg_len, "Partial read()");
            
            memcpy(&ep_infos[i], socket_buf, sizeof(struct endpoint_info));
            memcpy(qpns + i * (n_endpoint - 1), socket_buf + sizeof(struct endpoint_info), msg_len - sizeof(struct endpoint_info));
            
            fd_list[i] = conn_fd;
        }

        for (int i = 0; i < n_endpoint; i ++) {
            int conn_fd = fd_list[i];
            for (int j = 0; j < n_endpoint; j ++) {
                if (j != i) {
                    struct endpoint_info dst_info;
                    memcpy(&dst_info, &ep_infos[j], sizeof(struct endpoint_info));
                    dst_info.qpn = qpns[j * (n_endpoint - 1) + i - (i > j)];
                    memcpy(socket_buf + sizeof(struct endpoint_info) * (j - (j > i)), &dst_info, sizeof(struct endpoint_info));
                }
            }
            ret = write(conn_fd, socket_buf, sizeof(struct endpoint_info) * (n_endpoint - 1));
            logassert(ret < (int)sizeof(struct endpoint_info) * (n_endpoint - 1), "Partial write");
            close(conn_fd);
        }

    }

    RDMAMaster(Config &cfg) {
        n_endpoint = cfg.n_endpoint;

        master_ip = ntohl(inet_addr(cfg.master_ip.c_str()));
        master_port = cfg.master_port;
    }

    void run() {
        master_exchange_address();
    }
};
