#pragma once

#include "../common/config.hpp"
#include "../common/types.h"
#include "../common/utils.h"
#include "shuffle_drv.hpp"

#include <string>
#include <vector>

class ShuffleMaster {
public:
    int n_ep;
    int fp_rank[MAX_NUM_ENDPOINT];
    struct endpoint_info ep_infos[MAX_NUM_ENDPOINT];
    struct shuffle_qp_info qp_infos[MAX_NUM_ENDPOINT];

    uint32_t master_ip;
    uint16_t master_port;
    
    ShuffleDrv drv;

    int listen_fd;
    int conn_fds[MAX_NUM_ENDPOINT];
    int n_conn;

    volatile bool is_closed;

    enum master_state {
        IDLE,
        LISTEN,
        RUNNING, 
        CLOSED
    }state;

    ShuffleMaster(Config &cfg)
        : n_ep(cfg.n_endpoint),
          drv(cfg, n_ep, fp_rank, ep_infos, qp_infos) {

        master_ip = ntohl(inet_addr(cfg.master_ip.c_str()));
        master_port = cfg.master_port;
        state = IDLE;
        is_closed = false;
    }

    void bind() {
        
        int ret;
        listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        logassert(listen_fd < 0, "Cannot create socket");
        struct sockaddr_in master_addr = {
            .sin_family = AF_INET,
            .sin_port = htons(master_port),
            .sin_addr = { .s_addr = htonl(master_ip) },
            .sin_zero = {},
        };

        int opt = 1;
        ret = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));
        logassert(ret < 0, "Cannot setsockopt");

        ret = ::bind(listen_fd, (struct sockaddr *)&master_addr, sizeof(master_addr));
        logassert(ret < 0, "Cannot bind");
    }

    void listen() {
        int ret;
        n_conn = 0;
        ret = ::listen(listen_fd, n_ep);
        logassert(ret < 0, "Cannot listen");
    }

    void accept() {
        ssize_t ret;
        struct sockaddr conn_addr;
        socklen_t conn_addr_len;
        char socket_buf[MAX_SOCKET_BUFSIZE];
        struct msg_accept msg_acc;
        for (int i = 0; i < n_ep; i ++) {

            int conn_fd = ::accept(listen_fd, &conn_addr, &conn_addr_len);
            logassert(conn_fd < 0, "Cannot accept");
            loginfo("Accepted connection from ", sin_to_str((struct sockaddr_in *)&conn_addr));

            msg_acc.type = MSG_TYPE_ACCEPT;
            msg_acc.n_ep = n_ep;
            msg_acc.rank = n_conn;
            
            ret = send(conn_fd, &msg_acc, sizeof(struct msg_accept), 0);
            logassert(ret != sizeof(struct msg_accept), "Wrong send length");
            loginfo("Endpoint ", sin_to_str((struct sockaddr_in *)&conn_addr), " has rank ", n_conn);

            ret = recv(conn_fd, socket_buf, sizeof(struct msg_gather), 0);
            logassert(ret != sizeof(struct msg_gather), "Unexpected recv length");
            struct msg_gather *msg_gat = (struct msg_gather *)socket_buf;
            logassert(msg_gat->type != MSG_TYPE_GATHER, "Invalid message type");
            
            memcpy(&ep_infos[n_conn], &msg_gat->ep_info, sizeof(struct endpoint_info));
            memcpy(&qp_infos[n_conn], &msg_gat->qp_info, sizeof(struct shuffle_qp_info));

            conn_fds[i] = conn_fd;
        }
    }

    void scatter() {
        ssize_t ret;
        struct msg_scatter msg_sca;
        msg_sca.type = MSG_TYPE_SCATTER;
        memcpy(msg_sca.ep_infos, ep_infos, sizeof(struct endpoint_info) * n_ep);
        msg_sca.dqp_info.req_qpn = vir_qp_info.req_qpn;
        msg_sca.dqp_info.dst_qpn = vir_qp_info.dst_qpn;
        memcpy(msg_sca.dqp_info.src_qpn, vir_qp_info.src_qpn, sizeof(vir_qp_info.src_qpn));

        for (int i = 0; i < n_ep; i ++) {
            int conn_fd = conn_fds[i];
            for (int j = 0; j < n_ep; j ++) {
                msg_sca.dqp_info.nor_qpn[j] = qp_infos[j].nor_qpn[i];
            }
            ret = send(conn_fd, &msg_sca, sizeof(struct msg_scatter), 0);
            logassert(ret != sizeof(struct msg_scatter), "Wrong send size");
        }
        state = RUNNING;
    }

    void join() {
        ssize_t ret;
        char socket_buf[MAX_SOCKET_BUFSIZE];
        for (int i = 0; i < n_ep; i ++) {
            int conn_fd = conn_fds[i];
            
            ret = recv(conn_fd, socket_buf, sizeof(struct msg_finish), 0);
            logassert(ret != sizeof(struct msg_finish), "Wrong recv length");
            struct msg_finish *msg_fin = (struct msg_finish *)socket_buf;
            logassert(msg_fin->type != MSG_TYPE_FINISH, "Invalid message type");
        }
    }

    void close() {
        ssize_t ret;
        struct msg_close msg_clo;
        msg_clo.type = MSG_TYPE_CLOSE;
        for (int i = 0; i < n_ep; i ++) {
            int conn_fd = conn_fds[i];
            ret = send(conn_fd, &msg_clo, sizeof(struct msg_close), 0);
            logassert(ret!= sizeof(struct msg_close), "Wrong send length");

            ::close(conn_fd);
        }
    }

    void loop() {
        while (!is_closed) {
            listen();
            accept();
            drv.init_tables();
            drv.init_registers();
            scatter();
            join();
            close();
            drv.clear_tables();
        }
    }

    void run() {
        drv.start();
        bind();
        loop();
    }

    void stop() {
        is_closed = true;
    }
};
