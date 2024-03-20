#pragma once

#include "vswitchd.hpp"

#include "../common/config.hpp"
#include "../common/types.h"
#include "../common/utils.h"

#include <string>
#include <vector>

using namespace std;

const char vir_mac[] = "00:02:00:00:03:00";
const char vir_ip[] = "192.168.1.100";
const uint16_t vir_udp_port = 17787;
const struct shuffle_qp_info vir_qp_info = {
    .req_qpn = 0x93589,
    .dst_qpn = 0xd13cb,
    .src_qpn = {0x810970, 0x810971, 0x810972, 0x810973, 0x810974, 0x810975, 0x810976, 0x810977},
    .nor_qpn = {0}
};

static const size_t batch_size = 16;

class ShuffleDrv : public VSwitchd {
public:
    int n_ep;
    int *fp_rank;
    struct endpoint_info *ep_infos;
    struct shuffle_qp_info *qp_infos;

    ShuffleDrv(Config &cfg,
               int _n_ep,
               int *_fp_rank,
               struct endpoint_info *_ep_infos,
               struct shuffle_qp_info *_qp_infos)
        : VSwitchd(cfg),
          n_ep(_n_ep),
          fp_rank(_fp_rank),
          ep_infos(_ep_infos),
          qp_infos(_qp_infos)
    {
    }

    /* init l2_route table */
    int init_l2_route() {
        bf_status_t status;
        const bfrt::BfRtTable *tbl;
        /* ALl field ids */
        bf_rt_id_t k_dst_mac;
        bf_rt_id_t aid;
        bf_rt_id_t d_port;

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;

        status = bfrtInfo->bfrtTableFromNameGet("ShuffleIngress.l2_route", &tbl);
        ASSERT_STATUS("Cannot get bfrt table");
        status = tbl->keyFieldIdGet("hdr.eth.dst_addr", &k_dst_mac);
        status = tbl->actionIdGet("ShuffleIngress.l2_forward", &aid);
        status = tbl->dataFieldIdGet("port", aid, &d_port);

        status = tbl->keyAllocate(&key);
        status = tbl->dataAllocate(aid, &data);

        uint8_t mac_val[6];

        for (size_t i = 0; i < sizeof(macs) / sizeof(macs[0]); i ++) {
            str2mac(macs[i], mac_val);
            status = key->setValue(k_dst_mac, mac_val, 6);
            status = data->setValue(d_port, ports[i]);
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
        }
        status = sess->sessionCompleteOperations();
        loginfo("Successfully initialized l2_route table.");
        return 0;
    }

    int init_tx_loopback_tbl() {
        bf_status_t status;
        const bfrt::BfRtTable *tbl;
        /* ALl field ids */
        bf_rt_id_t k_src_mac;
        bf_rt_id_t aid;
        bf_rt_id_t d_port;

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;

        status = bfrtInfo->bfrtTableFromNameGet("ShuffleIngress.tx_loopback_tbl", &tbl);
        ASSERT_STATUS("Cannot get bfrt table");
        status = tbl->keyFieldIdGet("hdr.eth.src_addr", &k_src_mac);
        status = tbl->actionIdGet("ShuffleIngress.tx_loopback_action", &aid);
        status = tbl->dataFieldIdGet("port", aid, &d_port);

        status = tbl->keyAllocate(&key);
        status = tbl->dataAllocate(aid, &data);

        uint8_t mac_val[6];

        for (size_t i = 0; i < sizeof(macs) / sizeof(macs[0]) - 1; i ++) {
            str2mac(macs[i], mac_val);
            status = key->setValue(k_src_mac, mac_val, 6);
            status = data->setValue(d_port, lpbk_ports[i]);
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
        }
        status = sess->sessionCompleteOperations();
        loginfo("Successfully initialized tx_loopback_tbl table.");
        return 0;
    }

