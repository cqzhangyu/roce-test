#pragma once

#include "../common/config.hpp"
#include "../common/huge_malloc.h"
#include "../common/logger.hpp"
#include "../common/ring_buffer.hpp"

#include "../rdma/rdma_types.h"
#include "../rdma/rdma_utils.hpp"

class RDMAEndpoint {
public:
    struct ibv_device   *ib_dev;
    struct ibv_context  *context;
    struct ibv_comp_channel *channel;
    struct ibv_pd   *pd;
    struct ibv_mr   *mr;
    vector<struct ibv_cq *> cqs;
    vector<struct ibv_wc *> wcs;
    vector<struct ibv_qp *> qps;
    struct endpoint_addr    my_addr;
    vector<struct endpoint_addr> dst_addrs;
    void            *mr_mmap;
    size_t          mr_size;
    int             ib_port;
    int             gid_index;
    enum ibv_mtu    mtu;
    int             n_dst;
    int             n_core;
    int             q_size;
    uint32_t        master_ip;
    uint16_t        master_port;
    int             psn;
    int             pending;

    string bind_ip;
    string dev_name;
    string ib_dev_name;
    int socket_id;

    vector<ring_buffer<uint64_t> *> cq_perthread;

    RDMAEndpoint(Config &cfg) {
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
        
    void init_qp(int qp_id, int q_size) {
        int ret;
        // CQ
        struct ibv_cq *cq = ibv_create_cq(context, q_size, NULL, /* channel */NULL, 0);
        logassert(cq == NULL, "Cannot create completion queue");

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
        logassert(qp == NULL, "Cannot create qp ", qp_id);

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
        logassert(ret != 0, "Cannot set QP ", qp_id," to INIT");

        cqs[qp_id] = cq;
        qps[qp_id] = qp;
        return ;
    }

    void endpoint_exchange_address() {
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

        ret = connect(fd, (struct sockaddr *)&master_addr, sizeof(struct sockaddr));
        logassert(ret < 0, "Cannot connect to master");
        loginfo("Connected to master ", sin_to_str(&master_addr));

        char socket_buf[MAX_SOCKET_BUFSIZE];

        /*
            int n_dst;
            int rank;
        */
        ret = read(fd, socket_buf, sizeof(int) * 2);
        logassert(ret < (int)sizeof(int) * 2, "Partial read()");
        n_dst = *(int *)socket_buf;
        my_addr.rank = *(int *)(socket_buf + sizeof(int));

        // init QP
        qps.resize(n_dst);
        cqs.resize(n_dst);
        wcs.resize(n_core);
        cq_perthread.resize(n_core);

        for (int i = 0; i < n_dst; i ++) {
            init_qp(i, q_size);
            logassert(qps[i] == NULL, "Cannot create QP");
        }

        for (int i = 0; i < n_core; i ++) {
            wcs[i] = new struct ibv_wc[q_size];
            cq_perthread[i] = new ring_buffer<uint64_t>(q_size);
        }
        loginfo("Created QP");

        /*
            struct endpoint_addr my_addr;
            int qpns[n_dst];
        */
        memcpy(socket_buf, &my_addr, sizeof(struct endpoint_addr));
        for (int i = 0; i < n_dst; i ++) {
            *(int *)(socket_buf + sizeof(struct endpoint_addr) + sizeof(int) * i) = qps[i]->qp_num;
            // logdebug("my qp for dst ", i, " is ", qps[i]->qp_num);
        }
        int msg_len = sizeof(struct endpoint_addr) + sizeof(int) * n_dst;
        ret = write(fd, socket_buf, msg_len);
        logassert(ret < msg_len, "Partial write()");

        /*
            struct endpoint_addr dst_addrs[n_dst];
        */
        dst_addrs.resize(n_dst);
        msg_len = sizeof(struct endpoint_addr) * n_dst;
        ret = read(fd, &dst_addrs[0], msg_len);
        logassert(ret < msg_len, "Partial read()");
        loginfo("Received destination endpoint addresses");

        close(fd);
    }

    void move_qp_to_rts(int dst_id) {
        struct ibv_qp *qp = qps[dst_id];
        struct endpoint_addr *dst_addr = &dst_addrs[dst_id];

        // logdebug("dst ", dst_id, "'s qp_num is ", dst_addr->qpn);
        int ret;
        // INIT -> RTR
        struct ibv_qp_attr attr = {
            .qp_state       = IBV_QPS_RTR,
            .path_mtu       = mtu,
            .rq_psn         = (uint32_t) dst_addr->psn,
            .dest_qp_num    = (uint32_t) dst_addr->qpn,
            .ah_attr        = {
                .dlid       = (uint16_t) dst_addr->lid,
                .sl         = 0,
                .src_path_bits  = 0,
                // .static_rate = IBV_RATE_5_GBPS,
                .is_global  = 0,
                .port_num   = (uint8_t) ib_port
            },
            .max_dest_rd_atomic = 1,
            .min_rnr_timer      = 12,
        };

        // attr.ah_attr.static_rate = IBV_RATE_2_5_GBPS;
        // loginfo("Use 2.5Gbps link");

        // grh must be set in RoCE
        if(dst_addr->gid.global.interface_id) {
            attr.ah_attr.is_global = 1;
            attr.ah_attr.grh.hop_limit = 1;
            attr.ah_attr.grh.dgid = dst_addr->gid;
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
        attr.timeout        = 8;// 4=65us, 8=1ms, 14=67ms, 18=1s, 31=8800s, 0=INF
        attr.retry_cnt      = 7;
        attr.rnr_retry      = 7;// 7 means retry infinitely when RNR NACK is received
        attr.sq_psn         = my_addr.psn;
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

    void run() {

        dev_name = get_dev_by_ip(bind_ip);
        ib_dev_name = dev_to_ib_dev(dev_name);
        socket_id = get_socket_by_pci(get_pci_by_dev(dev_name));

        vector<int> cpu_list = get_cpu_list_by_socket(socket_id);
        logassert(cpu_list.empty(), "CPU list empty");

        cpu_set_t mask;
        CPU_ZERO(&mask);
        for (int i = 0; i < n_core; i ++) {
            CPU_SET(cpu_list[i], &mask);
        }
        logassert(sched_setaffinity(0, sizeof(mask), &mask) == -1, "Cannot set CPU affinity");

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
        my_addr.rank = 0;
        my_addr.lid = port_attr.lid;
        my_addr.psn = psn;
        my_addr.qpn = 0;
        my_addr.addr = mr_mmap;
        my_addr.rkey = mr->rkey;
        ibv_query_gid(context, ib_port, gid_index, &my_addr.gid);
        loginfo("GID: ", gid_to_str(my_addr.gid));

        // exchange addresses and create QP
        endpoint_exchange_address();

        for (int i = 0; i < n_dst; i ++) {
            move_qp_to_rts(i);
        }
        loginfo("Moved QPs to RTS");
    }

    int post_read(int dst, uint64_t wr_id, int thread_id, void *dst_addr, void *src_addr, uint32_t length) {
        int ret;
        struct ibv_sge list = {
            .addr   = (uintptr_t) src_addr,
            .length = length,
            .lkey   = mr->lkey,
        };
        struct ibv_send_wr wr = {
            .wr_id      = wr_id * n_core + thread_id,
            .sg_list    = &list,
            .num_sge    = 1,
            .opcode     = IBV_WR_RDMA_READ,
            .send_flags = IBV_SEND_SIGNALED,
        };
        
        wr.wr.rdma.remote_addr = (uintptr_t) dst_addr;
        wr.wr.rdma.rkey = dst_addrs[dst].rkey;
        struct ibv_send_wr *bad_wr;
        ret = ibv_post_send(qps[dst], &wr, &bad_wr);
        // logassert(ret != 0, "Cannot post read");
        if (ret != 0) {
            return -1;
        }
        return 0;
    }

    int poll_cq(int dst, int thread_id, uint64_t &cqe) {
        int ret;
        ret = cq_perthread[thread_id]->pop(cqe);
        if (ret == 0) {
            return 0;
        }

        ibv_wc *wc = wcs[thread_id];
        ret = ibv_poll_cq(cqs[dst], q_size, wc);
        logassert(ret < 0, "Error on ibv_poll_cq");
        // logdebug("Polled ", ret, " CQEs");
        for(int i = 0; i < ret; i++) {
            logassert(wc[i].status != IBV_WC_SUCCESS, "WC ", wc[i].wr_id, " has wrong status : ", wc[i].status);
            int wr_thread_id = wc[i].wr_id % n_core;
            ret = cq_perthread[wr_thread_id]->push(wc[i].wr_id / n_core);
            logassert(ret != 0, "Cannot push WR to Thread ", wr_thread_id, "'s CQ");
        }
        
        ret = cq_perthread[thread_id]->pop(cqe);
        return ret;
    }
};
