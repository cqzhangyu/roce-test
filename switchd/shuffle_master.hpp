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
    uint32_t fp_rank[MAX_NUM_ENDPOINT];
    struct endpoint_info ep_infos[MAX_NUM_ENDPOINT];
    struct shuffle_qp_info qp_infos[MAX_NUM_ENDPOINT];

    uint32_t master_ip;
    uint16_t master_port;
    
    ShuffleDrv drv;

    int listen_fd;
    int conn_fds[MAX_NUM_ENDPOINT];

    volatile bool is_closed;

    ShuffleMaster(Config &cfg)
        : n_ep(cfg.n_endpoint),
          drv(cfg, n_ep, fp_rank, ep_infos, qp_infos) {

        master_ip = ntohl(inet_addr(cfg.master_ip.c_str()));
        master_port = cfg.master_port;
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
        loginfo("Start listening on ", ip_to_str(htonl(master_ip)), ":", master_port);
        ret = ::listen(listen_fd, n_ep);
        logassert(ret < 0, "Cannot listen");
    }

    void accept() {
        ssize_t ret;
        struct sockaddr conn_addr;
        socklen_t conn_addr_len = sizeof(conn_addr);
        char socket_buf[MAX_SOCKET_BUFSIZE];
        struct msg_accept msg_acc;
        for (int i = 0; i < n_ep; i ++) {

            int conn_fd = ::accept(listen_fd, &conn_addr, &conn_addr_len);
            logassert(conn_fd < 0, "Cannot accept");
            loginfo("Accepted connection from ", sin_to_str((struct sockaddr_in *)&conn_addr));

            // Magic number here!
            // fp_rank is the last byte of the ip address - 1
            fp_rank[i] = (ntohl(((struct sockaddr_in *)&conn_addr)->sin_addr.s_addr) & 0xff) - 1;

            msg_acc.type = MSG_TYPE_ACCEPT;
            msg_acc.n_ep = n_ep;
            msg_acc.rank = i;
            
            ret = send(conn_fd, &msg_acc, sizeof(struct msg_accept), 0);
            logassert(ret != sizeof(struct msg_accept), "Wrong send length");
            loginfo("Endpoint ", sin_to_str((struct sockaddr_in *)&conn_addr), " has rank ", i);

            ret = recv(conn_fd, socket_buf, sizeof(struct msg_gather), 0);
            logassert(ret != sizeof(struct msg_gather), "Unexpected recv length");
            struct msg_gather *msg_gat = (struct msg_gather *)socket_buf;
            logassert(msg_gat->type != MSG_TYPE_GATHER, "Invalid message type");
            
            memcpy(&ep_infos[i], &msg_gat->ep_info, sizeof(struct endpoint_info));
            memcpy(&qp_infos[i], &msg_gat->qp_info, sizeof(struct shuffle_qp_info));

            conn_fds[i] = conn_fd;
        }
    }

    void scatter() {
        ssize_t ret;
        struct msg_scatter msg_sca;
        msg_sca.type = MSG_TYPE_SCATTER;
        memcpy(&msg_sca.vir_ep_info, &vir_ep_info, sizeof(struct endpoint_info));
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

    void dump_reg() {
        drv.dump_reg("ShuffleIngress.endp_state", n_ep);
        drv.dump_reg("ShuffleEgress.req_msn.req_msn", n_ep);
        drv.dump_reg("ShuffleIngress.write_psn.write_psn", n_ep);
        drv.dump_reg("ShuffleEgress.req_epsn.req_epsn", n_ep);
        drv.dump_reg("ShuffleEgress.req_unack_unit.req_unack_unit", n_ep);
        drv.dump_reg("ShuffleIngress.read_unack_psn.read_unack_psn", n_ep * n_ep);
        drv.dump_reg("ShuffleIngress.read_psn.read_psn", n_ep * n_ep);
        drv.dump_reg("ShuffleIngress.read_psn_to_item.read_psn_to_item", 16);
        drv.dump_reg("ShuffleEgress.write_psn_to_unit.write_psn_to_unit", 16);
        drv.dump_reg("ShuffleEgress.item_write_offset.item_write_offset", 16);
        drv.dump_reg("ShuffleEgress.req_dst_addr.req_dst_addr_hi", 16);
        drv.dump_reg("ShuffleEgress.req_dst_addr.req_dst_addr_lo", 16);
        drv.dump_reg("ShuffleEgress.unit_req_psn.unit_req_psn", 16);
        drv.dump_reg("ShuffleEgress.unit_req_msn.unit_req_msn", 16);
        drv.dump_reg("ShuffleEgress.unit_dst_addr.unit_dst_addr_hi", 16);
        drv.dump_reg("ShuffleEgress.unit_dst_addr.unit_dst_addr_lo", 16);
        drv.dump_reg("ShuffleEgress.unit_remain.unit_remain", 16);
        drv.dump_reg("ShuffleIngress.ingress_counter", 1);
        drv.dump_reg("ShuffleEgress.egress_counter", 1);
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
        // dump_reg();
    }

    void loop() {
        while (!is_closed) {
            listen();
            accept();
            drv.clear_tables();
            drv.init_tables();
            drv.init_registers();
            scatter();
            join();
            close();
            while (true) {
                // wait for input
                std::string op;
                std::cin >> op;
                if (op == "q") {
                    stop();
                    break;
                }
                if (op == "c") {
                    break;
                }
                if (op == "r") {
                    dump_reg();
                }
            }
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