    int init_tx_req_ack_tbl() {
        bf_status_t status;
        const bfrt::BfRtTable *tbl;

        bf_rt_id_t k_dst_id;
        bf_rt_id_t aid;
        bf_rt_id_t d_port;
        bf_rt_id_t d_src_mac;
        bf_rt_id_t d_dst_mac;
        bf_rt_id_t d_src_ip;
        bf_rt_id_t d_dst_ip;
        bf_rt_id_t d_udp_src_port;
        bf_rt_id_t d_dqpn;
        bf_rt_id_t d_rkey;

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;

        uint8_t mac_val[6];

        status = bfrtInfo->bfrtTableFromNameGet("ShuffleIngress.tx_req_ack_tbl", &tbl);
        ASSERT_STATUS("Cannot get bfrt table");
        
        status = tbl->keyFieldIdGet("dst_id", &k_dst_id);
        status = tbl->actionIdGet("ShuffleIngress.tx_req_ack_action", &aid);
        status = tbl->dataFieldIdGet("port", aid, &d_port);
        status = tbl->dataFieldIdGet("src_mac", aid, &d_src_mac);
        status = tbl->dataFieldIdGet("dst_mac", aid, &d_dst_mac);
        status = tbl->dataFieldIdGet("src_ip", aid, &d_src_ip);
        status = tbl->dataFieldIdGet("dst_ip", aid, &d_dst_ip);
        status = tbl->dataFieldIdGet("udp_src_port", aid, &d_udp_src_port);
        status = tbl->dataFieldIdGet("dqpn", aid, &d_dqpn);
        status = tbl->dataFieldIdGet("rkey", aid, &d_rkey);

        status = tbl->keyAllocate(&key);
        status = tbl->dataAllocate(aid, &data);

        for (int i = 0; i < n_ep; i ++) {
            // dst_id = i
            status = key->setValue(k_dst_id, i);
            status = data->setValue(d_port, ports[fp_rank[i]]);
            
            str2mac(vir_mac, mac_val);
            status = data->setValue(d_src_mac, mac_val, 6);
            str2mac(macs[fp_rank[i]], mac_val);
            status = data->setValue(d_dst_mac, mac_val, 6);
            status = data->setValue(d_src_ip, (uint64_t)str2ip(vir_ip));
            status = data->setValue(d_dst_ip, (uint8_t *)(ep_infos[i].gid) + 12, 4);
            status = data->setValue(d_udp_src_port, (uint64_t)vir_udp_port);
            status = data->setValue(d_dqpn, (uint64_t)qp_infos[i].req_qpn);
            status = data->setValue(d_rkey, (uint64_t)ep_infos[i].rkey);
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
        }

        status = sess->sessionCompleteOperations();
        loginfo("Successfully initialized tx_req_ack_tbl");
        return 0;
    }

    int init_tx_dst_write_tbl() {
        bf_status_t status;
        const bfrt::BfRtTable *tbl;

        bf_rt_id_t k_dst_id;
        bf_rt_id_t aid;
        bf_rt_id_t d_port;
        bf_rt_id_t d_src_mac;
        bf_rt_id_t d_dst_mac;
        bf_rt_id_t d_src_ip;
        bf_rt_id_t d_dst_ip;
        bf_rt_id_t d_udp_src_port;
        bf_rt_id_t d_dqpn;
        bf_rt_id_t d_rkey;

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;

        uint8_t mac_val[6];

        status = bfrtInfo->bfrtTableFromNameGet("ShuffleIngress.tx_dst_write_tbl", &tbl);
        ASSERT_STATUS("Cannot get bfrt table");
        
        status = tbl->keyFieldIdGet("dst_id", &k_dst_id);
        status = tbl->actionIdGet("ShuffleIngress.tx_dst_write_action", &aid);
        status = tbl->dataFieldIdGet("port", aid, &d_port);
        status = tbl->dataFieldIdGet("src_mac", aid, &d_src_mac);
        status = tbl->dataFieldIdGet("dst_mac", aid, &d_dst_mac);
        status = tbl->dataFieldIdGet("src_ip", aid, &d_src_ip);
        status = tbl->dataFieldIdGet("dst_ip", aid, &d_dst_ip);
        status = tbl->dataFieldIdGet("udp_src_port", aid, &d_udp_src_port);
        status = tbl->dataFieldIdGet("dqpn", aid, &d_dqpn);
        status = tbl->dataFieldIdGet("rkey", aid, &d_rkey);

        status = tbl->keyAllocate(&key);
        status = tbl->dataAllocate(aid, &data);

        for (int i = 0; i < n_ep; i ++) {
            // dst_id = i
            status = key->setValue(k_dst_id, i);
            status = data->setValue(d_port, ports[fp_rank[i]]);
            
            str2mac(vir_mac, mac_val);
            status = data->setValue(d_src_mac, mac_val, 6);
            str2mac(macs[fp_rank[i]], mac_val);
            status = data->setValue(d_dst_mac, mac_val, 6);
            status = data->setValue(d_src_ip, (uint64_t)str2ip(vir_ip));
            status = data->setValue(d_dst_ip, (uint8_t *)(ep_infos[i].gid) + 12, 4);
            status = data->setValue(d_udp_src_port, (uint64_t)vir_udp_port);
            status = data->setValue(d_dqpn, (uint64_t)qp_infos[i].dst_qpn);
            status = data->setValue(d_rkey, (uint64_t)ep_infos[i].rkey);
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
        }

        status = sess->sessionCompleteOperations();
        loginfo("Successfully initialized tx_dst_write_tbl");
        return 0;
    }

