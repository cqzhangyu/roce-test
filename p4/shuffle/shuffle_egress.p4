#include "shuffle_header.p4"

control write_psn_to_unit_ctl(
        in bool is_set,
        in bit<16> ind,
        inout bit<16> val)(
        bit<32> reg_size
        ) {
    
    Register<bit<16>, bit<16> >(reg_size) write_psn_to_unit;

    RegisterAction<bit<16>, bit<16>, bit<16> >(write_psn_to_unit) write_psn_to_unit_action = {
        void apply(inout bit<16> reg_data, out bit<16> result) {
            if (is_set) {
                reg_data = val;
            }
            result = reg_data;
        }
    };
    apply {
        val = write_psn_to_unit_action.execute(ind);
    }
}

control item_write_offset_ctl(
        in bool is_set,
        in bit<16> ind,
        inout bit<32> val)(
        bit<32> reg_size
        ) {
    
    Register<bit<32>, bit<32> >(reg_size) item_write_offset;

    RegisterAction<bit<32>, bit<16>, bit<32> >(item_write_offset) item_write_offset_action = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            if (is_set) {
                reg_data = val;
            }
            result = reg_data;
        }
    };
    apply {
        val = item_write_offset_action.execute(ind);
    }
}

control req_unack_unit_ctl (
        in bool is_set,
        in bit<16> ind,
        in bit<16> in_val,
        out bit<16> out_val)(
        bit<32> reg_size
        ) {
    
    Register<bit<16>, bit<16> >(reg_size) req_unack_unit;

    RegisterAction<bit<16>, bit<16>, bit<16> >(req_unack_unit) req_unack_unit_sub = {
        void apply(inout bit<16> reg_data, out bit<16> result) {
            result = reg_data - in_val;
        }
    };
    RegisterAction<bit<16>, bit<16>, bit<16> >(req_unack_unit) req_unack_unit_inc = {
        void apply(inout bit<16> reg_data, out bit<16> result) {
            if (reg_data == in_val) {
                reg_data = reg_data + (1<<SHL_UNIT_SHIFT);
                result = 0;
            }
            else {
                result = 1;
            }
        }
    };

    apply {
        if (is_set) {
            out_val = req_unack_unit_inc.execute(ind);
        }
        else {
            out_val = req_unack_unit_sub.execute(ind);
        }
    }
}

control req_epsn_ctl(
        in bit<16> ind,
        inout bit<32> val)(
        bit<32> reg_size
        ) {
    
    Register<bit<32>, bit<32> >(reg_size) req_epsn;

    RegisterAction<bit<32>, bit<16>, bit<32> >(req_epsn) req_epsn_action = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            result = reg_data - val;
            if (val == reg_data) {
                reg_data = reg_data + 1;
            }
        }
    };

    apply {
        val = req_epsn_action.execute(ind);
    }
}

control req_msn_ctl(
        in bool is_inc,
        in bit<16> ind,
        out bit<32> val)(
        bit<32> reg_size
        ) {
    
    Register<bit<32>, bit<32> >(reg_size) req_msn;

    RegisterAction<bit<32>, bit<16>, bit<32> >(req_msn) req_msn_action = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            if (is_inc) {
                reg_data = reg_data + 1;
            }
            result = reg_data;
        }
    };
    apply {
        val = req_msn_action.execute(ind);
    }
}

control req_dst_addr_ctl(
        in bool is_set,
        in bit<16> ind,
        inout bit<64> val
        )
        (bit<32> reg_size) {
    
    Register<bit<32>, bit<32> >(reg_size) req_dst_addr_hi;
    Register<bit<32>, bit<32> >(reg_size) req_dst_addr_lo;

    RegisterAction<bit<32>, bit<16>, bit<32> >(req_dst_addr_hi) req_dst_addr_hi_action = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            if (is_set) {
                reg_data = val[63:32];
            }
            result = reg_data;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32> >(req_dst_addr_lo) req_dst_addr_lo_action = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            if (is_set) {
                reg_data = val[31:0];
            }
            result = reg_data;
        }
    };
    apply {
        val[63:32] = req_dst_addr_hi_action.execute(ind);
        val[31:0] = req_dst_addr_lo_action.execute(ind);
    }
}

control unit_req_psn_ctl(
        in bool is_set,
        in bit<16> ind,
        inout bit<32> val)(
        bit<32> reg_size
        ) {
    
    Register<bit<32>, bit<32> >(reg_size) unit_req_psn;

    RegisterAction<bit<32>, bit<16>, bit<32> >(unit_req_psn) unit_req_psn_action = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            if (is_set) {
                reg_data = val;
            }
            result = reg_data;
        }
    };
    apply {
        val = unit_req_psn_action.execute(ind);
    }
}

