#pragma once

#include <infiniband/verbs.h>

#include "../common/config.hpp"
#include "../common/huge_malloc.h"
#include "../common/logger.hpp"
#include "../common/ring_buffer.hpp"
#include "../common/types.h"
#include "../common/utils.h"

#include "../rdma/rdma_utils.hpp"

class ShuffleEndpoint {
public:
    struct ibv_device   *ib_dev;
    struct ibv_context  *context;
    struct ibv_comp_channel *channel;
    struct ibv_pd   *pd;
    struct ibv_mr   *mr;
    struct ibv_qp           *req_qp;
    struct ibv_qp           *dst_qp;
    vector<struct ibv_qp *> src_qps;
    vector<struct ibv_qp *> nor_qps;
    struct ibv_cq           *req_cq;
    vector<struct ibv_cq *> nor_cqs;
    struct shuffle_qp_info  my_qp_info;
    struct shuffle_qp_info  dst_qp_info;
    struct endpoint_info    my_ep_info;
    struct endpoint_info    vir_ep_info;
    vector<struct endpoint_info> net_ep_infos;
    void            *mr_mmap;
    size_t          mr_size;
    int             ib_port;
    int             gid_index;
    enum ibv_mtu    mtu;
    int             n_ep;
    int             n_core;
    int             q_size;
    int             master_fd;
    uint32_t        master_ip;
    uint16_t        master_port;
    int             psn;

    string bind_ip;
    string dev_name;
    string ib_dev_name;
    int socket_id;

    vector<ring_buffer<uint64_t> *> nor_cq_perthread;
    vector<ring_buffer<uint64_t> *> req_cq_perthread;

    ShuffleEndpoint(Config &cfg) {
        bind_ip = cfg.bind_ip;
        ib_port = cfg.ib_port;
        gid_index = cfg.gid_index;
        n_core = cfg.n_core;
        mr_size = cfg.mr_size;
        mtu = mtu_to_enum(cfg.mtu);
        master_ip = ntohl(inet_addr(cfg.master_ip.c_str()));
        master_port = cfg.master_port;
        psn = cfg.psn;
        q_size = cfg.q_size;
    }
        
    struct ibv_qp *create_qp(struct ibv_cq *cq) {
        int ret;

        struct ibv_qp_init_attr init_attr = {
            .send_cq = cq,
            .recv_cq = cq,
            .cap = {
                .max_send_wr  = (uint32_t) q_size,
                .max_recv_wr  = (uint32_t) q_size,
                .max_send_sge = 1,
                .max_recv_sge = 1
            },
            .qp_type = IBV_QPT_RC
        };

        // QP
        ibv_qp *qp = ibv_create_qp(pd, &init_attr);
        logassert(qp == NULL, "Cannot create qp");

        // Reset -> INIT
        struct ibv_qp_attr attr = {
            .qp_state        = IBV_QPS_INIT,
            .qp_access_flags = IBV_ACCESS_LOCAL_WRITE |
                               IBV_ACCESS_REMOTE_READ |
                               IBV_ACCESS_REMOTE_WRITE,
            .pkey_index      = 0,
            .port_num        = (uint8_t) ib_port
        };

        ret = ibv_modify_qp(qp, &attr, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS);
        logassert(ret != 0, "Cannot set QP to INIT");

        return qp;
    }

    void endpoint_exchange_address() {
        int ret;
        master_fd = socket(AF_INET, SOCK_STREAM, 0);
        logassert(master_fd < 0, "Cannot create socket");
        struct sockaddr_in master_addr = {
            .sin_family = AF_INET,
            .sin_port = htons(master_port),
            .sin_addr = { .s_addr = htonl(master_ip) },
            .sin_zero = {},
        };
        
        int opt = 1;
        ret = setsockopt(master_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));
        logassert(ret < 0, "Cannot setsockopt");

        ret = connect(master_fd, (struct sockaddr *)&master_addr, sizeof(struct sockaddr));
        logassert(ret < 0, "Cannot connect to master");
        loginfo("Connected to master ", sin_to_str(&master_addr));