    int init_tx_repl_many_tbl() {
        bf_status_t status;
        const bfrt::BfRtTable *tbl;

        bf_rt_id_t k_src_mac;
        bf_rt_id_t aid;
        bf_rt_id_t d_port;
        bf_rt_id_t d_sess;

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;

        uint8_t mac_val[6];

        status = bfrtInfo->bfrtTableFromNameGet("ShuffleIngress.tx_repl_many_tbl", &tbl);
        ASSERT_STATUS("Cannot get bfrt table");
        
        status = tbl->keyFieldIdGet("hdr.eth.src_addr", &k_src_mac);
        status = tbl->actionIdGet("ShuffleIngress.tx_repl_many_action", &aid);
        status = tbl->dataFieldIdGet("port", aid, &d_port);
        status = tbl->dataFieldIdGet("sess", aid, &d_sess);

        status = tbl->keyAllocate(&key);
        status = tbl->dataAllocate(aid, &data);

        for (int i = 0; i < n_ep; i ++) {
            // dst_id = i
            str2mac(macs[fp_rank[i]], mac_val);
            status = data->setValue(k_src_mac, mac_val, 6);

            status = data->setValue(d_port, lpbk_ports[fp_rank[i]]);
            status = data->setValue(d_sess, mir_sess[fp_rank[i]]);
            
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
        }

        status = sess->sessionCompleteOperations();
        loginfo("Successfully initialized tx_repl_many_tbl");
        return 0;
    }
    
    int init_tx_repl_only_tbl() {
        bf_status_t status;
        const bfrt::BfRtTable *tbl;

        bf_rt_id_t k_src_mac;
        bf_rt_id_t aid;
        bf_rt_id_t d_port;

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;

        uint8_t mac_val[6];

        status = bfrtInfo->bfrtTableFromNameGet("ShuffleIngress.tx_repl_only_tbl", &tbl);
        ASSERT_STATUS("Cannot get bfrt table");
        
        status = tbl->keyFieldIdGet("hdr.eth.src_addr", &k_src_mac);
        status = tbl->actionIdGet("ShuffleIngress.tx_repl_only_action", &aid);
        status = tbl->dataFieldIdGet("port", aid, &d_port);

        status = tbl->keyAllocate(&key);
        status = tbl->dataAllocate(aid, &data);

        for (int i = 0; i < n_ep; i ++) {
            // dst_id = i
            str2mac(macs[fp_rank[i]], mac_val);
            status = data->setValue(k_src_mac, mac_val, 6);

            status = data->setValue(d_port, lpbk_ports[fp_rank[i]]);
            
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
        }

        status = sess->sessionCompleteOperations();
        loginfo("Successfully initialized tx_repl_only_tbl");
        return 0;
    }


