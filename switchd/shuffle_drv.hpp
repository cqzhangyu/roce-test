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
const uint16_t vir_udp_port = 0x457b;
const struct endpoint_info vir_ep_info = {
    .rank = -1,
    .lid = 0,
    .psn = 0,
    .rkey = 0,
    .addr = 0,
    .gid = {0,0, 0,0, 0,0, 0,0, 0,0, (char)0xff, (char)0xff, (char)192, (char)168, (char)1, (char)100}
};
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
    uint32_t *fp_rank;
    struct endpoint_info *ep_infos;
    struct shuffle_qp_info *qp_infos;
    int req_mtu;

    ShuffleDrv(Config &cfg,
               int _n_ep,
               uint32_t *_fp_rank,
               struct endpoint_info *_ep_infos,
               struct shuffle_qp_info *_qp_infos)
        : VSwitchd(cfg),
          n_ep(_n_ep),
          fp_rank(_fp_rank),
          ep_infos(_ep_infos),
          qp_infos(_qp_infos),
          req_mtu(cfg.mtu)
    {
    }

    /* init l2_route table */
    int init_l2_route() {
        bf_status_t status;
        const bfrt::BfRtTable *tbl = nullptr;
        /* ALl field ids */
        bf_rt_id_t k_dst_mac;
        bf_rt_id_t aid;
        bf_rt_id_t d_port;

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;

        status = bfrtInfo->bfrtTableFromNameGet("ShuffleIngress.l2_route", &tbl);
        if (tbl == nullptr) {
            return -1;
        }
        ASSERT_STATUS("Cannot get bfrt table");
        status = tbl->keyFieldIdGet("hdr.eth.dst_addr", &k_dst_mac);
        ASSERT_STATUS("Cannot get key field id");
        status = tbl->actionIdGet("ShuffleIngress.l2_forward", &aid);
        ASSERT_STATUS("Cannot get action id");
        status = tbl->dataFieldIdGet("port", aid, &d_port);

        uint8_t mac_val[6];

        for (size_t i = 0; i < sizeof(macs) / sizeof(macs[0]); i ++) {
            status = tbl->keyAllocate(&key);
            status = tbl->dataAllocate(aid, &data);

            str2mac(macs[i], mac_val);
            status = key->setValue(k_dst_mac, mac_val, 6);
            status = data->setValue(d_port, ports[i]);
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
        }
        status = sess->sessionCompleteOperations();
        loginfo("Successfully initialized l2_route table.");
        return 0;
    }

    int init_get_read_ring_id_tbl() {
        bf_status_t status;
        const bfrt::BfRtTable *tbl = nullptr;

        bf_rt_id_t k_src_dst_id;
        bf_rt_id_t k_read_psn_out;
        bf_rt_id_t aid;
        bf_rt_id_t d_read_ring_id;

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;

        status = bfrtInfo->bfrtTableFromNameGet("get_read_ring_id_tbl", &tbl);
        if (tbl == nullptr) {
            return -1;
        }
        ASSERT_STATUS("Cannot get bfrt table");

        string str = "read_psn_out";//[" + to_string(READ_RING_SHIFT-1) + ":0]";
        
        status = tbl->keyFieldIdGet("src_dst_id", &k_src_dst_id);
        status = tbl->keyFieldIdGet(str, &k_read_psn_out);
        status = tbl->actionIdGet("ShuffleIngress.get_read_ring_id.set_read_ring_id", &aid);
        status = tbl->dataFieldIdGet("_read_ring_id", aid, &d_read_ring_id);

        for (int src_id = 0; src_id < n_ep; src_id ++) {
            for (int dst_id = 0; dst_id < n_ep; dst_id ++) {
                for (int psn_tail = 0; psn_tail < READ_RING_SIZE; psn_tail ++) {
                    status = tbl->keyAllocate(&key);
                    status = tbl->dataAllocate(aid, &data);

                    uint64_t src_dst_id = src_id * n_ep + dst_id;
                    uint8_t read_psn_out = psn_tail;
                    uint64_t ring_id = (uint64_t)((src_id * n_ep + dst_id)<<READ_RING_SHIFT) | psn_tail;
                    status = key->setValue(k_src_dst_id, src_dst_id);
                    status = key->setValue(k_read_psn_out, read_psn_out);
                    status = data->setValue(d_read_ring_id, ring_id);
                    status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
                    // if (status != BF_SUCCESS) {
                    //     const char *err_str;
                    //     bf_rt_err_str(status, &err_str);
                    //     logerro("Cannot add table entry src_dst_id=", src_dst_id, " psn_tail=", psn_tail, " ring_id=", ring_id);
                    //     logerro("Error message: ", err_str);
                    // }
                    // else {
                    //     loginfo("Successfully added table entry src_dst_id=", src_dst_id, " psn_tail=", psn_tail, " ring_id=", ring_id);
                    // }
                }
            }
        }
        status = sess->sessionCompleteOperations();
        loginfo("Successfully initialized get_read_ring_id_tbl");
        return 0;
    }
    
    int init_get_write_ring_id_tbl() {
        bf_status_t status;
        const bfrt::BfRtTable *tbl = nullptr;

        bf_rt_id_t k_dst_id;
        bf_rt_id_t k_write_psn_out;
        bf_rt_id_t aid;
        bf_rt_id_t d_write_ring_id;

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;

        status = bfrtInfo->bfrtTableFromNameGet("get_write_ring_id_tbl", &tbl);
        if (tbl == nullptr) {
            return -1;
        }
        ASSERT_STATUS("Cannot get bfrt table");
        
        string str = "write_psn_out";//[" + to_string(WRITE_RING_SHIFT-1) + ":0]";
        status = tbl->keyFieldIdGet("dst_id", &k_dst_id);
        status = tbl->keyFieldIdGet(str, &k_write_psn_out);
        status = tbl->actionIdGet("ShuffleEgress.get_write_ring_id.set_write_ring_id", &aid);
        status = tbl->dataFieldIdGet("_write_ring_id", aid, &d_write_ring_id);

        for (int dst_id = 0; dst_id < n_ep; dst_id ++) {
            for (int ring_id = 0; ring_id < WRITE_RING_SIZE; ring_id ++) {
                status = tbl->keyAllocate(&key);
                status = tbl->dataAllocate(aid, &data);

                status = key->setValue(k_dst_id, dst_id);
                status = key->setValue(k_write_psn_out, ring_id);
                status = data->setValue(d_write_ring_id, (uint64_t)(dst_id<<WRITE_RING_SHIFT) | ring_id);
                status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
            }
        }
        status = sess->sessionCompleteOperations();
        loginfo("Successfully initialized get_write_ring_id_tbl");
        return 0;
    }

    int init_get_item_cnt_tbl() {
        bf_status_t status;
        const bfrt::BfRtTable *tbl = nullptr;

        bf_rt_id_t k_udp_len;
        bf_rt_id_t k_opcode;
        bf_rt_id_t aid;
        bf_rt_id_t d_item_cnt;

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;

        status = bfrtInfo->bfrtTableFromNameGet("get_item_cnt_tbl", &tbl);
        if (tbl == nullptr) {
            return -1;
        }
        ASSERT_STATUS("Cannot get bfrt table");
        
        status = tbl->keyFieldIdGet("hdr.udp.hdr_length", &k_udp_len);
        status = tbl->keyFieldIdGet("hdr.bth.opcode", &k_opcode);
        status = tbl->actionIdGet("ShuffleIngress.set_item_cnt", &aid);
        status = tbl->dataFieldIdGet("_item_cnt", aid, &d_item_cnt);

        int item_size = (int)sizeof(struct shuffle_request);
        for (int i = 1; i <= req_mtu / item_size; i ++) {
            status = tbl->keyAllocate(&key);
            status = tbl->dataAllocate(aid, &data);

            status = key->setValue(k_udp_len, i * item_size + 40);
            status = key->setValue(k_opcode, RDMA_OP_WRITE_FIRST);
            status = data->setValue(d_item_cnt, (uint64_t)i);
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
            
            status = tbl->keyAllocate(&key);
            status = tbl->dataAllocate(aid, &data);
            
            status = key->setValue(k_udp_len, i * item_size + 40);
            status = key->setValue(k_opcode, RDMA_OP_WRITE_ONLY);
            status = data->setValue(d_item_cnt, (uint64_t)i);
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
            
            status = tbl->keyAllocate(&key);
            status = tbl->dataAllocate(aid, &data);
            
            status = key->setValue(k_udp_len, i * item_size + 24);
            status = key->setValue(k_opcode, RDMA_OP_WRITE_MIDDLE);
            status = data->setValue(d_item_cnt, (uint64_t)i);
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
            
            status = tbl->keyAllocate(&key);
            status = tbl->dataAllocate(aid, &data);
            
            status = key->setValue(k_udp_len, i * item_size + 24);
            status = key->setValue(k_opcode, RDMA_OP_WRITE_LAST);
            status = data->setValue(d_item_cnt, (uint64_t)i);
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
        }
        status = sess->sessionCompleteOperations();
        loginfo("Successfully initialized get_item_cnt_tbl");
        return 0;
    }

    int init_get_unit_id_from_dst_tbl() {
        bf_status_t status;
        const bfrt::BfRtTable *tbl = nullptr;

        bf_rt_id_t k_dst_id;
        bf_rt_id_t k_psn;
        bf_rt_id_t k_priority;
        bf_rt_id_t aid;
        bf_rt_id_t d_unit_id;
        bf_rt_id_t d_item_id;

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;

        status = bfrtInfo->bfrtTableFromNameGet("get_unit_id_from_dst_tbl", &tbl);
        if (tbl == nullptr) {
            return -1;
        }
        ASSERT_STATUS("Cannot get bfrt table");
        
        status = tbl->keyFieldIdGet("dst_id", &k_dst_id);
        status = tbl->keyFieldIdGet("hdr.bth.psn", &k_psn);
        status = tbl->keyFieldIdGet("$MATCH_PRIORITY", &k_priority);
        status = tbl->actionIdGet("ShuffleIngress.set_unit_id_item_id", &aid);
        status = tbl->dataFieldIdGet("_unit_id", aid, &d_unit_id);
        status = tbl->dataFieldIdGet("_item_id", aid, &d_item_id);


        // search in ascending order
        int pri = 0;
        int item_size = (int)sizeof(struct shuffle_request);
        int item_per_unit = req_mtu / item_size;
        for (int i = 0; i < MAX_NUM_ENDPOINT; i ++) {
            for (int j = 0; j < UNIT_PER_ENDP; j ++) {
                status = tbl->keyAllocate(&key);
                status = tbl->dataAllocate(aid, &data);

                status = key->setValue(k_dst_id, i);
                status = key->setValueandMask(k_psn, j, UNIT_MASK);
                status = key->setValue(k_priority, pri ++);
                uint64_t unit_id = (uint64_t)(i<<UNIT_SHIFT) + j;
                status = data->setValue(d_unit_id, unit_id);
                status = data->setValue(d_item_id, unit_id * item_per_unit);
                status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
            }
        }
        status = sess->sessionCompleteOperations();
        loginfo("Successfully initialized get_unit_id_from_dst_tbl");
        return 0;
    }

    int init_get_unit_id_from_item_tbl() {
        bf_status_t status;
        const bfrt::BfRtTable *tbl = nullptr;

        bf_rt_id_t k_item_id;
        bf_rt_id_t k_priority;
        bf_rt_id_t aid;
        bf_rt_id_t d_unit_id;

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;

        status = bfrtInfo->bfrtTableFromNameGet("get_unit_id_from_item_tbl", &tbl);
        if (tbl == nullptr) {
            return -1;
        }
        ASSERT_STATUS("Cannot get bfrt table");
        
        status = tbl->keyFieldIdGet("item_id", &k_item_id);
        status = tbl->keyFieldIdGet("$MATCH_PRIORITY", &k_priority);
        status = tbl->actionIdGet("ShuffleIngress.set_unit_id", &aid);
        status = tbl->dataFieldIdGet("_unit_id", aid, &d_unit_id);

        // search in ascending order
        int pri = 0;
        int item_size = (int)sizeof(struct shuffle_request);
        int item_per_unit = req_mtu / item_size;
        for (int i = 0; i < MAX_NUM_ENDPOINT * UNIT_PER_ENDP; i ++) {
            status = tbl->keyAllocate(&key);
            status = tbl->dataAllocate(aid, &data);

            status = key->setValueandMask(k_item_id, (uint64_t)i * item_per_unit, ((uint64_t)-item_per_unit) & 0xffffull);
            status = key->setValue(k_priority, pri ++);
            uint64_t unit_id = (uint64_t)i;
            status = data->setValue(d_unit_id, unit_id);
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
            logassert(status != BF_SUCCESS, "Cannot add table entry");
        }
        status = sess->sessionCompleteOperations();
        loginfo("Successfully initialized get_unit_id_from_item_tbl");
        return 0;
    }

    int init_get_unit_id_from_shl_tbl() {
        
        bf_status_t status;
        const bfrt::BfRtTable *tbl = nullptr;

        bf_rt_id_t k_shl_unit_id;
        bf_rt_id_t aid;
        bf_rt_id_t d_unit_id;

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;

        status = bfrtInfo->bfrtTableFromNameGet("get_unit_id_from_shl_tbl", &tbl);
        if (tbl == nullptr) {
            return -1;
        }
        ASSERT_STATUS("Cannot get bfrt table");
        
        status = tbl->keyFieldIdGet("shl_unit_id", &k_shl_unit_id);
        status = tbl->actionIdGet("ShuffleEgress.set_unit_id", &aid);
        status = tbl->dataFieldIdGet("_unit_id", aid, &d_unit_id);

        for (int i = 0; i < MAX_NUM_ENDPOINT * UNIT_PER_ENDP; i ++) {
            status = tbl->keyAllocate(&key);
            status = tbl->dataAllocate(aid, &data);

            status = key->setValue(k_shl_unit_id, i << SHL_UNIT_SHIFT);
            uint64_t unit_id = (uint64_t)i;
            status = data->setValue(d_unit_id, unit_id);
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
        }
        status = sess->sessionCompleteOperations();
        loginfo("Successfully initialized get_unit_id_from_shl_tbl");
        return 0;
    }

    int init_tx_repl_loopback_tbl() {
        bf_status_t status;
        const bfrt::BfRtTable *tbl = nullptr;
        /* ALl field ids */
        bf_rt_id_t k_src_mac;
        bf_rt_id_t aid;
        bf_rt_id_t d_port;

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;

        status = bfrtInfo->bfrtTableFromNameGet("tx_repl_loopback_tbl", &tbl);
        if (tbl == nullptr) {
            return -1;
        }
        ASSERT_STATUS("Cannot get bfrt table");
        status = tbl->keyFieldIdGet("hdr.eth.src_addr", &k_src_mac);
        status = tbl->actionIdGet("ShuffleIngress.tx_repl_loopback_action", &aid);
        status = tbl->dataFieldIdGet("port", aid, &d_port);

        uint8_t mac_val[6];

        for (size_t i = 0; i < sizeof(macs) / sizeof(macs[0]) - 1; i ++) {
            status = tbl->keyAllocate(&key);
            status = tbl->dataAllocate(aid, &data);

            str2mac(macs[i], mac_val);
            status = key->setValue(k_src_mac, mac_val, 6);
            status = data->setValue(d_port, lpbk_ports[i]);
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
        }
        status = sess->sessionCompleteOperations();
        loginfo("Successfully initialized tx_repl_loopback_tbl table.");
        return 0;
    }

    int init_tx_request_loopback_tbl() {
        bf_status_t status;
        const bfrt::BfRtTable *tbl = nullptr;
        /* ALl field ids */
        bf_rt_id_t k_src_mac;
        bf_rt_id_t aid;
        bf_rt_id_t d_port;

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;

        status = bfrtInfo->bfrtTableFromNameGet("tx_request_loopback_tbl", &tbl);
        if (tbl == nullptr) {
            return -1;
        }
        ASSERT_STATUS("Cannot get bfrt table");
        status = tbl->keyFieldIdGet("hdr.eth.src_addr", &k_src_mac);
        status = tbl->actionIdGet("ShuffleIngress.tx_request_loopback_action", &aid);
        status = tbl->dataFieldIdGet("port", aid, &d_port);

        uint8_t mac_val[6];

        for (size_t i = 0; i < sizeof(macs) / sizeof(macs[0]) - 1; i ++) {
            status = tbl->keyAllocate(&key);
            status = tbl->dataAllocate(aid, &data);

            str2mac(macs[i], mac_val);
            status = key->setValue(k_src_mac, mac_val, 6);
            status = data->setValue(d_port, lpbk_ports[i]);
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
        }
        status = sess->sessionCompleteOperations();
        loginfo("Successfully initialized tx_request_loopback_tbl table.");
        return 0;
    }

    int init_tx_req_ack_tbl() {
        bf_status_t status;
        const bfrt::BfRtTable *tbl = nullptr;

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
        if (tbl == nullptr) {
            return -1;
        }
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

        for (int i = 0; i < n_ep; i ++) {
            status = tbl->keyAllocate(&key);
            status = tbl->dataAllocate(aid, &data);
            // dst_id = i
            status = key->setValue(k_dst_id, i);
            status = data->setValue(d_port, ports[fp_rank[i]]);
            
            str2mac(vir_mac, mac_val);
            status = data->setValue(d_src_mac, mac_val, 6);
            str2mac(macs[fp_rank[i]], mac_val);
            status = data->setValue(d_dst_mac, mac_val, 6);
            status = data->setValue(d_src_ip, (uint64_t)ntohl(str2ip(vir_ip)));
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
        const bfrt::BfRtTable *tbl = nullptr;

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
        if (tbl == nullptr) {
            return -1;
        }
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

        for (int i = 0; i < n_ep; i ++) {
            status = tbl->keyAllocate(&key);
            status = tbl->dataAllocate(aid, &data);
            // dst_id = i
            status = key->setValue(k_dst_id, i);
            status = data->setValue(d_port, (uint64_t)ports[fp_rank[i]]);
            
            str2mac(vir_mac, mac_val);
            status = data->setValue(d_src_mac, mac_val, 6);
            str2mac(macs[fp_rank[i]], mac_val);
            status = data->setValue(d_dst_mac, mac_val, 6);
            status = data->setValue(d_src_ip, (uint64_t)ntohl(str2ip(vir_ip)));
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
        const bfrt::BfRtTable *tbl = nullptr;

        bf_rt_id_t k_src_mac;
        bf_rt_id_t aid;
        bf_rt_id_t d_port;
        bf_rt_id_t d_sess;

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;

        uint8_t mac_val[6];

        status = bfrtInfo->bfrtTableFromNameGet("ShuffleIngress.tx_repl_many_tbl", &tbl);
        if (tbl == nullptr) {
            return -1;
        }
        ASSERT_STATUS("Cannot get bfrt table");
        
        status = tbl->keyFieldIdGet("hdr.eth.src_addr", &k_src_mac);
        status = tbl->actionIdGet("ShuffleIngress.tx_repl_many_action", &aid);
        status = tbl->dataFieldIdGet("port", aid, &d_port);
        status = tbl->dataFieldIdGet("sess", aid, &d_sess);

        for (int i = 0; i < n_ep; i ++) {
            status = tbl->keyAllocate(&key);
            status = tbl->dataAllocate(aid, &data);
            // dst_id = i
            str2mac(macs[fp_rank[i]], mac_val);
            status = key->setValue(k_src_mac, mac_val, 6);

            status = data->setValue(d_port, (uint64_t)lpbk_ports[fp_rank[i]]);
            status = data->setValue(d_sess, mir_sess[fp_rank[i]]);
            
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
        }

        status = sess->sessionCompleteOperations();
        loginfo("Successfully initialized tx_repl_many_tbl");
        return 0;
    }

    int init_tx_src_read_tbl() {
        bf_status_t status;
        const bfrt::BfRtTable *tbl = nullptr;

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
        if (tbl == nullptr) {
            return -1;
        }
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

        for (int i = 0; i < n_ep; i ++) {
            status = tbl->keyAllocate(&key);
            status = tbl->dataAllocate(aid, &data);
            // src_id = i
            for (int j = 0; j < n_ep; j ++) {
                // dst_id = j
                status = key->setValue(k_src_id, i);
                status = key->setValue(k_dst_id, j);
                status = data->setValue(d_port, (uint64_t)ports[fp_rank[i]]);
                
                str2mac(vir_mac, mac_val);
                status = data->setValue(d_src_mac, mac_val, 6);
                str2mac(macs[fp_rank[i]], mac_val);
                status = data->setValue(d_dst_mac, mac_val, 6);
                status = data->setValue(d_src_ip, (uint64_t)ntohl(str2ip(vir_ip)));
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
        const bfrt::BfRtTable *tbl = nullptr;

        bf_rt_id_t k_src_mac;
        bf_rt_id_t k_src_id;
        bf_rt_id_t aid;
        bf_rt_id_t d_dst_id;
        bf_rt_id_t d_src_dst_id;

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;

        uint8_t mac_val[6];

        status = bfrtInfo->bfrtTableFromNameGet("ShuffleIngress.rx_repl_only_tbl", &tbl);
        if (tbl == nullptr) {
            return -1;
        }
        ASSERT_STATUS("Cannot get bfrt table");
        
        status = tbl->keyFieldIdGet("hdr.eth.src_addr", &k_src_mac);
        status = tbl->keyFieldIdGet("hdr.item0.src_id", &k_src_id);
        status = tbl->actionIdGet("ShuffleIngress.rx_repl_only_action", &aid);
        status = tbl->dataFieldIdGet("_dst_id", aid, &d_dst_id);
        status = tbl->dataFieldIdGet("_src_dst_id", aid, &d_src_dst_id);

        for (int src_id = 0; src_id < n_ep; src_id ++) {
            for (int dst_id = 0; dst_id < n_ep; dst_id ++) {
                status = tbl->keyAllocate(&key);
                status = tbl->dataAllocate(aid, &data);

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

    int init_roce_case_tbl() {
        
        bf_status_t status;
        const bfrt::BfRtTable *tbl = nullptr;

        bf_rt_id_t k_src_mac;
        bf_rt_id_t k_opcode;
        bf_rt_id_t k_dqpn;
        bf_rt_id_t aid;
        bf_rt_id_t d_dst_id;
        bf_rt_id_t d_src_dst_id;

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;

        uint8_t mac_val[6];

        status = bfrtInfo->bfrtTableFromNameGet("ShuffleIngress.roce_case_tbl", &tbl);
        if (tbl == nullptr) {
            return -1;
        }
        ASSERT_STATUS("Cannot get bfrt table");
        
        status = tbl->keyFieldIdGet("hdr.eth.src_addr", &k_src_mac);
        status = tbl->keyFieldIdGet("hdr.bth.opcode", &k_opcode);
        status = tbl->keyFieldIdGet("hdr.bth.dqpn", &k_dqpn);

        // request first
        status = tbl->actionIdGet("ShuffleIngress.rx_request_first", &aid);
        status = tbl->dataFieldIdGet("_dst_id", aid, &d_dst_id);

        for (int i = 0; i < n_ep; i ++) {
            status = tbl->keyAllocate(&key);
            status = tbl->dataAllocate(aid, &data);

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

        for (int i = 0; i < n_ep; i ++) {
            status = tbl->keyAllocate(&key);
            status = tbl->dataAllocate(aid, &data);

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

        for (int i = 0; i < n_ep; i ++) {
            status = tbl->keyAllocate(&key);
            status = tbl->dataAllocate(aid, &data);

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

        for (int i = 0; i < n_ep; i ++) {
            status = tbl->keyAllocate(&key);
            status = tbl->dataAllocate(aid, &data);

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

        for (int src_id = 0; src_id < n_ep; src_id ++) {
            for (int dst_id = 0; dst_id < n_ep; dst_id ++) {
                status = tbl->keyAllocate(&key);
                status = tbl->dataAllocate(aid, &data);

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

        for (int src_id = 0; src_id < n_ep; src_id ++) {
            for (int dst_id = 0; dst_id < n_ep; dst_id ++) {
                status = tbl->keyAllocate(&key);
                status = tbl->dataAllocate(aid, &data);

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

        for (int i = 0; i < n_ep; i ++) {
            status = tbl->keyAllocate(&key);
            status = tbl->dataAllocate(aid, &data);

            str2mac(macs[fp_rank[i]], mac_val);
            status = key->setValue(k_src_mac, mac_val, 6);
            status = key->setValue(k_opcode, RDMA_OP_ACK);
            status = key->setValue(k_dqpn, vir_qp_info.dst_qpn);

            status = data->setValue(d_dst_id, (uint64_t)i);
            
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
        }
        status = sess->sessionCompleteOperations();
        loginfo("Successfully initialized roce_case_tbl");
        return 0;
    }
    
    int init_check_read_ring_tbl() {
        bf_status_t status;
        const bfrt::BfRtTable *tbl = nullptr;

        bf_rt_id_t k_sub_val;
        bf_rt_id_t k_priority;
        bf_rt_id_t aid;

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;
        
        status = bfrtInfo->bfrtTableFromNameGet("ShuffleIngress.check_read_ring.check_read_ring_tbl", &tbl);
        if (tbl == nullptr) {
            return -1;
        }
        ASSERT_STATUS("Cannot get bfrt table");
        
        status = tbl->keyFieldIdGet("sub_val", &k_sub_val);
        status = tbl->keyFieldIdGet("$MATCH_PRIORITY", &k_priority);

        int pri = 0;
        status = tbl->actionIdGet("ShuffleIngress.check_read_ring.read_ring_full", &aid);

        for (int i = READ_RING_SHIFT; i < 32; i ++) {
            status = tbl->keyAllocate(&key);
            status = tbl->dataAllocate(aid, &data);

            status = key->setValueandMask(k_sub_val, 1ull<<i, 1ull<<i);
            status = key->setValue(k_priority, pri ++);
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
        }

        status = sess->sessionCompleteOperations();
        loginfo("Successfully initialized check_read_ring_tbl");
        return 0;
    }

    int init_request_case_tbl() {
        bf_status_t status;
        const bfrt::BfRtTable *tbl = nullptr;

        bf_rt_id_t k_req_epsn_val;
        bf_rt_id_t k_req_unack_unit_out;
        bf_rt_id_t k_priority;
        bf_rt_id_t aid;

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;
        
        status = bfrtInfo->bfrtTableFromNameGet("ShuffleEgress.request_case.request_case_tbl", &tbl);
        if (tbl == nullptr) {
            return -1;
        }
        ASSERT_STATUS("Cannot get bfrt table");
        
        status = tbl->keyFieldIdGet("req_epsn_val", &k_req_epsn_val);
        status = tbl->keyFieldIdGet("req_unack_unit_out", &k_req_unack_unit_out);
        status = tbl->keyFieldIdGet("$MATCH_PRIORITY", &k_priority);

        int pri = 0;
        // out-of-order request
        status = tbl->actionIdGet("ShuffleEgress.request_case.out_of_order_req", &aid);
        status = tbl->keyAllocate(&key);
        status = tbl->dataAllocate(aid, &data);
        // epsn < psn, epsn-psn<0
        status = key->setValueandMask(k_req_epsn_val, 0xff000000u, 0xff000000u);
        status = key->setValueandMask(k_req_unack_unit_out, 0, 0);
        status = key->setValue(k_priority, pri ++);
        status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);

        // duplicate request
        status = tbl->actionIdGet("ShuffleEgress.request_case.duplicate_req", &aid);
        status = tbl->keyAllocate(&key);
        status = tbl->dataAllocate(aid, &data);
        // psn <= epsn - UNIT_PER_ENDP, epsn-psn >= UNIT_PER_ENDP
        for (int i = UNIT_SHIFT; i < 24; i ++) {
            status = key->setValueandMask(k_req_epsn_val, (1<<i), (1<<i));
            status = key->setValueandMask(k_req_unack_unit_out, 0, 0);
            status = key->setValue(k_priority, pri ++);
            status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
        }
        // unit < unackunit
        // when 0<=(epsn-psn)<N
        // psn<unack_psn
        // (epsn-psn)>(epsn-psn+psn-unack_psn)
        // (epsn-psn)>((epsn-psn)+(psn%-unack_psn%)%)%
        // (epsn-psn)>((epsn-psn)+(unit-unack_unit)%)%
        // A>(A+B)%N
        for (uint32_t i = 0; i < UNIT_PER_ENDP; i ++) {
            for (uint32_t j = 0; j < UNIT_PER_ENDP; j ++) {
                if (i > ((i+j)&UNIT_MASK)) {
                    status = key->setValueandMask(k_req_epsn_val, i, UNIT_MASK);
                    status = key->setValueandMask(k_req_unack_unit_out, j << SHL_UNIT_SHIFT, 0xffffu);
                    status = key->setValue(k_priority, pri ++);
                    status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);
                }
            }
        }

        // run out of unack unit
        status = tbl->actionIdGet("ShuffleEgress.request_case.run_out_of_unit", &aid);
        status = tbl->keyAllocate(&key);
        status = tbl->dataAllocate(aid, &data);
        // unit_minus_unack == (UNIT_PER_ENDP-1) << SHL_UNIT_SHIFT
        status = key->setValueandMask(k_req_epsn_val, 0, 0);
        status = key->setValueandMask(k_req_unack_unit_out, (UNIT_PER_ENDP-1) << SHL_UNIT_SHIFT, 0xffffu);
        status = key->setValue(k_priority, pri ++);
        status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);

        // correct request
        status = tbl->actionIdGet("ShuffleEgress.request_case.correct_req", &aid);
        status = tbl->keyAllocate(&key);
        status = tbl->dataAllocate(aid, &data);
        // epsn=psn
        status = key->setValueandMask(k_req_epsn_val, 0, 0xffffffffu);
        status = key->setValueandMask(k_req_unack_unit_out, 0, 0);
        status = key->setValue(k_priority, pri ++);
        status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);

        // retry request
        status = tbl->actionIdGet("ShuffleEgress.request_case.retry_req", &aid);
        status = tbl->keyAllocate(&key);
        status = tbl->dataAllocate(aid, &data);
        // otherwise
        status = key->setValueandMask(k_req_epsn_val, 0, 0);
        status = key->setValueandMask(k_req_unack_unit_out, 0, 0);
        status = key->setValue(k_priority, pri ++);
        status = tbl->tableEntryAdd(*sess, dev_tgt, *key, *data);

        status = sess->sessionCompleteOperations();
        loginfo("Successfully initialized request_case_tbl");
        return 0;
    }

    
    int init_register(string str, vector<uint32_t> vals) {
        bf_status_t status;
        const bfrt::BfRtTable *reg = nullptr;
        string table_name = str + "";
        status = bfrtInfo->bfrtTableFromNameGet(table_name, &reg);
        
        if (reg == nullptr) {
            return -1;
        }

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;
        bf_rt_id_t key_field_id;
        bf_rt_id_t data_field_id;
        status = reg->keyFieldIdGet("$REGISTER_INDEX", &key_field_id);
        string df_name = str + ".f1";
        status = reg->dataFieldIdGet(df_name, &data_field_id);

        size_t reg_size;
        status = reg->tableSizeGet(*sess, dev_tgt, (size_t *)&reg_size);
        logassert(reg_size < vals.size(), "register size mismatch");

        // uint8_t data_buf[4];
        // memset(data_buf, 0, sizeof(data_buf));
        // *(uint32_t *)data_buf = htonl(val);
        // status = data->setValue(data_field_id, data_buf, 4);

        for (size_t i = 0; i < vals.size(); i += batch_size) {
            status = sess->beginBatch();
            for (size_t j = i; j < i + batch_size && j < vals.size(); j ++) {
                status = reg->keyAllocate(&key);
                status = reg->dataAllocate(&data);

                status = key->setValue(key_field_id, j);
                status = data->setValue(data_field_id, (uint64_t)vals[j]);
                status = reg->tableEntryMod(*sess, dev_tgt, *key, *data);
            }
            status = sess->endBatch(false);
        }
        status = sess->sessionCompleteOperations();
        return 0;
    }

    int dump_reg(string str, size_t max_size=65536) {
        bf_status_t status;
        const bfrt::BfRtTable::BfRtTableGetFlag flag = bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_HW;
        const bfrt::BfRtTable *reg = nullptr;
        string table_name = str;
        status = bfrtInfo->bfrtTableFromNameGet(table_name, &reg);
        if (reg == nullptr) {
            return -1;
        }

        unique_ptr<bfrt::BfRtTableKey> key;
        unique_ptr<bfrt::BfRtTableData> data;
        bf_rt_id_t key_field_id;
        bf_rt_id_t data_field_id;
        status = reg->keyFieldIdGet("$REGISTER_INDEX", &key_field_id);
        string df_name = str + ".f1";
        status = reg->dataFieldIdGet(df_name, &data_field_id);

        size_t reg_size;
        status = reg->tableSizeGet(*sess, dev_tgt, (size_t *)&reg_size);
        reg_size = min(reg_size, max_size);

        vector<uint64_t> data_buf;
        status = reg->dataAllocate(&data);

        for (size_t i = 0; i < reg_size; i ++) {
            status = reg->keyAllocate(&key);

            status = key->setValue(key_field_id, i);
            status = reg->tableEntryGet(*sess, dev_tgt, *key, flag, data.get());
        }

        status = sess->sessionCompleteOperations();

        status = data->getValue(data_field_id, &data_buf);
        ASSERT_STATUS("Cannot get value from data");

        logger << str << "   ";
        for (size_t i = 0; i < reg_size; i ++) {
            logger << i << ":" << data_buf[2*i+1] << ", ";
        }
        logger << "\n";
        return 0;
    }

    void clear_tables() {
        clear_table("get_read_ring_id_tbl");
        clear_table("get_write_ring_id_tbl");
        clear_table("get_item_cnt_tbl");
        clear_table("get_unit_id_from_dst_tbl");
        clear_table("get_unit_id_from_item_tbl");
        clear_table("get_unit_id_from_shl_tbl");
        clear_table("tx_repl_loopback_tbl");
        clear_table("tx_request_loopback_tbl");
        clear_table("tx_req_ack_tbl");
        clear_table("tx_dst_write_tbl");
        clear_table("tx_repl_many_tbl");
        clear_table("tx_src_read_tbl");
        clear_table("rx_repl_only_tbl");
        clear_table("roce_case_tbl");
    }

    void init_tables() {
        init_get_read_ring_id_tbl();
        init_get_write_ring_id_tbl();
        init_get_item_cnt_tbl();
        init_get_unit_id_from_dst_tbl();
        init_get_unit_id_from_item_tbl();
        init_get_unit_id_from_shl_tbl();
        init_tx_repl_loopback_tbl();
        init_tx_request_loopback_tbl();
        init_tx_req_ack_tbl();
        init_tx_dst_write_tbl();
        init_tx_repl_many_tbl();
        init_tx_src_read_tbl();
        init_rx_repl_only_tbl();
        init_roce_case_tbl();
    }

    void init_registers() {
        vector<uint32_t> endp_state(n_ep, 1);
        init_register("ShuffleIngress.endp_state", endp_state);

        vector<uint32_t> dst_vals(n_ep, 0);
        init_register("ShuffleEgress.req_msn.req_msn", dst_vals);
        for (int i = 0; i < n_ep; i ++) {
            dst_vals[i] = ep_infos[i].psn;
        }
        init_register("ShuffleIngress.write_psn.write_psn", dst_vals);
        init_register("ShuffleEgress.req_epsn.req_epsn", dst_vals);
        for (int i = 0; i < n_ep; i ++) {
            dst_vals[i] = (uint16_t)(ep_infos[i].psn << SHL_UNIT_SHIFT);
        }
        init_register("ShuffleEgress.req_unack_unit.req_unack_unit", dst_vals);

        vector<uint32_t> src_dst_vals(n_ep * n_ep, 0);
        for (int i = 0; i < n_ep; i ++) {
            for (int j = 0; j < n_ep; j ++) {
                src_dst_vals[i * n_ep + j] = ep_infos[i].psn;
            }
        }
        init_register("ShuffleIngress.read_unack_psn.read_unack_psn", src_dst_vals);
        init_register("ShuffleIngress.read_psn.read_psn", src_dst_vals);
    }

    void start() override {
        initialize();
        init_l2_route();
        init_request_case_tbl();
        init_check_read_ring_tbl();
        return ;
    }
};