        char socket_buf[MAX_SOCKET_BUFSIZE];

        ssize_t len;
        len = read(master_fd, socket_buf, sizeof(struct msg_accept));
        logassert(len != sizeof(struct msg_accept), "Wrong read length");
        struct msg_accept *msg_acc = (struct msg_accept *)socket_buf;
        logassert(msg_acc->type!= MSG_TYPE_ACCEPT, "Wrong message type");
        n_ep = msg_acc->n_ep;
        my_ep_info.rank = msg_acc->rank;

        // init QP
        src_qps.resize(n_ep);
        nor_qps.resize(n_ep);
        nor_cqs.resize(n_ep);
        nor_cq_perthread.resize(n_core);
        req_cq_perthread.resize(n_core);

        for (int i = 0; i < n_ep + 1; i ++) {
            // CQ
            struct ibv_cq *cq = ibv_create_cq(context, q_size, NULL, /* channel */NULL, 0);
            logassert(cq == NULL, "Cannot create completion queue");

            if (i < n_ep) {
                nor_cqs[i] = cq;
            }
            else {
                req_cq = cq;
            }
        }

        for (int i = 0; i < n_ep; i ++) {
            struct ibv_qp *qp = create_qp(nor_cqs[i]);
            nor_qps[i] = qp;
            my_qp_info.nor_qpn[i] = qp->qp_num;
        }
        for (int i = 0; i < n_ep; i ++) {
            struct ibv_qp *qp = create_qp(req_cq);
            src_qps[i] = qp;
            my_qp_info.src_qpn[i] = qp->qp_num;
        }
        req_qp = create_qp(req_cq);
        my_qp_info.req_qpn = req_qp->qp_num;
        dst_qp = create_qp(req_cq);
        my_qp_info.dst_qpn = dst_qp->qp_num;

        for (int i = 0; i < n_core; i ++) {
            nor_cq_perthread[i] = new ring_buffer<uint64_t>(q_size);
            req_cq_perthread[i] = new ring_buffer<uint64_t>(q_size);
        }
        loginfo("Created QP");

        struct msg_gather msg_gat;
        msg_gat.type = MSG_TYPE_GATHER;
        memcpy(&msg_gat.ep_info, &my_ep_info, sizeof(struct endpoint_info));
        memcpy(&msg_gat.qp_info, &my_qp_info, sizeof(struct shuffle_qp_info));

        len = write(master_fd, &msg_gat, sizeof(struct msg_gather));
        logassert(len != sizeof(struct msg_gather), "Wrong write length");

        net_ep_infos.resize(n_ep);
        len = read(master_fd, &socket_buf, sizeof(struct msg_scatter));
        logassert(len != sizeof(struct msg_scatter), "Wrong read length");
        struct msg_scatter *msg_sca = (struct msg_scatter *)socket_buf;
        logassert(msg_sca->type!= MSG_TYPE_SCATTER, "Wrong message type");
        memcpy(&vir_ep_info, &msg_sca->vir_ep_info, sizeof(struct endpoint_info));
        memcpy(&net_ep_infos[0], msg_sca->ep_infos, sizeof(struct endpoint_info) * n_ep);
        memcpy(&dst_qp_info, &msg_sca->dqp_info, sizeof(struct shuffle_qp_info));