control unit_req_msn_ctl(
        in bool is_set,
        in bit<16> ind,
        inout bit<32> val)(
        bit<32> reg_size
        ) {
    
    Register<bit<32>, bit<32> >(reg_size) unit_req_msn;

    RegisterAction<bit<32>, bit<16>, bit<32> >(unit_req_msn) unit_req_msn_action = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            if (is_set) {
                reg_data = val;
            }
            result = reg_data;
        }
    };
    apply {
        val = unit_req_msn_action.execute(ind);
    }
}

control unit_dst_addr_ctl(
        in bool is_set,
        in bit<16> ind,
        inout bit<64> val
        )
        (bit<32> reg_size) {
    
    Register<bit<32>, bit<32> >(reg_size) unit_dst_addr_hi;
    Register<bit<32>, bit<32> >(reg_size) unit_dst_addr_lo;

    RegisterAction<bit<32>, bit<16>, bit<32> >(unit_dst_addr_hi) unit_dst_addr_hi_action = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            if (is_set) {
                reg_data = val[63:32];
            }
            result = reg_data;
        }
    };
    RegisterAction<bit<32>, bit<16>, bit<32> >(unit_dst_addr_lo) unit_dst_addr_lo_action = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            if (is_set) {
                reg_data = val[31:0];
            }
            result = reg_data;
        }
    };
    apply {
        val[63:32] = unit_dst_addr_hi_action.execute(ind);
        val[31:0] = unit_dst_addr_lo_action.execute(ind);
    }
}

control unit_remain_ctl(
        in bool is_set,
        in bit<16> ind,
        inout bit<8> val)(
        bit<32> reg_size
        ) {
    
    Register<bit<8>, bit<8> >(reg_size) unit_remain;

    RegisterAction<bit<8>, bit<16>, bit<8> >(unit_remain) unit_remain_action = {
        void apply(inout bit<8> reg_data, out bit<8> result) {
            if (is_set) {
                reg_data = val;
            }
            else {
                reg_data = reg_data - 1;
            }
            result = reg_data;
        }
    };
    apply {
        val = unit_remain_action.execute(ind);
    }
}

control request_case_ctl (
        in bit<32> req_epsn_val,
        in bit<16> req_unack_unit_out,
        inout bool req_msn_inc,
        inout bool req_dst_addr_set,
        inout bool unit_req_psn_set,
        inout bool unit_req_msn_set,
        inout bool unit_dst_addr_set,
        inout bool unit_remain_acc,
        inout bit<8> hdr_repl_flag,
        out bit<8> request_case_id)
        (bit<32> tbl_size) {

    action out_of_order_req() {
        req_msn_inc = false;
        req_dst_addr_set = false;
        unit_req_psn_set = false;
        unit_req_msn_set = false;
        unit_dst_addr_set = false;
        unit_remain_acc = false;

        request_case_id = 1;
    }

    action duplicate_req() {
        req_msn_inc = false;
        req_dst_addr_set = false;
        unit_req_psn_set = false;
        unit_req_msn_set = false;
        unit_dst_addr_set = false;
        unit_remain_acc = false;

        request_case_id = 2;
    }

    action run_out_of_unit() {
        // run out of unacknowledged units

        req_msn_inc = false;
        req_dst_addr_set = false;
        unit_req_psn_set = false;
        unit_req_msn_set = false;
        unit_dst_addr_set = false;
        unit_remain_acc = false;

        request_case_id = 3;
    }

    action retry_req() {
        req_msn_inc = false;
        req_dst_addr_set = false;
        unit_req_psn_set = false;
        unit_req_msn_set = false;
        unit_dst_addr_set = false;

        hdr_repl_flag = REPL_FLAG_SETSTATE;
        request_case_id = 4;
    }

    action correct_req() {
        request_case_id = 5;
    }

    table request_case_tbl {
        key = {
            req_epsn_val : ternary;//TODO change it
            req_unack_unit_out : ternary;
        }
        actions = {
            out_of_order_req;
            duplicate_req;
            run_out_of_unit;
            retry_req;
            correct_req;
        }
        size = tbl_size;
        default_action = retry_req;
    }
    apply {
        request_case_tbl.apply();
    }
}

