#include "shuffle_header.p4"
#include "shuffle_ingress_parser.p4"

control read_unack_psn_ctl(
        in bool is_set,
        in bit<16> ind,
        inout bit<32> val)(
        bit<32> reg_size
        ) {
    
    Register<bit<32>, bit<16> >(reg_size) read_unack_psn;

    RegisterAction<bit<32>, bit<16>, bit<32> >(read_unack_psn) read_unack_psn_action = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            if (is_set) {
                reg_data = val;
            }
            result = reg_data;
        }
    };
    apply {
        val = read_unack_psn_action.execute(ind);
    }
}

control read_psn_ctl(
        in bool is_acc,
        in bool is_set,
        in bit<8> endp_state,
        in bit<16> ind,
        inout bit<32> val)(
        bit<32> reg_size
        ) {
    
    Register<bit<32>, bit<32> >(reg_size) read_psn;
    
    RegisterAction<bit<32>, bit<16>, bit<32> >(read_psn) read_psn_cmp_and_inc = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            result = reg_data;
            reg_data = reg_data + 1;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32> >(read_psn) read_psn_set = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            reg_data = val;
            result = reg_data;
        }
    };
    action read_psn_cmp_and_inc_action() {
        val = read_psn_cmp_and_inc.execute(ind);
    }
    action read_psn_set_action() {
        val = read_psn_set.execute(ind);
    }

    table read_psn_tbl {
        key = {
            is_acc : exact;
            is_set : exact;
            endp_state : exact;
        }
        actions = {
            read_psn_cmp_and_inc_action;
            read_psn_set_action;
            NoAction;
        }
        size = 16;
        default_action = NoAction;
        const entries = {
            (false, false, 0) : NoAction();
            (false, false, 1) : NoAction();
            (false, true, 0) : read_psn_set_action(); // impossible
            (false, true, 1) : read_psn_set_action(); // impossible
            (true, false, 0) : NoAction();
            (true, false, 1) : read_psn_cmp_and_inc_action();
            (true, true, 0) : read_psn_set_action();
            (true, true, 1) : read_psn_set_action();
        }
    }

    apply {
        read_psn_tbl.apply();
    }
}

control read_psn_to_item_ctl(
        in bool is_set,
        in bit<16> ind,
        inout bit<16> val)(
        bit<32> reg_size
        ) {
    
    Register<bit<16>, bit<16> >(reg_size) read_psn_to_item;

    RegisterAction<bit<16>, bit<16>, bit<16> >(read_psn_to_item) read_psn_to_item_action = {
        void apply(inout bit<16> reg_data, out bit<16> result) {
            if (is_set) {
                reg_data = val;
            }
            result = reg_data;
        }
    };
    apply {
        val = read_psn_to_item_action.execute(ind);
    }
}

control write_psn_ctl(
        in bool is_acc,
        in bool is_set,
        in bit<8> endp_state,
        in bit<16> ind,
        inout bit<32> val)(
        bit<32> reg_size
        ) {
    
    Register<bit<32>, bit<32> >(reg_size) write_psn;

    RegisterAction<bit<32>, bit<16>, bit<32> >(write_psn) write_psn_inc = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            result = reg_data;
            reg_data = reg_data + 1;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32> >(write_psn) write_psn_set = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            reg_data = val;
            result = reg_data;
        }
    };
    action write_psn_inc_action() {
        val = write_psn_inc.execute(ind);
    }
    action write_psn_set_action() {
        val = write_psn_set.execute(ind);
    }

    table write_psn_tbl {
        key = {
            is_acc : exact;
            is_set : exact;
            endp_state : exact;
        }
        actions = {
            write_psn_inc_action;
            write_psn_set_action;
            NoAction;
        }
        size = 16;
        default_action = NoAction;
        const entries = {
            (false, false, 0) : NoAction();
            (false, false, 1) : NoAction();
            (false, true, 0) : write_psn_set_action(); // impossible
            (false, true, 1) : write_psn_set_action(); // impossible
            (true, false, 0) : NoAction();
            (true, false, 1) : write_psn_inc_action();
            (true, true, 0) : write_psn_set_action();
            (true, true, 1) : write_psn_set_action();
        }
    }

    apply {
        write_psn_tbl.apply();
    }
}

