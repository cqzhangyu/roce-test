#include "shuffle_header.p4"
#include "shuffle_ingress_parser.p4"

control ShuffleIngress(
        inout ig_header_t hdr,
        inout ig_metadata_t ig_md,
        in ingress_intrinsic_metadata_t ig_intr_md,
        in ingress_intrinsic_metadata_from_parser_t ig_prsr_md,
        inout ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md,
        inout ingress_intrinsic_metadata_for_tm_t ig_tm_md) {
    
    /******** shuffle metadata ********/
    // shuffle src id
    bit<16> src_dst_id = 0;
    // shuffle dst id
    bit<16> dst_id = 0;
    // shuffle unit id
    bit<16> unit_id = 0;
    // payload length
    bit<16> item_len = 0;
    // PSN ring id
    bit<16> ring_id = 0;

    // method case
    bit<8> method_case = 0;

    bool inc_req_msn = false;
    bool update_req_dst_addr = false;
    bool update_endp_state = false;

    // temporary variables
    bit<32> tmp_msn = 0;
    bit<32> tmp_psn = 0;
    bit<64> tmp_dst_addr = 0;
    bit<64> tmp_write_offset = 0;
    bit<32> tmp_read_ring_head = 0;
    bit<8> tmp_endp_state = 0;
    
    action drop() {
        ig_dprsr_md.drop_ctl = 1;
    }
    action l2_forward(PortId_t port) {
        ig_tm_md.ucast_egress_port = port;
    }
    table l2_route {
        key = {
            hdr.eth.dst_addr : exact;
        }
        actions = {
            l2_forward;
            drop;
        }
        size = 512;
        default_action = drop;
    }

    action tx_loopback_action(PortId_t port) {
        ig_tm_md.ucast_egress_port = port;
    }

    table tx_loopback_tbl {
        key = {
            hdr.eth.src_addr : exact;
        }
        actions = {
            tx_loopback_action;
            drop;
        }
        size = 512;
        default_action = drop;
    }

    Register<bit<8>, bit<16> >(MAX_NUM_ENDP) endp_state;

    Register<bit<32>, bit<16> >(MAX_NUM_ENDP * MAX_NUM_ENDP) read_unack_psn;

    Register<bit<32>, bit<16> >(MAX_NUM_ENDP * MAX_NUM_ENDP) read_psn;

    Register<bit<16>, bit<16> >(MAX_NUM_ENDP * MAX_NUM_ENDP * READ_RING_SIZE) read_psn_to_item;
    Register<bit<32>, bit<16> >(MAX_NUM_ENDP) write_psn;

    Register<bit<16>, bit<16> >(MAX_NUM_ENDP * UNIT_PER_ENDP * ITEM_PER_UNIT) write_psn_to_unit;

    Register<bit<32>, bit<16> >(MAX_NUM_ENDP * UNIT_PER_ENDP) unit_req_psn;
    
    Register<bit<16>, bit<16> >(MAX_NUM_ENDP) req_unack_unit;
    Register<bit<32>, bit<16> >(MAX_NUM_ENDP) req_epsn;

    Register<bit<32>, bit<16> >(MAX_NUM_ENDP) req_msn;
    Register<bit<32>, bit<16> >(MAX_NUM_ENDP) req_dst_addr_hi;
    Register<bit<32>, bit<16> >(MAX_NUM_ENDP) req_dst_addr_lo;
    
    Register<bit<32>, bit<16> >(MAX_NUM_ENDP * UNIT_PER_ENDP) unit_dst_addr_hi;
    Register<bit<32>, bit<16> >(MAX_NUM_ENDP * UNIT_PER_ENDP) unit_dst_addr_lo;
    Register<bit<32>, bit<16> >(MAX_NUM_ENDP * UNIT_PER_ENDP) unit_req_msn;
    Register<bit<16>, bit<16> >(MAX_NUM_ENDP * UNIT_PER_ENDP) unit_remain;
    Register<bit<32>, bit<16> >(MAX_NUM_ENDP * UNIT_PER_ENDP * ITEM_PER_UNIT) item_write_offset;

    
    action calculate_unit_id_hi() {
        unit_id = dst_id << UNIT_SHIFT;
    }
    action calculate_unit_id_lo(bit<16> unit_id_tail) {
        unit_id = unit_id | unit_id_tail;
    }
    action calculate_unit_id_from_item_id(bit<16> item_id) {
        unit_id = item_id >> ITEM_SHIFT;
    }

    action calculate_write_ring_id_hi() {
        ring_id = dst_id << WRITE_RING_SHIFT;
    }

    action calculate_read_ring_id_hi() {
        ring_id = src_dst_id << READ_RING_SHIFT;
    }

    action calculate_ring_id_lo(bit<16> ring_id_tail) {
        ring_id = ring_id | ring_id_tail;
    }

    action calculate_item_cnt() {
        hdr.repl.item_cnt = (item_len >> ITEM_SIZE_SHIFT);
    }

    action rx_request_first(bit<16> _dst_id) {
        dst_id = _dst_id;   // based on smac
        item_len = REQ_MTU;

        method_case = CASE_REQUEST;
        update_req_dst_addr = true;
    }

    action rx_request_middle(bit<16> _dst_id) {
        dst_id = _dst_id;
        item_len = REQ_MTU;

        method_case = CASE_REQUEST;
    }

    action rx_request_last(bit<16> _dst_id) {
        dst_id = _dst_id;
        item_len = hdr.udp.hdr_length - 24;

        method_case = CASE_REQUEST;
        inc_req_msn = true;
    }

    action rx_request_only(bit<16> _dst_id) {
        dst_id = _dst_id;
        item_len = hdr.udp.hdr_length - 40;

        method_case = CASE_REQUEST;
        inc_req_msn = true;
        update_req_dst_addr = true;
    }

    action rx_read_response(bit<16> _dst_id, bit<16> _src_dst_id) {
        dst_id = _dst_id;   // based on dqpn
        src_dst_id = _src_dst_id;   // based on smac

        method_case = CASE_READ_RESPONSE;
    }

    action rx_read_nak(bit<16> _dst_id, bit<16> _src_dst_id) {
        dst_id = _dst_id;   // based on dqpn
        src_dst_id = _src_dst_id;   // based on smac

        method_case = CASE_READ_NAK;
        update_endp_state = true;
        tmp_endp_state = 0;
    }

    action rx_write_ack(bit<16> _dst_id) {
        dst_id = _dst_id;   // based on smac

        method_case = CASE_WRITE_ACK;
    }

    action rx_write_nak(bit<16> _dst_id) {
        dst_id = _dst_id;   // based on smac

        method_case = CASE_WRITE_NAK;
        update_endp_state = true;
        tmp_endp_state = 0;
    }

    table roce_method_tbl {
        key = {
            hdr.eth.src_addr : exact;
            hdr.bth.opcode : exact;
            hdr.bth.dqpn : exact;

        }
        actions = {
            rx_request_first;
            rx_request_middle;
            rx_request_last;
            rx_request_only;
            rx_read_response;
            rx_read_nak;
            rx_write_ack;
            
            NoAction;
        }
        size = 512;
        default_action = NoAction;
    }

    action rx_repl_single(bit<16> _dst_id, bit<16> _src_dst_id) {
        dst_id = _dst_id;   // based on smac
        src_dst_id = _src_dst_id;   // from item

        method_case = CASE_REPL_ONLY;
    }

    table rx_repl_only_tbl {
        key = {
            hdr.eth.src_addr : exact;
            hdr.item0.src_id : exact;
        }
        actions = {
            rx_repl_single;
            NoAction;
        }
        size = 512;
        default_action = NoAction;
    }
    
    action tx_src_read_action(PortId_t port,
                          mac_addr_t src_mac, 
                          mac_addr_t dst_mac, 
                          ipv4_addr_t src_ip, 
                          ipv4_addr_t dst_ip, 
                          bit<16> udp_src_port, 
                          bit<24> dqpn,
                          bit<32> rkey) {
        ig_tm_md.ucast_egress_port = port;

        hdr.eth.src_addr = src_mac;
        hdr.eth.dst_addr = dst_mac;
        hdr.eth.ether_type = ETHERTYPE_IPV4;

        hdr.ipv4.ver_ihl = 0x45;
        hdr.ipv4.diffserv = 0x02;
        hdr.ipv4.total_len = 60;    // RoCEv2 READ 
        hdr.ipv4.identification = 0x1234;
        hdr.ipv4.flag_offset = 0x40;
        hdr.ipv4.ttl = 64;
        hdr.ipv4.protocol = IP_PROTOCOLS_UDP;
        hdr.ipv4.hdr_checksum = 0x0000;
        hdr.ipv4.src_addr = src_ip;
        hdr.ipv4.dst_addr = dst_ip;

        hdr.udp.src_port = udp_src_port;
        hdr.udp.dst_port = UDP_PORT_ROCE;
        hdr.udp.hdr_length = 40;   // RoCEv2 READ 
        hdr.udp.checksum = 0x0000;

        hdr.bth.opcode = RDMA_OP_READ_REQ;
        hdr.bth.se_migreq_pad_ver = 0x40;
        hdr.bth.pkey = 0xffff;
        hdr.bth.f_b_rsv = 0;
        hdr.bth.dqpn = dqpn;
        hdr.bth.ackreq_rsv = 0;

        hdr.reth.addr = hdr.item0.src_addr;
        hdr.reth.rkey = rkey;
        hdr.reth.len = (bit<32>)hdr.item0.len;
    }

    table tx_src_read_tbl {
        key = {
            hdr.item0.src_id : exact;
            dst_id : exact;
        }
        actions = {
            tx_src_read_action;
            NoAction;
        }
        size = 512;
        default_action = NoAction;
    }

    action tx_repl_only(PortId_t port) {
        hdr.eth.ether_type = ETHERTYPE_REPL;
        hdr.repl.item_cnt = 1;

        ig_tm_md.ucast_egress_port = port;
    }

    action tx_repl_many(PortId_t port, MirrorId_t sess) {
        // item0 in the mirror packet
        ig_tm_md.ucast_egress_port = port;

        ig_dprsr_md.mirror_type = MIRROR_TYPE_I2E;

        ig_md.sess = sess;
        ig_md.pkt_type = PKT_TYPE_MIRROR;
        ig_md.flag = 0;
        ig_md.item_id = hdr.repl.item_id;
        ig_md.item_cnt = 1;

        // item1... in the original packet
        hdr.eth.ether_type = ETHERTYPE_REPL;
        hdr.repl.item_id = hdr.repl.item_id + 1;
        hdr.repl.item_cnt = hdr.repl.item_cnt - 1;
        hdr.item0.setInvalid();
    }

    table tx_repl_only_tbl {
        key = {
            hdr.eth.src_addr : exact;
        }
        actions = {
            tx_repl_only;
            NoAction;
        }
        size = 256;
        default_action = NoAction;
    }
    
    table tx_repl_many_tbl {
        key = {
            hdr.eth.src_addr : exact;
        }
        actions = {
            tx_repl_many;
            NoAction;
        }
        size = 256;
        default_action = NoAction;
    }

    action tx_dst_write_action(PortId_t port,
                           mac_addr_t src_mac, 
                           mac_addr_t dst_mac, 
                           ipv4_addr_t src_ip, 
                           ipv4_addr_t dst_ip, 
                           bit<16> udp_src_port, 
                           bit<24> dqpn,
                           bit<32> rkey) {
        ig_tm_md.ucast_egress_port = port;
                            
        hdr.reth.len = (bit<32>)(hdr.udp.hdr_length - 28);

        hdr.eth.src_addr = src_mac;
        hdr.eth.dst_addr = dst_mac;
        // hdr.eth.ether_type = ETHERTYPE_IPV4;

        // hdr.ipv4.ver_ihl = 0x45;
        // hdr.ipv4.diffserv = 0x02;
        hdr.ipv4.total_len = hdr.ipv4.total_len + 12; // WRITE(payload+60) - READ(payload+48)
        // hdr.ipv4.identification = 0x1234;
        // hdr.ipv4.flag_offset = 0x40;
        // hdr.ipv4.ttl = 64;
        // hdr.ipv4.protocol = IP_PROTOCOLS_UDP;
        // hdr.ipv4.hdr_checksum = 0x0000;
        hdr.ipv4.src_addr = src_ip;
        hdr.ipv4.dst_addr = dst_ip;

        hdr.udp.src_port = udp_src_port;
        hdr.udp.dst_port = UDP_PORT_ROCE;
        hdr.udp.hdr_length = hdr.udp.hdr_length + 12;   // WRITE(payload+40) - READ(payload+28)
        hdr.udp.checksum = 0x0000;

        hdr.bth.opcode = RDMA_OP_WRITE_ONLY;
        // hdr.bth.se_migreq_pad_ver = 0x40;
        // hdr.bth.pkey = 0xffff;
        // hdr.bth.f_b_rsv = 0;
        hdr.bth.dqpn = dqpn;
        // hdr.bth.ackreq_rsv = x;

        hdr.reth.addr = tmp_dst_addr + tmp_write_offset;
        hdr.reth.rkey = rkey;
    }

    table tx_dst_write_tbl {
        key = {
            dst_id : exact;
        }
        actions = {
            tx_dst_write_action;
            NoAction;
        }
        size = 512;
        default_action = NoAction;
    }

    action tx_req_ack_action(PortId_t port,
                             mac_addr_t src_mac, 
                             mac_addr_t dst_mac, 
                             ipv4_addr_t src_ip, 
                             ipv4_addr_t dst_ip, 
                             bit<16> udp_src_port, 
                             bit<24> dqpn,
                             bit<32> rkey) {
        ig_tm_md.ucast_egress_port = port;

        hdr.eth.src_addr = src_mac;
        hdr.eth.dst_addr = dst_mac;
        // hdr.eth.ether_type = ETHERTYPE_IPV4;

        // hdr.ipv4.ver_ihl = 0x45;
        // hdr.ipv4.diffserv = 0x02;
        hdr.ipv4.total_len = 40; // ACK
        // hdr.ipv4.identification = 0x1234;
        // hdr.ipv4.flag_offset = 0x40;
        // hdr.ipv4.ttl = 64;
        // hdr.ipv4.protocol = IP_PROTOCOLS_UDP;
        // hdr.ipv4.hdr_checksum = 0x0000;
        hdr.ipv4.src_addr = src_ip;
        hdr.ipv4.dst_addr = dst_ip;

        hdr.udp.src_port = udp_src_port;
        hdr.udp.dst_port = UDP_PORT_ROCE;
        hdr.udp.hdr_length = 28;   // ACK
        hdr.udp.checksum = 0x0000;

        hdr.bth.opcode = RDMA_OP_ACK;
        // hdr.bth.se_migreq_pad_ver = 0x40;
        // hdr.bth.pkey = 0xffff;
        // hdr.bth.f_b_rsv = 0;
        hdr.bth.dqpn = dqpn;
        // hdr.bth.ackreq_rsv = x;

        hdr.aeth.syndrome = 0;
    }


    table tx_req_ack_tbl {
        key = {
            dst_id : exact;
        }
        actions = {
            tx_req_ack_action;
            NoAction;
        }
        size = 512;
        default_action = NoAction;
    }
    
    RegisterAction<bit<8>, bit<16>, bit<8> >(endp_state) endp_state_action = {
        void apply(inout bit<8> reg_data, out bit<8> result) {
            if (update_endp_state) {
                reg_data = tmp_endp_state;
            }
            result = reg_data;
        }
    };

    RegisterAction<bit<32>, bit<16>, bit<32> >(req_epsn) req_epsn_action = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            result = (bit<32>)(hdr.bth.psn) - reg_data;
            if ((bit<32>)(hdr.bth.psn) == reg_data) {
                reg_data = reg_data + 1;
            }
        }
    };

    RegisterAction<bit<16>, bit<16>, bit<16> >(req_unack_unit) req_unack_unit_sub = {
        void apply(inout bit<16> reg_data, out bit<16> result) {
            result = reg_data - unit_id;
        }
    };
    RegisterAction<bit<16>, bit<16>, bit<8> >(req_unack_unit) req_unack_unit_inc = {
        void apply(inout bit<16> reg_data, out bit<8> result) {
            if (reg_data == unit_id) {
                reg_data = reg_data + 1;
                result = 0;
            }
            else {
                result = 1;
            }
        }
    };


    RegisterAction<bit<32>, bit<16>, bit<32> >(req_msn) req_msn_action = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            result = reg_data;
            if (inc_req_msn) {
                reg_data = reg_data + 1;
            }
        }
    };
    
    RegisterAction<bit<32>, bit<16>, bit<32> >(req_dst_addr_hi) req_dst_addr_hi_action = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            if (update_req_dst_addr) {
                reg_data = hdr.reth.addr[63:32];
            }
            result = reg_data;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32> >(req_dst_addr_lo) req_dst_addr_lo_action = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            if (update_req_dst_addr) {
                reg_data = hdr.reth.addr[31:0];
            }
            result = reg_data;
        }
    };

    RegisterAction<bit<32>, bit<16>, bit<32> >(unit_dst_addr_hi) unit_dst_addr_hi_action = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            if (method_case == CASE_REQUEST) {
                reg_data = tmp_dst_addr[63:32];
            }
            result = reg_data;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32> >(unit_dst_addr_lo) unit_dst_addr_lo_action = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            if (method_case == CASE_REQUEST) {
                reg_data = tmp_dst_addr[31:0];
            }
            result = reg_data;
        }
    };

    RegisterAction<bit<32>, bit<16>, bit<32> >(unit_req_psn) unit_req_psn_action = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            if (method_case == CASE_REQUEST) {
                reg_data = (bit<32>)(hdr.bth.psn);
            }
            result = reg_data;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32> >(unit_req_msn) unit_req_msn_action = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            if (method_case == CASE_REQUEST) {
                reg_data = tmp_msn;
            }
            result = reg_data;
        }
    };

    RegisterAction<bit<16>, bit<16>, bit<16> >(unit_remain) unit_remain_set = {
        void apply(inout bit<16> reg_data, out bit<16> result) {
            reg_data = hdr.repl.item_cnt;
        }
    };
    RegisterAction<bit<16>, bit<16>, bit<8> >(unit_remain) unit_remain_dec = {
        void apply(inout bit<16> reg_data, out bit<8> result) {
            if (reg_data == 1) {
                result = 0x80;  // ackreq
            }
            else {
                result = 0;
            }
            reg_data = reg_data - 1;
        }
    };
    

    RegisterAction<bit<32>, bit<16>, bit<32> >(read_unack_psn) read_unack_psn_get = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            result = reg_data + READ_RING_SIZE;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32> >(read_unack_psn) read_unack_psn_set = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            result = reg_data;
            if (method_case == CASE_READ_RESPONSE) {
                reg_data = (bit<32>)hdr.bth.psn + 1;
            }
            else {
                reg_data = (bit<32>)hdr.bth.psn;
            }
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32> >(read_psn) read_psn_cmp_and_inc = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            result = reg_data;
            if (reg_data < tmp_read_ring_head) {
                reg_data = reg_data + 1;
            }
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<8> >(read_psn) read_psn_set = {
        void apply(inout bit<32> reg_data, out bit<8> result) {
            reg_data = tmp_read_ring_head;
        }
    };

    
    RegisterAction<bit<16>, bit<16>, bit<16> >(read_psn_to_item) read_psn_to_item_action = {
        void apply(inout bit<16> reg_data, out bit<16> result) {
            result = reg_data;
            if (method_case == CASE_REPL_ONLY) {
                reg_data = hdr.repl.item_id;
            }
        }
    };
    
    RegisterAction<bit<32>, bit<16>, bit<32> >(item_write_offset) item_write_offset_action = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            result = reg_data;
            if (method_case == CASE_REPL_ONLY) {
                reg_data = hdr.item0.write_off;
            }
        }
    };

    RegisterAction<bit<32>, bit<16>, bit<32> >(write_psn) write_psn_action = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            result = reg_data;
            if (method_case == CASE_READ_RESPONSE) {
                reg_data = reg_data + 1;
            }
        }
    };

    RegisterAction<bit<16>, bit<16>, bit<16> >(write_psn_to_unit) write_psn_to_unit_action = {
        void apply(inout bit<16> reg_data, out bit<16> result) {
            result = reg_data;
            if (method_case == CASE_READ_RESPONSE) {
                reg_data = unit_id;
            }
        }
    };

    action test_action() {
        dst_id = hdr.ipv4.identification;
        bit<16> imm1 = dst_id << ENDP_SHIFT;
        bit<16> imm2 = hdr.bth.psn[15:0] & ((1<<UNIT_SHIFT+ITEM_SHIFT)-1);
        unit_id = imm1 | imm2;
    }

    apply {
        /* test_action();
        if (unit_id == 0x1234) {
            drop();
        }
        else {
            l2_route.apply();
        } */
        if (hdr.bth.isValid()) {
            roce_method_tbl.apply();
        }
        else if (hdr.repl.isValid()) {
            if (hdr.repl.item_cnt == 1) {
                rx_repl_only_tbl.apply();
            }
            else {
                method_case = CASE_REPL_MANY;
            }

            if ((hdr.repl.flag & REPL_FLAG_SETSTATE) != 0) {
                update_endp_state = true;
                tmp_endp_state = 1;
            }
            hdr.repl.flag = 0;
        }

        endp_state_action.execute(dst_id);

        if (method_case == CASE_REPL_ONLY) {
            if (tmp_endp_state == 1) {
                hdr.ipv4.setValid();
                hdr.udp.setValid();
                hdr.bth.setValid();
                hdr.reth.setValid();

                tmp_read_ring_head = read_unack_psn_get.execute(src_dst_id);
                hdr.bth.psn = (bit<24>)(read_psn_cmp_and_inc.execute(src_dst_id));

                if (hdr.bth.psn == (bit<24>)tmp_read_ring_head) {
                    // read psn ring is full
                    // loopback?
                    tx_loopback_tbl.apply();
                }
                else {
                    calculate_read_ring_id_hi();
                    bit<16> ring_id_tail = hdr.bth.psn[15:0] & ((1<<READ_RING_SHIFT)-1);
                    calculate_ring_id_lo(ring_id_tail);

                    bit<16> item_id = read_psn_to_item_action.execute(ring_id);

                    item_write_offset_action.execute(item_id);

                    tx_src_read_tbl.apply();

                    hdr.repl.setInvalid();
                    hdr.item0.setInvalid();
                }
            }
            else {
                drop();
            }
        }
        else if (method_case == CASE_REPL_MANY) {
            if (tmp_endp_state == 1) {
                tx_repl_many_tbl.apply();
            }
        }
        else if (method_case == CASE_REQUEST) {

            calculate_unit_id_hi();
            bit<16> unit_id_tail = hdr.bth.psn[15:0] & ((1<<UNIT_SHIFT)-1);
            calculate_unit_id_lo(unit_id_tail);
            
            bit<32> psn_minus_req_epsn = req_epsn_action.execute(dst_id);
            bit<16> unit_minus_unack = req_unack_unit_sub.execute(dst_id);
            
            if (psn_minus_req_epsn[31:24] != 0) {
                // out-of-order request, drop
                drop();
            }
            else if (unit_minus_unack[15:15] != 0) {
                // duplicate request, send ack directly
                hdr.reth.setInvalid();
                hdr.aeth.setValid();

                hdr.aeth.msn = (bit<24>)req_msn_action.execute(dst_id);
                tx_req_ack_tbl.apply();
            }
            else if (unit_minus_unack == (UNIT_PER_ENDP-1)) {
                // run out of unit
                drop();
            }
            else {
                hdr.repl.setValid();
                calculate_item_cnt();

                if (psn_minus_req_epsn == 0) {
                    tmp_msn = req_msn_action.execute(dst_id);
                    tmp_dst_addr[63:32] = req_dst_addr_hi_action.execute(dst_id);
                    tmp_dst_addr[31:0] = req_dst_addr_lo_action.execute(dst_id);

                    unit_req_psn_action.execute(unit_id);
                    unit_req_msn_action.execute(unit_id);
                    unit_dst_addr_hi_action.execute(unit_id);
                    unit_dst_addr_lo_action.execute(unit_id);
                    unit_remain_set.execute(unit_id);
                }
                else {
                    // retry packet
                    unit_remain_set.execute(unit_id);
                    hdr.repl.flag = REPL_FLAG_SETSTATE;
                }
                hdr.repl.item_id = unit_id << ITEM_SHIFT;

                hdr.ipv4.setInvalid();
                hdr.udp.setInvalid();
                hdr.bth.setInvalid();
                hdr.reth.setInvalid();

                if (hdr.repl.item_cnt == 1) {
                    tx_repl_only_tbl.apply();
                }
                else {
                    tx_repl_many_tbl.apply();
                }
            }
        }
        else if (method_case == CASE_READ_RESPONSE) {
            calculate_read_ring_id_hi();
            bit<16> ring_id_tail = hdr.bth.psn[15:0] & ((1<<READ_RING_SHIFT)-1);
            calculate_ring_id_lo(ring_id_tail);

            bit<24> tmp_unack_psn = (bit<24>)read_unack_psn_set.execute(src_dst_id);
            if (tmp_endp_state == 1 && tmp_unack_psn == hdr.bth.psn) {
                bit<16> item_id = read_psn_to_item_action.execute(ring_id);
                hdr.bth.psn = (bit<24>)write_psn_action.execute(dst_id);

                calculate_unit_id_from_item_id(item_id);

                calculate_write_ring_id_hi();
                bit<16> write_tail = hdr.bth.psn[15:0] & ((1<<WRITE_RING_SHIFT)-1);
                calculate_ring_id_lo(write_tail);

                write_psn_to_unit_action.execute(ring_id);

                tmp_dst_addr[63:32] = unit_dst_addr_hi_action.execute(unit_id);
                tmp_dst_addr[31:0] = unit_dst_addr_lo_action.execute(unit_id);
                tmp_write_offset[63:32] = 0;
                tmp_write_offset[31:0] = item_write_offset_action.execute(item_id);

                hdr.bth.ackreq_rsv = unit_remain_dec.execute(unit_id);

                hdr.reth.setValid();

                tx_dst_write_tbl.apply();

                hdr.aeth.setInvalid();
            }
        }
        else if (method_case == CASE_READ_NAK) {
            tmp_read_ring_head = (bit<32>)hdr.bth.psn;
            read_unack_psn_set.execute(src_dst_id);
            read_psn_set.execute(src_dst_id);
            drop();
        }
        else if (method_case == CASE_WRITE_ACK) {
            calculate_write_ring_id_hi();
            bit<16> write_tail = hdr.bth.psn[15:0] & ((1<<WRITE_RING_SHIFT)-1);
            calculate_ring_id_lo(write_tail);

            unit_id = write_psn_to_unit_action.execute(ring_id);

            bit<8> ret_req_unack_unit_inc = req_unack_unit_inc.execute(dst_id);
            if (ret_req_unack_unit_inc != 0 || tmp_endp_state == 0) {
                drop();
            }
            else {
                hdr.aeth.msn = (bit<24>)unit_req_msn_action.execute(unit_id);
                hdr.bth.psn = (bit<24>)unit_req_psn_action.execute(unit_id);
                tx_req_ack_tbl.apply();
            }
        }
        else if (method_case == CASE_WRITE_NAK) {
            write_psn_action.execute(dst_id);
        }

    }
}