    int init_tx_src_read_tbl() {
        bf_status_t status;
        const bfrt::BfRtTable *tbl;

        bf_rt_id_t k_src_id;
        bf_rt_id_t k_dst_id;
        bf_rt_id_t aid;
        bf_rt_id_t d_port;
        bf_rt_id_t d_src_mac;
        bf_rt_id_t d_dst_mac;
        bf_rt_id_t d_src_ip;
        bf_rt_id_t d_dst_ip;
        bf_rt_id_t d_udp_src_port;
        bf_rt_id_t d_dqpn;
        bf_rt_id_t d_rkey;

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;

        uint8_t mac_val[6];

        status = bfrtInfo->bfrtTableFromNameGet("ShuffleIngress.tx_src_read_tbl", &tbl);
        ASSERT_STATUS("Cannot get bfrt table");
        
        status = tbl->keyFieldIdGet("hdr.item0.src_id", &k_src_id);
        status = tbl->keyFieldIdGet("dst_id", &k_dst_id);
        status = tbl->actionIdGet("ShuffleIngress.tx_src_read_action", &aid);
        status = tbl->dataFieldIdGet("port", aid, &d_port);
        status = tbl->dataFieldIdGet("src_mac", aid, &d_src_mac);
        status = tbl->dataFieldIdGet("dst_mac", aid, &d_dst_mac);
        status = tbl->dataFieldIdGet("src_ip", aid, &d_src_ip);
        status = tbl->dataFieldIdGet("dst_ip", aid, &d_dst_ip);
        status = tbl->dataFieldIdGet("udp_src_port", aid, &d_udp_src_port);
        status = tbl->dataFieldIdGet("dqpn", aid, &d_dqpn);
        status = tbl->dataFieldIdGet("rkey", aid, &d_rkey);

        status = tbl->keyAllocate(&key);
        status = tbl->dataAllocate(aid, &data);

        for (int i = 0; i < n_ep; i ++) {
            // src_id = i
            for (int j = 0; j < n_ep; j ++) {
                // dst_id = j
                status = key->setValue(k_src_id, i);
                status = key->setValue(k_dst_id, j);
                status = data->setValue(d_port, ports[fp_rank[i]]);
                
                str2mac(vir_mac, mac_val);
                status = data->setValue(d_src_mac, mac_val, 6);
                str2mac(macs[fp_rank[i]], mac_val);
                status = data->setValue(d_dst_mac, mac_val, 6);
                status = data->setValue(d_src_ip, (uint64_t)str2ip(vir_ip));
                status = data->setValue(d_dst_ip, (uint8_t *)ep_infos[i].gid + 12, 4);
                status = data->setValue(d_udp_src_port, (uint64_t)vir_udp_port);
                status = data->setValue(d_dqpn, (uint64_t)qp_infos[i].src_qpn[j]);
                status = data->setValue(d_rkey, (uint64_t)ep_infos[i].rkey);
                status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
            }
        }

        status = sess->sessionCompleteOperations();
        loginfo("Successfully initialized tx_src_read_tbl");
        return 0;
    }