control get_write_ring_id_ctl (
            in bit<16> dst_id,
            in bit<WRITE_RING_SHIFT> write_psn_out,
            out bit<16> write_ring_id)(
            bit<32> tbl_size) {
            
    action set_write_ring_id(bit<16> _write_ring_id) {
        write_ring_id = _write_ring_id;
    }

    table get_write_ring_id_tbl {
        key = {
            dst_id : exact;
            write_psn_out : exact;
        }
        actions = {
            set_write_ring_id;
            NoAction;
        }
        size = tbl_size;
        default_action = NoAction;
    }

    apply {
        get_write_ring_id_tbl.apply();
    }
}

control ShuffleEgress(
        inout eg_header_t hdr,
        inout eg_metadata_t eg_md,
        in egress_intrinsic_metadata_t eg_intr_md,
        in egress_intrinsic_metadata_from_parser_t eg_intr_md_from_prsr,
        inout egress_intrinsic_metadata_for_deparser_t eg_intr_dprs_md,
        inout egress_intrinsic_metadata_for_output_port_t eg_intr_oport_md) {

    Register<bit<32>, bit<16> >(100) egress_counter;

    RegisterAction<bit<32>, bit<16>, bit<32> >(egress_counter) egress_counter_inc = {
        void apply(inout bit<32> reg_data, out bit<32> result) {
            reg_data = reg_data + 1;
        }
    };

    bool req_unack_unit_set = false;
    bool req_epsn_acc = false;

    bool req_msn_inc = false;
    bool req_dst_addr_set = false;
    
    bool unit_dst_addr_set = false;
    bool unit_req_psn_set = false;
    bool unit_req_msn_set = false;
    bool unit_remain_set = false;
    bool unit_remain_acc = false;
    bool item_write_offset_set = false;
    bool write_psn_to_unit_set = false;

    bit<8> rx_case = 0;
    bit<16> dst_id = 0;
    bit<16> unit_id = 0;
    bit<8> item_cnt = 0;
    bit<16> item_id = 0;
    bit<16> shl_unit_id = 0;
    bit<32> hdr_bth_psn = 0;
    bit<32> item_write_offset_val = 0;

    action add_by_32(inout bit<32> a, in bit<32> b) {
        a = a + b;
    }
    action add_64(in bit<64> a, in bit<64> b, out bit<64> c) {
        c = a + b;
    }
    action or_by_32(inout bit<32> a, in bit<32> b) {
        a = a | b;
    }
    action shl_by_16(inout bit<16> a, in bit<16> b) {
        a = a << b;
    }

    action drop() {
        eg_intr_dprs_md.drop_ctl = 1;
    }

    item_write_offset_ctl(MAX_NUM_ENDP * UNIT_PER_ENDP * ITEM_PER_UNIT) item_write_offset;
    write_psn_to_unit_ctl(MAX_NUM_ENDP * UNIT_PER_ENDP * ITEM_PER_UNIT) write_psn_to_unit;

    req_unack_unit_ctl(MAX_NUM_ENDP) req_unack_unit;
    req_epsn_ctl(MAX_NUM_ENDP) req_epsn;

    req_msn_ctl(MAX_NUM_ENDP) req_msn;
    req_dst_addr_ctl(MAX_NUM_ENDP) req_dst_addr;
    
    unit_req_psn_ctl(MAX_NUM_ENDP * UNIT_PER_ENDP) unit_req_psn;
    unit_req_msn_ctl(MAX_NUM_ENDP * UNIT_PER_ENDP) unit_req_msn;
    unit_dst_addr_ctl(MAX_NUM_ENDP * UNIT_PER_ENDP) unit_dst_addr;
    unit_remain_ctl(MAX_NUM_ENDP * UNIT_PER_ENDP) unit_remain;

    get_write_ring_id_ctl(MAX_NUM_ENDP*WRITE_RING_SIZE) get_write_ring_id;
    
    action set_unit_id(bit<16> _unit_id) {
        unit_id = _unit_id;
    }

    table get_unit_id_from_shl_tbl {
        key = {
            shl_unit_id : exact;
        }
        actions = {
            set_unit_id;
            NoAction;
        }
        size = MAX_NUM_ENDP*UNIT_PER_ENDP;
        default_action = NoAction;
    }

    
    Hash<bit<32>>(HashAlgorithm_t.CRC32) hash_icrc;

    action calc_icrc() {
        bit<32> icrc_val = hash_icrc.get({
            64w0xffffffffffffffff,
            hdr.ipv4.ver_ihl,
            8w0xff, // hdr.ipv4.diffserv,
            hdr.ipv4.total_len,
            hdr.ipv4.identification,
            hdr.ipv4.flag_offset,
            8w0xff, // hdr.ipv4.ttl
            hdr.ipv4.protocol,
            16w0xffff, // hdr.ipv4.hdr_checksum
            hdr.ipv4.src_addr,
            hdr.ipv4.dst_addr,

            hdr.udp.src_port,
            hdr.udp.dst_port,
            hdr.udp.hdr_length,
            16w0xffff, // hdr.udp.checksum,

            hdr.bth.opcode,
            hdr.bth.se_migreq_pad_ver,
            hdr.bth.pkey,
            8w0xff,
            hdr.bth.dqpn,
            hdr.bth.psn,

            hdr.aeth.syndrome,
            hdr.aeth.msn
        });
        hdr.icrc.setValid();
        hdr.icrc = {icrc_val[7:0], icrc_val[15:8], icrc_val[23:16], icrc_val[31:24]};
    }

    action case_read_response() {}
    action case_repl_only() {}
    action case_request() {}
    action case_write_ack() {}
    action case_bypass() {}

    table case_tbl {
        key = {
            rx_case : exact;
        }
        actions = {
            case_read_response;
            case_repl_only;
            case_request;
            case_write_ack;
            case_bypass;
        }
        size = 8;
        default_action = case_bypass;
        const entries = {
            (CASE_READ_RESPONSE) : case_read_response();
            (CASE_REPL_ONLY) : case_repl_only();
            (CASE_REQUEST) : case_request();
            (CASE_WRITE_ACK) : case_write_ack();
            (CASE_BYPASS) : case_bypass();
        }
    }

    request_case_ctl(512) request_case;
    apply {
        if (eg_md.mirror_bridge.pkt_type == PKT_TYPE_BRIDGE) {
            rx_case = hdr.bridge.rx_case;
            dst_id = hdr.bridge.dst_id;
            unit_id = hdr.bridge.unit_id;
            item_cnt = hdr.bridge.item_cnt;
            item_id = hdr.bridge.item_id;
            shl_unit_id = unit_id;
            hdr_bth_psn = hdr.bth.psn;

            // WARNING!!! error-prone
            shl_by_16(shl_unit_id, SHL_UNIT_SHIFT);

            if (hdr.bridge.flag[BRI_FLAGBIT_REQ_UNACK_UNIT_SET:BRI_FLAGBIT_REQ_UNACK_UNIT_SET] == 1w1) {
                req_unack_unit_set = true;
            }
            if (hdr.bridge.flag[BRI_FLAGBIT_REQ_EPSN_ACC:BRI_FLAGBIT_REQ_EPSN_ACC] == 1w1) {
                req_epsn_acc = true;
            }
            if (hdr.bridge.flag[BRI_FLAGBIT_REQ_MSN_INC:BRI_FLAGBIT_REQ_MSN_INC] == 1w1) {
                req_msn_inc = true;
            }
            if (hdr.bridge.flag[BRI_FLAGBIT_REQ_DST_ADDR_SET:BRI_FLAGBIT_REQ_DST_ADDR_SET] == 1w1) {
                req_dst_addr_set = true;
            }
            if (hdr.bridge.flag[BRI_FLAGBIT_UNIT_DST_ADDR_SET:BRI_FLAGBIT_UNIT_DST_ADDR_SET] == 1w1) {
                unit_dst_addr_set = true;
            }
            if (hdr.bridge.flag[BRI_FLAGBIT_UNIT_REQ_MSN_SET:BRI_FLAGBIT_UNIT_REQ_MSN_SET] == 1w1) {
                unit_req_msn_set = true;
            }
            if (hdr.bridge.flag[BRI_FLAGBIT_UNIT_REQ_PSN_SET:BRI_FLAGBIT_UNIT_REQ_PSN_SET] == 1w1) {
                unit_req_psn_set = true;
            }
            if (hdr.bridge.flag[BRI_FLAGBIT_UNIT_REMAIN_SET:BRI_FLAGBIT_UNIT_REMAIN_SET] == 1w1) {
                unit_remain_set = true;
            }
            if (hdr.bridge.flag[BRI_FLAGBIT_UNIT_REMAIN_ACC:BRI_FLAGBIT_UNIT_REMAIN_ACC] == 1w1) {
                unit_remain_acc = true;
            }
            if (hdr.bridge.flag[BRI_FLAGBIT_ITEM_WRITE_OFFSET_SET:BRI_FLAGBIT_ITEM_WRITE_OFFSET_SET] == 1w1) {
                item_write_offset_set = true;
            }

            if (hdr.item0.isValid()) {
                item_write_offset_val = hdr.item0.write_off;
            }
            item_write_offset.apply(item_write_offset_set, item_id, item_write_offset_val);
            
            // write PSN ring id
            bit<16> write_ring_id = 0;
            get_write_ring_id.apply(dst_id, hdr_bth_psn[WRITE_RING_SHIFT-1:0], write_ring_id);
            
            // bit<16> write_psn_to_unit_val = shl_unit_id;
            write_psn_to_unit.apply(write_psn_to_unit_set, write_ring_id, shl_unit_id);
            if (rx_case == CASE_WRITE_ACK) {
                get_unit_id_from_shl_tbl.apply();
            }

            bit<16> req_unack_unit_out = 0;
            req_unack_unit.apply(req_unack_unit_set, dst_id, shl_unit_id, req_unack_unit_out);

            bit<32> req_epsn_val = 0;
            req_epsn_val = hdr_bth_psn;
            if (req_epsn_acc) {
                req_epsn.apply(dst_id, req_epsn_val);
            }

            bit<8> hdr_repl_flag = 0;
            bit<8> request_case_id = 0;
            if (rx_case == CASE_REQUEST) {
                request_case.apply(req_epsn_val,
                                   req_unack_unit_out,
                                   req_msn_inc,
                                   req_dst_addr_set,
                                   unit_req_psn_set,
                                   unit_req_msn_set,
                                   unit_dst_addr_set,
                                   unit_remain_acc,
                                   hdr_repl_flag,
                                   request_case_id);
            }

            bit<32> req_msn_out = 0;
            req_msn.apply(req_msn_inc, dst_id, req_msn_out);

            bit<64> req_dst_addr_val = 0;
            if (hdr.reth.isValid()) {
                req_dst_addr_val = hdr.reth.addr;
            }
            req_dst_addr.apply(req_dst_addr_set, dst_id, req_dst_addr_val);

            bit<32> unit_req_psn_val = 0;
            unit_req_psn_val = hdr_bth_psn;
            unit_req_psn.apply(unit_req_psn_set, unit_id, unit_req_psn_val);

            bit<32> unit_req_msn_val = req_msn_out;
            unit_req_msn.apply(unit_req_msn_set, unit_id, unit_req_msn_val);

            bit<64> unit_dst_addr_val = 0;
            if (hdr.reth.isValid()) {
                unit_dst_addr_val = hdr.reth.addr;
            }
            unit_dst_addr.apply(unit_dst_addr_set, unit_id, unit_dst_addr_val);

            bit<8> unit_remain_val = item_cnt;
            if (unit_remain_acc) {
                unit_remain.apply(unit_remain_set, unit_id, unit_remain_val);
            }

            switch (case_tbl.apply().action_run) {
                case_read_response : {
                    if (unit_remain_val == 0) {
                        or_by_32(hdr.bth.psn, 0x80000000);
                    }
                    bit<64> write_offset_64 = 0;
                    write_offset_64[63:32] = 0;
                    write_offset_64[31:0] = item_write_offset_val;
                    add_64(unit_dst_addr_val, write_offset_64, hdr.reth.addr);
                }
                case_repl_only : {
                    hdr.bth.opcode = RDMA_OP_READ_REQ;

                    hdr.reth.addr = hdr.item0.src_addr;
                    hdr.reth.len_hi = 0;
                    hdr.reth.len_lo = hdr.item0.len;

                    hdr.repl.setInvalid();
                    hdr.item0.setInvalid();
                }
                case_request : {
                    hdr.reth.setInvalid();

                    hdr.bth.opcode = RDMA_OP_REPL;
                    
                    hdr.repl.setValid();
                    hdr.repl.flag = hdr_repl_flag;
                    hdr.repl.item_id = item_id;
                    hdr.repl.item_cnt = item_cnt;
                }
                case_write_ack : {
                    hdr.bth.psn = unit_req_psn_val;
                    hdr.aeth.msn = unit_req_msn_val[23:0];

                    // calc_icrc();
                }
            }
            hdr.bridge.setInvalid();
        }
        else if (eg_md.mirror_bridge.pkt_type == PKT_TYPE_MIRROR) {
            hdr.repl.setValid();
            hdr.item0.setValid();

            hdr.repl.flag = 0;
            hdr.repl.item_cnt = 1;

            hdr.mirror.setInvalid();
        }
        else if (eg_md.mirror_bridge.pkt_type == PKT_TYPE_BYPASS) {
            hdr.mirror_bridge.setInvalid();
        }
    }
}