control get_read_ring_id_ctl (
            in bit<16> src_dst_id,
            in bit<(READ_RING_SHIFT)> read_psn_out,
            out bit<16> read_ring_id)(
            bit<32> tbl_size) {
    action set_read_ring_id(bit<16> _read_ring_id) {
        read_ring_id = _read_ring_id;
    }

    table get_read_ring_id_tbl {
        key = {
            src_dst_id : exact;
            read_psn_out : exact;
        }
        actions = {
            set_read_ring_id;
            NoAction;
        }
        size = tbl_size;
        default_action = NoAction;
    }

    apply {
        get_read_ring_id_tbl.apply();
    }
}

control check_read_ring_ctl (
            in bit<32> read_unack_psn_val,
            in bit<32> read_psn_val,
            inout bit<8> rx_case,
            inout bool read_psn_to_item_set)(
            bit<32> tbl_size) {
    bit<32> sub_val;

    action get_sub_val() {
        sub_val = read_psn_val - read_unack_psn_val;
    }

    action read_ring_full() {
        rx_case = CASE_DROP;
        read_psn_to_item_set = false;
    }
    
    table check_read_ring_tbl {
        key = {
            sub_val : ternary;
        }
        actions = {
            read_ring_full;
            NoAction;
        }
        size = tbl_size;
        default_action = NoAction;
    }

    apply {
        get_sub_val();
        check_read_ring_tbl.apply();
    }
}

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
    bit<16> item_id = 0;
    bit<8> item_cnt = 0;

    bit<32> hdr_bth_psn = 0;
    bit<32> read_unack_psn_val = 0;
    bit<32> read_psn_val = 0;
    bit<32> write_psn_val = 0;
    
    bit<16> hdr_bridge_flag = 0;

    // method case
    bit<8> rx_case = CASE_BYPASS;

    bool update_endp_state = false;

    // temporary variables
    bit<8> new_endp_state = 0;

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
    
    action tx_repl_loopback_action(PortId_t port) {
        ig_tm_md.ucast_egress_port = port;

        hdr.mirror_bridge.setValid();
        hdr.mirror_bridge.pkt_type = PKT_TYPE_BYPASS;
    }

    action tx_request_loopback_action(PortId_t port) {
        ig_tm_md.ucast_egress_port = port;

        hdr.bridge.setValid();
        hdr.bridge.pkt_type = PKT_TYPE_BRIDGE;
        hdr.bridge.rx_case = rx_case;
        hdr.bridge.flag = hdr_bridge_flag;
        hdr.bridge.dst_id = dst_id;
        hdr.bridge.unit_id = unit_id;
        hdr.bridge.item_cnt = item_cnt;
        hdr.bridge.item_id = item_id;
    }

    table tx_repl_loopback_tbl {
        key = {
            hdr.eth.src_addr : exact;
        }
        actions = {
            tx_repl_loopback_action;
            drop;
        }
        size = 512;
        default_action = drop;
    }
    table tx_request_loopback_tbl {
        key = {
            hdr.eth.src_addr : exact;
        }
        actions = {
            tx_request_loopback_action;
            drop;
        }
        size = 512;
        default_action = drop;
    }

    bool read_unack_psn_set = false;
    bool read_psn_acc = false;
    bool read_psn_set = false;

    bool read_psn_to_item_set = false;
    bool write_psn_acc = false;
    bool write_psn_set = false;

    bool unit_req_psn_set = false;

    Register<bit<8>, bit<32> >(MAX_NUM_ENDP) endp_state;

    read_unack_psn_ctl(MAX_NUM_ENDP * MAX_NUM_ENDP) read_unack_psn;
    read_psn_ctl(MAX_NUM_ENDP * MAX_NUM_ENDP) read_psn;

    read_psn_to_item_ctl(MAX_NUM_ENDP * MAX_NUM_ENDP * READ_RING_SIZE) read_psn_to_item;
    write_psn_ctl(MAX_NUM_ENDP) write_psn;
    check_read_ring_ctl(32) check_read_ring;

    action add_by_32(inout bit<32> a, in bit<32> b) {
        a = a + b;
    }

    action rx_request_common(bit<16> _dst_id) {
        dst_id = _dst_id;   // based on smac
        
        rx_case = CASE_REQUEST;

        // remove ackreq
        hdr.bth.psn = hdr.bth.psn & 0x00ffffff;
    }

    action rx_request_first(bit<16> _dst_id) {
        rx_request_common(_dst_id);

        hdr_bridge_flag = BRI_FLAG_REQ_EPSN_ACC |
                          BRI_FLAG_UNIT_DST_ADDR_SET |
                          BRI_FLAG_UNIT_REQ_PSN_SET |
                          BRI_FLAG_UNIT_REQ_MSN_SET |
                          BRI_FLAG_UNIT_REMAIN_ACC |
                          BRI_FLAG_UNIT_REMAIN_SET |
                          BRI_FLAG_REQ_DST_ADDR_SET;
    }

    action rx_request_middle(bit<16> _dst_id) {
        rx_request_common(_dst_id);

        hdr_bridge_flag = BRI_FLAG_REQ_EPSN_ACC |
                          BRI_FLAG_UNIT_DST_ADDR_SET |
                          BRI_FLAG_UNIT_REQ_PSN_SET |
                          BRI_FLAG_UNIT_REQ_MSN_SET |
                          BRI_FLAG_UNIT_REMAIN_ACC |
                          BRI_FLAG_UNIT_REMAIN_SET;
    }

    action rx_request_last(bit<16> _dst_id) {
        rx_request_common(_dst_id);

        hdr_bridge_flag = BRI_FLAG_REQ_EPSN_ACC |
                          BRI_FLAG_UNIT_DST_ADDR_SET |
                          BRI_FLAG_UNIT_REQ_PSN_SET |
                          BRI_FLAG_UNIT_REQ_MSN_SET |
                          BRI_FLAG_UNIT_REMAIN_ACC |
                          BRI_FLAG_UNIT_REMAIN_SET |
                          BRI_FLAG_REQ_MSN_INC;
    }

    action rx_request_only(bit<16> _dst_id) {
        rx_request_common(_dst_id);

        hdr_bridge_flag = BRI_FLAG_REQ_EPSN_ACC |
                          BRI_FLAG_UNIT_DST_ADDR_SET |
                          BRI_FLAG_UNIT_REQ_PSN_SET |
                          BRI_FLAG_UNIT_REQ_MSN_SET |
                          BRI_FLAG_UNIT_REMAIN_ACC |
                          BRI_FLAG_UNIT_REMAIN_SET |
                          BRI_FLAG_REQ_MSN_INC |
                          BRI_FLAG_REQ_DST_ADDR_SET;
    }

    action rx_read_response(bit<16> _dst_id, bit<16> _src_dst_id) {
        dst_id = _dst_id;   // based on dqpn
        src_dst_id = _src_dst_id;   // based on smac

        rx_case = CASE_READ_RESPONSE;

        read_unack_psn_set = true;
        read_unack_psn_val = hdr.bth.psn + 1;
        read_psn_to_item_set = false;
        write_psn_acc = true;
        write_psn_set = false;
    }

    action rx_read_nak(bit<16> _dst_id, bit<16> _src_dst_id) {
        dst_id = _dst_id;   // based on dqpn
        src_dst_id = _src_dst_id;   // based on smac

        rx_case = CASE_READ_NAK;
        update_endp_state = true;
        new_endp_state = 0;

        read_unack_psn_set = true;
        read_unack_psn_val = hdr.bth.psn;
        read_psn_acc = true;
        read_psn_set = true;
    }

    action rx_write_ack(bit<16> _dst_id) {
        dst_id = _dst_id;   // based on smac

        rx_case = CASE_WRITE_ACK;
    }

    table roce_case_tbl {
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

    action rx_repl_only_action(bit<16> _dst_id, bit<16> _src_dst_id) {
        dst_id = _dst_id;   // based on smac
        src_dst_id = _src_dst_id;   // from item

        rx_case = CASE_REPL_ONLY;
        read_unack_psn_set = false;
        read_psn_acc = true;
        read_psn_set = false;
        read_psn_to_item_set = true;
    }

    table rx_repl_only_tbl {
        key = {
            hdr.eth.src_addr : exact;
            hdr.item0.src_id : exact;
        }
        actions = {
            rx_repl_only_action;
            NoAction;
        }
        size = 512;
        default_action = NoAction;
    }

    action tx_repl_many_action(PortId_t port, MirrorId_t sess) {
        // item0 in the mirror packet
        ig_tm_md.ucast_egress_port = port;

        ig_dprsr_md.mirror_type = MIRROR_TYPE_I2E;

        hdr.mirror_bridge.setValid();
        hdr.mirror_bridge.pkt_type = PKT_TYPE_BYPASS;

        ig_md.sess = sess;
        ig_md.mirror.pkt_type = PKT_TYPE_MIRROR;

        // item1... in the original packet
        // hdr.eth.ether_type = ETHERTYPE_REPL;
        hdr.repl.item_id = hdr.repl.item_id + 1;
        hdr.repl.item_cnt = hdr.repl.item_cnt - 1;
        
        hdr.item0.setInvalid();
    }
    
    table tx_repl_many_tbl {
        key = {
            hdr.eth.src_addr : exact;
        }
        actions = {
            tx_repl_many_action;
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

        hdr.bridge.setValid();
        hdr.bridge.pkt_type = PKT_TYPE_BRIDGE;
        hdr.bridge.rx_case = rx_case;
        hdr.bridge.flag = BRI_FLAG_REQ_UNACK_UNIT_SET;
        hdr.bridge.dst_id = dst_id;
        hdr.bridge.unit_id = unit_id;
        hdr.bridge.item_cnt = item_cnt;
        hdr.bridge.item_id = item_id;

        hdr.eth.src_addr = src_mac;
        hdr.eth.dst_addr = dst_mac;
        // hdr.eth.ether_type = ETHERTYPE_IPV4;

        // hdr.ipv4.ver_ihl = 0x45;
        // hdr.ipv4.diffserv = 0x02;
        hdr.ipv4.total_len = 48; // ACK
        // hdr.ipv4.identification = 0x1234;
        // hdr.ipv4.flag_offset = 0x4000;
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

        // hdr.aeth.syndrome = 0;
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

    get_read_ring_id_ctl(MAX_NUM_ENDP*MAX_NUM_ENDP*READ_RING_SIZE) get_read_ring_id;
    
    RegisterAction<bit<8>, bit<16>, bit<8> >(endp_state) endp_state_action = {
        void apply(inout bit<8> reg_data, out bit<8> result) {
            if (update_endp_state) {
                reg_data = new_endp_state;
            }
            result = reg_data;
        }
    };

    Register<bit<32>, bit<32> >(100) ingress_counter;

    RegisterAction<bit<32>, bit<32>, bit<32> >(ingress_counter) ingress_counter_inc = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            reg_data = reg_data + 1;
        }
    };

    action set_item_cnt(bit<8> _item_cnt) {
        item_cnt = _item_cnt;
    }

    // FIRST ONLY : hdr_length - 40
    // MIDDLE LAST : hdr_length - 24
    table get_item_cnt_tbl {
        key = {
            hdr.udp.hdr_length : exact;
            hdr.bth.opcode : exact;
        }
        actions = {
            set_item_cnt;
            NoAction;
        }
        size = 512;
        default_action = NoAction;
    }

    action set_unit_id(bit<16> _unit_id) {
        unit_id = _unit_id;
    }

    action set_unit_id_item_id(bit<16> _unit_id, bit<16> _item_id) {
        unit_id = _unit_id;
        item_id = _item_id;
    }

    // KEY!!!
    // Do not use hdr.bth.psn directly
    table get_unit_id_from_dst_tbl {
        key = {
            dst_id : exact;
            hdr.bth.psn : ternary;
        }
        actions = {
            set_unit_id_item_id;
            NoAction;
        }
        size = 512;
        default_action = NoAction;
    }

    table get_unit_id_from_item_tbl {
        key = {
            item_id : ternary;
        }
        actions = {
            set_unit_id;
            NoAction;
        }
        size = MAX_NUM_ENDP*UNIT_PER_ENDP;
        default_action = NoAction;
    }
    
    action case_read_response() {}
    action case_repl_only() {}
    action case_repl_many() {}
    action case_repl_lpbk() {}
    action case_request() {}
    action case_write_ack() {}
    action case_write_nak() {}
    action case_read_nak() {}
    action case_bypass() {}
    action case_debug() {}
    action case_drop() {}

    table case_tbl {
        key = {
            rx_case : exact;
        }
        actions = {
            case_read_response;
            case_repl_only;
            case_repl_many;
            case_repl_lpbk;
            case_request;
            case_write_ack;
            case_write_nak;
            case_read_nak;
            case_bypass;
            case_debug;
            case_drop;
        }
        size = 16;
        default_action = case_bypass;
        const entries = {
            (CASE_READ_RESPONSE) : case_read_response();
            (CASE_REPL_ONLY) : case_repl_only();
            (CASE_REPL_MANY) : case_repl_many();
            (CASE_REPL_LPBK) : case_repl_lpbk();
            (CASE_REQUEST) : case_request();
            (CASE_WRITE_ACK) : case_write_ack();
            (CASE_WRITE_NAK) : case_write_nak();
            (CASE_READ_NAK) : case_read_nak();
            (CASE_BYPASS) : case_bypass();
            (CASE_DEBUG) : case_debug();
            (CASE_DROP) : case_drop();
        }
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

        hdr.bridge.setValid();
        hdr.bridge.pkt_type = PKT_TYPE_BRIDGE;
        hdr.bridge.rx_case = rx_case;
        hdr.bridge.flag = BRI_FLAG_ITEM_WRITE_OFFSET_SET;
        hdr.bridge.dst_id = dst_id;
        hdr.bridge.unit_id = unit_id;
        hdr.bridge.item_cnt = item_cnt;
        hdr.bridge.item_id = item_id;

        hdr.eth.src_addr = src_mac;
        hdr.eth.dst_addr = dst_mac;
        hdr.eth.ether_type = ETHERTYPE_IPV4;

        hdr.ipv4.ver_ihl = 0x45;
        hdr.ipv4.diffserv = 0x02;
        hdr.ipv4.total_len = 60;    // RoCEv2 READ 
        hdr.ipv4.identification = 0x1234;
        hdr.ipv4.flag_offset = 0x4000;
        hdr.ipv4.ttl = 64;
        hdr.ipv4.protocol = IP_PROTOCOLS_UDP;
        hdr.ipv4.hdr_checksum = 0x0000;
        hdr.ipv4.src_addr = src_ip;
        hdr.ipv4.dst_addr = dst_ip;

        hdr.udp.src_port = udp_src_port;
        hdr.udp.dst_port = UDP_PORT_ROCE;
        hdr.udp.hdr_length = 40;   // RoCEv2 READ 
        hdr.udp.checksum = 0x0000;

        // hdr.bth.opcode = RDMA_OP_READ_REQ;
        hdr.bth.se_migreq_pad_ver = 0x40;
        hdr.bth.pkey = 0xffff;
        // hdr.bth.f_b_rsv = 0;
        hdr.bth.dqpn = dqpn;
        // hdr.bth.ackreq_rsv = 0;
        hdr.bth.psn = read_psn_val;

        hdr.reth.setValid();
        hdr.reth.rkey = rkey;
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

    action tx_dst_write_action(PortId_t port,
                               mac_addr_t src_mac, 
                               mac_addr_t dst_mac, 
                               ipv4_addr_t src_ip, 
                               ipv4_addr_t dst_ip, 
                               bit<16> udp_src_port, 
                               bit<24> dqpn,
                               bit<32> rkey) {
        ig_tm_md.ucast_egress_port = port;

        hdr.bridge.setValid();
        hdr.bridge.pkt_type = PKT_TYPE_BRIDGE;
        hdr.bridge.rx_case = rx_case;
        hdr.bridge.flag = BRI_FLAG_UNIT_REMAIN_ACC | BRI_FLAG_WRITE_PSN_TO_UNIT_SET;
        hdr.bridge.dst_id = dst_id;
        hdr.bridge.unit_id = unit_id;
        hdr.bridge.item_cnt = item_cnt;
        hdr.bridge.item_id = item_id;

        hdr.reth.setValid();
        hdr.aeth.setInvalid();
        
        hdr.reth.len_hi = 0;
        hdr.reth.len_lo = hdr.udp.hdr_length - 28;

        hdr.eth.src_addr = src_mac;
        hdr.eth.dst_addr = dst_mac;
        // hdr.eth.ether_type = ETHERTYPE_IPV4;

        // hdr.ipv4.ver_ihl = 0x45;
        // hdr.ipv4.diffserv = 0x02;
        hdr.ipv4.total_len = hdr.ipv4.total_len + 12; // WRITE(payload+60) - READ(payload+48)
        // hdr.ipv4.identification = 0x1234;
        // hdr.ipv4.flag_offset = 0x4000;
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
        hdr.bth.psn = write_psn_val;

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
    
    apply {
        if (hdr.bth.isValid()) {
            if (hdr.repl.isValid()) {
                if (hdr.repl.item_cnt == 1) {
                    rx_repl_only_tbl.apply();
                }
                else {
                    rx_case = CASE_REPL_MANY;
                }

                if ((hdr.repl.flag & REPL_FLAG_SETSTATE) != 0) {
                    update_endp_state = true;
                    new_endp_state = 1;
                }
                item_id = hdr.repl.item_id;
                item_cnt = hdr.repl.item_cnt;
                hdr.repl.flag = 0;
            }
            else {
                hdr_bth_psn = hdr.bth.psn;
                switch (roce_case_tbl.apply().action_run) {
                    rx_write_ack : {
                        if (hdr.aeth.syndrome[6:5] != 0) {
                            rx_case = CASE_WRITE_NAK;
                            // reset metadata from WRITE_ACK
                            update_endp_state = true;
                            new_endp_state = 0;
                            write_psn_acc = true;
                            write_psn_set = true;
                        }
                    }
                }

                get_unit_id_from_dst_tbl.apply();
                get_item_cnt_tbl.apply();
            }
        }
        bit<8> cur_endp_state = endp_state_action.execute(dst_id);

        read_unack_psn.apply(read_unack_psn_set, src_dst_id, read_unack_psn_val);

        read_psn_val = hdr_bth_psn;
        read_psn.apply(read_psn_acc, read_psn_set, cur_endp_state, src_dst_id, read_psn_val);

        if (rx_case == CASE_REPL_ONLY) {
            check_read_ring.apply(read_unack_psn_val, read_psn_val, rx_case, read_psn_to_item_set);
        }

        // read PSN ring id
        bit<16> read_ring_id = 0;
        get_read_ring_id.apply(src_dst_id, read_psn_val[READ_RING_SHIFT-1:0], read_ring_id);

        if (cur_endp_state == 1) {
            read_psn_to_item.apply(read_psn_to_item_set, read_ring_id, item_id);
        }
        if (rx_case == CASE_READ_RESPONSE) {
            get_unit_id_from_item_tbl.apply();
        }

        write_psn_val = hdr_bth_psn;
        write_psn.apply(write_psn_acc, write_psn_set, cur_endp_state, dst_id, write_psn_val);
        
        switch (case_tbl.apply().action_run) {
            case_read_response : {
                tx_dst_write_tbl.apply();

                if (cur_endp_state == 0) {
                    drop();
                }
            }
            case_repl_lpbk : {
                tx_repl_loopback_tbl.apply();
            }
            case_repl_only : {
                tx_src_read_tbl.apply();

                if (cur_endp_state == 0) {
                    drop();
                }
            }
            case_repl_many : {
                tx_repl_many_tbl.apply();
            }
            case_read_nak : {
                drop();
            }
            case_write_nak : {
                drop();
            }
            case_request : {
                tx_request_loopback_tbl.apply();
            }
            case_write_ack : {
                tx_req_ack_tbl.apply();
            }
            case_bypass : {
                l2_route.apply();
                hdr.mirror_bridge.setValid();
                hdr.mirror_bridge.pkt_type = PKT_TYPE_BYPASS;
            }
            case_debug : {
                hdr.mirror_bridge.setValid();
                hdr.mirror_bridge.pkt_type = PKT_TYPE_BYPASS;
            }
            case_drop : {
                drop();
            }
        }
    }
}