    int init_rx_repl_only_tbl() {
        bf_status_t status;
        const bfrt::BfRtTable *tbl;

        bf_rt_id_t k_src_mac;
        bf_rt_id_t k_src_id;
        bf_rt_id_t aid;
        bf_rt_id_t d_dst_id;
        bf_rt_id_t d_src_dst_id;

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;

        uint8_t mac_val[6];

        status = bfrtInfo->bfrtTableFromNameGet("ShuffleIngress.rx_repl_only_tbl", &tbl);
        ASSERT_STATUS("Cannot get bfrt table");
        
        status = tbl->keyFieldIdGet("hdr.eth.src_addr", &k_src_mac);
        status = tbl->keyFieldIdGet("hdr.item0.src_id", &k_src_id);
        status = tbl->actionIdGet("ShuffleIngress.rx_repl_only_action", &aid);
        status = tbl->dataFieldIdGet("_dst_id", aid, &d_dst_id);
        status = tbl->dataFieldIdGet("_src_dst_id", aid, &d_src_dst_id);

        status = tbl->keyAllocate(&key);
        status = tbl->dataAllocate(aid, &data);

        for (int src_id = 0; src_id < n_ep; src_id ++) {
            for (int dst_id = 0; dst_id < n_ep; dst_id ++) {
                str2mac(macs[fp_rank[dst_id]], mac_val);
                status = key->setValue(k_src_mac, mac_val, 6);
                status = key->setValue(k_src_id, src_id);

                status = data->setValue(d_dst_id, (uint64_t)dst_id);
                status = data->setValue(d_src_dst_id, (uint64_t)src_id * n_ep + dst_id);
                
                status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
            }
        }

        status = sess->sessionCompleteOperations();
        loginfo("Successfully initialized rx_repl_only_tbl");
        return 0;
    }