        loginfo("Received destination endpoint addresses");
    }

    void move_qp_to_rts(struct ibv_qp *qp, 
                        struct endpoint_info *dst_ep_info, 
                        int dqpn,
                        enum ibv_mtu mtu,
                        int static_rate,
                        int timeout,
                        int retry_cnt,
                        int min_rnr_timer,
                        int rnr_retry) {
        int ret;
        // INIT -> RTR
        struct ibv_qp_attr attr = {
            .qp_state       = IBV_QPS_RTR,
            .path_mtu       = mtu,
            .rq_psn         = (uint32_t) dst_ep_info->psn,
            .dest_qp_num    = (uint32_t) dqpn,
            .ah_attr        = {
                .dlid       = (uint16_t) dst_ep_info->lid,
                .sl         = 0,
                .src_path_bits  = 0,
                .static_rate = (uint8_t)static_rate,
                .is_global  = 0,
                .port_num   = (uint8_t) ib_port
            },
            .max_dest_rd_atomic = 16,
            .min_rnr_timer      = (uint8_t)min_rnr_timer,
        };

        // attr.ah_attr.static_rate = IBV_RATE_2_5_GBPS;
        // loginfo("Use 2.5Gbps link");

        // grh must be set in RoCE
        if(((ibv_gid *)(dst_ep_info->gid))->global.interface_id) {
            attr.ah_attr.is_global = 1;
            attr.ah_attr.grh.hop_limit = 1;
            memcpy(&attr.ah_attr.grh.dgid, dst_ep_info->gid, sizeof(ibv_gid));
            attr.ah_attr.grh.sgid_index = gid_index;
        }

        ret = ibv_modify_qp(qp, &attr,
                IBV_QP_STATE              |
                IBV_QP_AV                 |
                IBV_QP_PATH_MTU           |
                IBV_QP_DEST_QPN           |
                IBV_QP_RQ_PSN             |
                IBV_QP_MAX_DEST_RD_ATOMIC |
                IBV_QP_MIN_RNR_TIMER);

        logassert(ret != 0, "Cannot modify QP state to RTR");
        // RTR -> RTS

        attr.qp_state       = IBV_QPS_RTS;
        // 18 will make the process really slow
        attr.timeout        = timeout;// 4=65us, 8=1ms, 14=67ms, 18=1s, 31=8800s, 0=INF
        attr.retry_cnt      = retry_cnt;
        attr.rnr_retry      = rnr_retry;// 7 means retry infinitely when RNR NACK is received
        attr.sq_psn         = my_ep_info.psn;
        attr.max_rd_atomic  = 1;

        ret = ibv_modify_qp(qp, &attr, 
              IBV_QP_STATE              |
              IBV_QP_TIMEOUT            |
              IBV_QP_RETRY_CNT          |
              IBV_QP_RNR_RETRY          |
              IBV_QP_SQ_PSN             |
              IBV_QP_MAX_QP_RD_ATOMIC);
        logassert(ret != 0, "Cannot modify QP state to RTS");
    }

    void set_cpu_affinity() {
        vector<int> cpu_list = get_cpu_list_by_socket(socket_id);
        logassert(cpu_list.empty(), "CPU list empty");
        cpu_set_t mask;
        CPU_ZERO(&mask);
        for (int i = 0; i < n_core; i ++) {
            CPU_SET(cpu_list[i], &mask);
        }
        logassert(sched_setaffinity(0, sizeof(mask), &mask) == -1, "Cannot set CPU affinity");
    }

    void initialize() {

        dev_name = get_dev_by_ip(bind_ip);
        ib_dev_name = dev_to_ib_dev(dev_name);
        socket_id = get_socket_by_pci(get_pci_by_dev(dev_name));

        set_cpu_affinity();
        
        // device
        ib_dev = find_ib_device(ib_dev_name);
        logassert(ib_dev == NULL, "IB device", ib_dev_name, " not found");
        loginfo("Found device ", ib_dev_name);

        context = ibv_open_device(ib_dev);
        logassert(context == NULL, "Cannot open ib device", ib_dev_name);
        loginfo("Openned device ", ib_dev_name);

        // channel = ibv_create_comp_channel(context);
        // logassert(channel == NULL, "Cannot create completion channel");

        // PD
        pd = ibv_alloc_pd(context);
        logassert(pd == NULL, "Cannot allocate pd");
        loginfo("Allocated pd");

        // MR
        mr_mmap = huge_malloc(mr_size);
        mr = ibv_reg_mr(pd, mr_mmap, mr_size, IBV_ACCESS_LOCAL_WRITE | 
                                          IBV_ACCESS_REMOTE_READ |
                                          IBV_ACCESS_REMOTE_WRITE);
        logassert(mr == NULL, "Cannot register memory region");
        loginfo("Registered memory region with size ", mr_size);

        struct ibv_port_attr port_attr;
        ibv_query_port(context, ib_port, &port_attr);
        my_ep_info.rank = 0;
        my_ep_info.lid = port_attr.lid;
        my_ep_info.psn = psn;
        my_ep_info.addr = mr_mmap;
        my_ep_info.rkey = mr->rkey;
        ibv_query_gid(context, ib_port, gid_index, (ibv_gid *)my_ep_info.gid);
        
        loginfo("GID: ", gid_to_str((ibv_gid *)(my_ep_info.gid)));

        // exchange addresses and create QP
        endpoint_exchange_address();

        for (int i = 0; i < n_ep; i ++) {
            move_qp_to_rts(nor_qps[i], 
                           &net_ep_infos[i],
                           dst_qp_info.nor_qpn[i],
                           mtu,
                           IBV_RATE_100_GBPS,
                           8, // 1 ms
                           3,
                           12, // 0.64ms
                           3);
        }
        
        for (int i = 0; i < n_ep; i ++) {
            move_qp_to_rts(src_qps[i], 
                           &vir_ep_info,
                           dst_qp_info.src_qpn[i],
                           mtu,
                           IBV_RATE_100_GBPS,
                           8,
                           3,
                           12,
                           3);
        }

        move_qp_to_rts(dst_qp, 
                        &vir_ep_info,
                        dst_qp_info.dst_qpn,
                        mtu,
                        IBV_RATE_100_GBPS,
                        8,
                        3,
                        12,
                        3);
        
        move_qp_to_rts(req_qp, 
                        &vir_ep_info,
                        dst_qp_info.req_qpn,
                        mtu,
                        IBV_RATE_2_5_GBPS,
                        15, // 67ms
                        0,  // DEBUG! do not retry  
                        27, // 122ms
                        4);
        loginfo("Moved QPs to RTS");
    }

    int post_read(int ep_id, uint64_t wr_id, int thread_id, void *dst_addr, void *src_addr, uint32_t length) {
        int ret;
        struct ibv_sge list = {
            .addr   = (uintptr_t) src_addr,
            .length = length,
            .lkey   = mr->lkey,
        };
        struct ibv_send_wr wr = {
            .wr_id      = wr_id * n_core + thread_id,
            .next       = NULL,
            .sg_list    = &list,
            .num_sge    = 1,
            .opcode     = IBV_WR_RDMA_READ,
            .send_flags = IBV_SEND_SIGNALED,
        };
        
        wr.wr.rdma.remote_addr = (uintptr_t) dst_addr;
        wr.wr.rdma.rkey = net_ep_infos[ep_id].rkey;
        struct ibv_send_wr *bad_wr;
        ret = ibv_post_send(nor_qps[ep_id], &wr, &bad_wr);
        // logassert(ret != 0, "Cannot post read");
        if (ret != 0) {
            return -1;
        }
        return 0;
    }

    int post_many_read(int n, int ep_id, void *dst_addr, void *src_addr, uint32_t length) {
        int ret;
        auto post_start = std::chrono::high_resolution_clock::now();
        struct ibv_sge list = {
            .addr   = (uintptr_t) src_addr,
            .length = length,
            .lkey   = mr->lkey,
        };
        struct ibv_send_wr *wrs = new struct ibv_send_wr[n];
        for (int i = 0; i < n; i ++) {
            wrs[i].wr_id      = (uint64_t)i;
            wrs[i].next       = &wrs[i+1];
            wrs[i].sg_list    = &list;
            wrs[i].num_sge    = 1;
            wrs[i].opcode     = IBV_WR_RDMA_READ;
            wrs[i].send_flags = 0;
            wrs[i].wr.rdma.remote_addr = (uintptr_t) dst_addr;
            wrs[i].wr.rdma.rkey = net_ep_infos[ep_id].rkey;
        }
        
        struct ibv_send_wr *bad_wr = NULL;
        ret = ibv_post_send(nor_qps[ep_id], &wrs[0], &bad_wr);
            
        auto post_end = std::chrono::high_resolution_clock::now();
        auto post_time = std::chrono::duration_cast<std::chrono::microseconds>(post_end - post_start).count();
        loginfo("Post ", n, " reads in ", post_time, " us");
        // logassert(ret != 0, "Cannot post read");
        if (ret != 0) {
            return -1;
        }
        return 0;
    }


    int poll_cq(int ep_id, int thread_id, uint64_t &cqe) {
        int ret;
        ret = nor_cq_perthread[thread_id]->pop(cqe);
        if (ret == 0) {
            return 0;
        }

        struct ibv_wc wcs[q_size];
        ret = ibv_poll_cq(nor_cqs[ep_id], q_size, wcs);
        logassert(ret < 0, "Error on ibv_poll_cq");
        // logdebug("Polled ", ret, " CQEs");
        for(int i = 0; i < ret; i++) {
            logassert(wcs[i].status != IBV_WC_SUCCESS, "WC ", wcs[i].wr_id, " has wrong status : ", ibv_wc_status_str(wcs[i].status), "(", wcs[i].status, ")");
            int wr_thread_id = wcs[i].wr_id % n_core;
            ret = nor_cq_perthread[wr_thread_id]->push(wcs[i].wr_id / n_core);
            logassert(ret != 0, "Cannot push WR to Thread ", wr_thread_id, "'s CQ");
        }
        
        ret = nor_cq_perthread[thread_id]->pop(cqe);
        return ret;
    }

    int post_shuffle_request(uint64_t wr_id, int thread_id, void *req_addr, void *dst_addr, int n) {
        int ret;
        struct ibv_sge list = {
            .addr   = (uintptr_t) req_addr,
            .length = n * (uint32_t)sizeof(struct shuffle_request),
            .lkey   = mr->lkey,
        };
        struct ibv_send_wr wr = {
            .wr_id      = wr_id * n_core + thread_id,
            .sg_list    = &list,
            .num_sge    = 1,
            .opcode     = IBV_WR_RDMA_WRITE,
            .send_flags = IBV_SEND_SIGNALED,
        };
        
        wr.wr.rdma.remote_addr = (uint64_t)dst_addr;
        wr.wr.rdma.rkey = my_ep_info.rkey;
        struct ibv_send_wr *bad_wr;
        ret = ibv_post_send(req_qp, &wr, &bad_wr);
        // logassert(ret != 0, "Cannot post read");
        if (ret != 0) {
            return -1;
        }
        return 0;
    }

    int poll_shuffle_request_cq(int thread_id, uint64_t &cqe) {
        int ret;
        ret = req_cq_perthread[thread_id]->pop(cqe);
        if (ret == 0) {
            return 0;
        }

        struct ibv_wc wcs[q_size];
        ret = ibv_poll_cq(req_cq, q_size, wcs);
        logassert(ret < 0, "Error on ibv_poll_cq");
        // logdebug("Polled ", ret, " CQEs");
        for(int i = 0; i < ret; i++) {
            logassert(wcs[i].status != IBV_WC_SUCCESS, "WC ", wcs[i].wr_id, " has wrong status : ", ibv_wc_status_str(wcs[i].status), "(", wcs[i].status, ")");
            int wr_thread_id = wcs[i].wr_id % n_core;
            ret = req_cq_perthread[wr_thread_id]->push(wcs[i].wr_id / n_core);
            logassert(ret != 0, "Cannot push WR to Thread ", wr_thread_id, "'s CQ");
        }
        
        ret = req_cq_perthread[thread_id]->pop(cqe);
        return ret;
    }

    void finish() {
        struct msg_finish msg_fin;
        msg_fin.type = MSG_TYPE_FINISH;
        ssize_t len;
        len = write(master_fd, &msg_fin, sizeof(struct msg_finish));
        logassert(len != sizeof(struct msg_finish), "Wrong write length");
        loginfo("Sent finish message");

        close(master_fd);
    }
};