    int init_roce_method_tbl() {
        
        bf_status_t status;
        const bfrt::BfRtTable *tbl;

        bf_rt_id_t k_src_mac;
        bf_rt_id_t k_opcode;
        bf_rt_id_t k_dqpn;
        bf_rt_id_t aid;
        bf_rt_id_t d_dst_id;
        bf_rt_id_t d_src_dst_id;

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;

        uint8_t mac_val[6];

        status = bfrtInfo->bfrtTableFromNameGet("ShuffleIngress.rx_repl_only_tbl", &tbl);
        ASSERT_STATUS("Cannot get bfrt table");
        
        status = tbl->keyFieldIdGet("hdr.eth.src_addr", &k_src_mac);
        status = tbl->keyFieldIdGet("hdr.bth.opcode", &k_opcode);
        status = tbl->keyFieldIdGet("hdr.bth.dqpn", &k_dqpn);
        status = tbl->keyAllocate(&key);

        // request first
        status = tbl->actionIdGet("ShuffleIngress.rx_request_first", &aid);
        status = tbl->dataFieldIdGet("_dst_id", aid, &d_dst_id);
        status = tbl->dataAllocate(aid, &data);

        for (int i = 0; i < n_ep; i ++) {
            str2mac(macs[fp_rank[i]], mac_val);
            status = key->setValue(k_src_mac, mac_val, 6);
            status = key->setValue(k_opcode, RDMA_OP_WRITE_FIRST);
            status = key->setValue(k_dqpn, vir_qp_info.req_qpn);

            status = data->setValue(d_dst_id, (uint64_t)i);
            
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
        }
        
        // request middle
        status = tbl->actionIdGet("ShuffleIngress.rx_request_middle", &aid);
        status = tbl->dataFieldIdGet("_dst_id", aid, &d_dst_id);
        status = tbl->dataAllocate(aid, &data);

        for (int i = 0; i < n_ep; i ++) {
            str2mac(macs[fp_rank[i]], mac_val);
            status = key->setValue(k_src_mac, mac_val, 6);
            status = key->setValue(k_opcode, RDMA_OP_WRITE_MIDDLE);
            status = key->setValue(k_dqpn, vir_qp_info.req_qpn);

            status = data->setValue(d_dst_id, (uint64_t)i);
            
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
        }

        // request last
        status = tbl->actionIdGet("ShuffleIngress.rx_request_last", &aid);
        status = tbl->dataFieldIdGet("_dst_id", aid, &d_dst_id);
        status = tbl->dataAllocate(aid, &data);

        for (int i = 0; i < n_ep; i ++) {
            str2mac(macs[fp_rank[i]], mac_val);
            status = key->setValue(k_src_mac, mac_val, 6);
            status = key->setValue(k_opcode, RDMA_OP_WRITE_LAST);
            status = key->setValue(k_dqpn, vir_qp_info.req_qpn);

            status = data->setValue(d_dst_id, (uint64_t)i);
            
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
        }

        // request only
        status = tbl->actionIdGet("ShuffleIngress.rx_request_only", &aid);
        status = tbl->dataFieldIdGet("_dst_id", aid, &d_dst_id);
        status = tbl->dataAllocate(aid, &data);

        for (int i = 0; i < n_ep; i ++) {
            str2mac(macs[fp_rank[i]], mac_val);
            status = key->setValue(k_src_mac, mac_val, 6);
            status = key->setValue(k_opcode, RDMA_OP_WRITE_ONLY);
            status = key->setValue(k_dqpn, vir_qp_info.req_qpn);

            status = data->setValue(d_dst_id, (uint64_t)i);
            
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
        }
        
        // read response
        status = tbl->actionIdGet("ShuffleIngress.rx_read_response", &aid);
        status = tbl->dataFieldIdGet("_dst_id", aid, &d_dst_id);
        status = tbl->dataFieldIdGet("_src_dst_id", aid, &d_src_dst_id);
        status = tbl->dataAllocate(aid, &data);

        for (int src_id = 0; src_id < n_ep; src_id ++) {
            for (int dst_id = 0; dst_id < n_ep; dst_id ++) {
                str2mac(macs[fp_rank[src_id]], mac_val);
                status = key->setValue(k_src_mac, mac_val, 6);
                status = key->setValue(k_opcode, RDMA_OP_READ_RES_ONLY);
                status = key->setValue(k_dqpn, vir_qp_info.src_qpn[dst_id]);

                status = data->setValue(d_dst_id, (uint64_t)dst_id);
                status = data->setValue(d_src_dst_id, (uint64_t)src_id * n_ep + dst_id);
                
                status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
            }
        }
        
        // read nak
        status = tbl->actionIdGet("ShuffleIngress.rx_read_nak", &aid);
        status = tbl->dataFieldIdGet("_dst_id", aid, &d_dst_id);
        status = tbl->dataFieldIdGet("_src_dst_id", aid, &d_src_dst_id);
        status = tbl->dataAllocate(aid, &data);

        for (int src_id = 0; src_id < n_ep; src_id ++) {
            for (int dst_id = 0; dst_id < n_ep; dst_id ++) {
                str2mac(macs[fp_rank[src_id]], mac_val);
                status = key->setValue(k_src_mac, mac_val, 6);
                status = key->setValue(k_opcode, RDMA_OP_ACK);
                status = key->setValue(k_dqpn, vir_qp_info.src_qpn[dst_id]);

                status = data->setValue(d_dst_id, (uint64_t)dst_id);
                status = data->setValue(d_src_dst_id, (uint64_t)src_id * n_ep + dst_id);
                
                status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
            }
        }

        // write ack
        status = tbl->actionIdGet("ShuffleIngress.rx_write_ack", &aid);
        status = tbl->dataFieldIdGet("_dst_id", aid, &d_dst_id);
        status = tbl->dataAllocate(aid, &data);

        for (int i = 0; i < n_ep; i ++) {
            str2mac(macs[fp_rank[i]], mac_val);
            status = key->setValue(k_src_mac, mac_val, 6);
            status = key->setValue(k_opcode, RDMA_OP_ACK);
            status = key->setValue(k_dqpn, vir_qp_info.dst_qpn);

            status = data->setValue(d_dst_id, (uint64_t)i);
            
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
        }
        status = sess->sessionCompleteOperations();
        loginfo("Successfully initialized rx_repl_only_tbl");
        return 0;
    }

    
    int init_register(string str, vector<uint32_t> vals) {
        bf_status_t status;
        const bfrt::BfRtTable *reg = nullptr;
        string table_name = str + "";
        status = bfrtInfo->bfrtTableFromNameGet(table_name,
                                                &reg);

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;
        bf_rt_id_t key_field_id;
        bf_rt_id_t data_field_id;
        status = reg->keyFieldIdGet("$REGISTER_INDEX", &key_field_id);
        string df_name = str + ".f1";
        status = reg->dataFieldIdGet(df_name, &data_field_id);
        status = reg->keyAllocate(&key);
        status = reg->dataAllocate(&data);

        size_t reg_size;
        status = reg->tableSizeGet(*sess, dev_tgt, (size_t *)&reg_size);
        logassert(reg_size >= vals.size(), "register size mismatch");

        // uint8_t data_buf[4];
        // memset(data_buf, 0, sizeof(data_buf));
        // *(uint32_t *)data_buf = htonl(val);
        // status = data->setValue(data_field_id, data_buf, 4);

        for (size_t i = 0; i < vals.size(); i += batch_size) {
            status = sess->beginBatch();
            for (size_t j = i; j < i + batch_size && j < reg_size; j ++) {
                status = key->setValue(key_field_id, j);
                status = data->setValue(data_field_id, (uint64_t)vals[j]);
                status = reg->tableEntryMod(*sess, dev_tgt, *key, *data);
            }
            status = sess->endBatch(false);
        }
        status = sess->sessionCompleteOperations();
        return 0;
    }

    int dump_reg(string str, size_t max_size=0xffffffffull) {
        bf_status_t status;
        const bfrt::BfRtTable::BfRtTableGetFlag flag = bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_HW;
        const bfrt::BfRtTable *reg = nullptr;
        string table_name = str;
        status = bfrtInfo->bfrtTableFromNameGet(table_name,
                                                &reg);

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;
        bf_rt_id_t key_field_id;
        bf_rt_id_t data_field_id;
        status = reg->keyFieldIdGet("$REGISTER_INDEX", &key_field_id);
        string df_name = str + ".f1";
        status = reg->dataFieldIdGet(df_name, &data_field_id);
        status = reg->keyAllocate(&key);
        status = reg->dataAllocate(&data);

        size_t reg_size;
        status = reg->tableSizeGet(*sess, dev_tgt, (size_t *)&reg_size);
        reg_size = min(reg_size, max_size);

        vector<uint64_t> data_buf;

        for (size_t i = 0; i < reg_size; i ++) {
            status = key->setValue(key_field_id, i);
            status = reg->tableEntryGet(*sess, dev_tgt, *key, flag, data.get());
        }

        status = sess->sessionCompleteOperations();

        status = data->getValue(data_field_id, &data_buf);
        ASSERT_STATUS("Cannot get value from data");

        logger << str << "   ";
        for (size_t i = 0; i < reg_size; i ++) {
            if (data_buf[2*i+1] != 0) {
                logger << i << ":" << data_buf[2*i+1] << ", ";
            }
        }
        logger << "\n";
        return 0;
    }

    void clear_tables() {
        clear_table("ShuffleIngress.tx_req_ack_tbl");
        clear_table("ShuffleIngress.tx_dst_write_tbl");
        clear_table("ShuffleIngress.tx_repl_many_tbl");
        clear_table("ShuffleIngress.tx_repl_only_tbl");
        clear_table("ShuffleIngress.tx_src_read_tbl");
        clear_table("ShuffleIngress.rx_repl_only_tbl");
        clear_table("ShuffleIngress.roce_method_tbl");
    }

    void init_tables() {
        init_tx_req_ack_tbl();
        init_tx_dst_write_tbl();
        init_tx_repl_many_tbl();
        init_tx_repl_only_tbl();
        init_tx_src_read_tbl();
        init_rx_repl_only_tbl();
        init_roce_method_tbl();
    }

    void init_registers() {
        vector<uint32_t> endp_state(n_ep, 1);
        init_register("SwitchIngress.endp_state", endp_state);

        vector<uint32_t> dst_vals(n_ep, 0);
        init_register("SwitchIngress.write_psn", dst_vals);
        init_register("SwitchIngress.req_unack_unit", dst_vals);
        init_register("SwitchIngress.req_epsn", dst_vals);
        init_register("SwitchIngress.req_msn", dst_vals);

        vector<uint32_t> src_dst_vals(n_ep * n_ep, 0);
        init_register("SwitchIngress.read_unack_psn", src_dst_vals);
        init_register("SwitchIngress.read_psn", src_dst_vals);
    }

    void start() override {
        initialize();
        init_l2_route();
        init_tx_loopback_tbl();

        return ;
    }

};
