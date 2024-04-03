#include "shuffle_header.p4"

parser ShuffleEgressParser(packet_in        pkt,
    /* User */
    out eg_header_t            hdr,
    out eg_metadata_t          eg_md,
    /* Intrinsic */
    out egress_intrinsic_metadata_t  eg_intr_md)
{
    /* This is a mandatory state, required by Tofino Architecture */
    state start {
        pkt.extract(eg_intr_md);
        eg_md.mirror_bridge = pkt.lookahead<mirror_bridged_metadata_h>();
        transition select(eg_md.mirror_bridge.pkt_type) {
            PKT_TYPE_BYPASS : parse_mirror_bridge;
            PKT_TYPE_BRIDGE : parse_bridge;
            PKT_TYPE_MIRROR : parse_mirror;
            default : accept;
        }
    }

    state parse_mirror_bridge {
        pkt.extract(hdr.mirror_bridge);
        transition accept;
    }

    state parse_mirror {
        pkt.extract(hdr.mirror);
        pkt.extract(hdr.eth);
        pkt.extract(hdr.ipv4);
        pkt.extract(hdr.udp);
        pkt.extract(hdr.bth);
        pkt.extract(hdr.repl);
        pkt.extract(hdr.item0);
        transition accept;
    }

    state parse_bridge {
        pkt.extract(hdr.bridge);
        pkt.extract(hdr.eth);
        pkt.extract(hdr.ipv4);
        pkt.extract(hdr.udp);
        pkt.extract(hdr.bth);
        transition select(hdr.bth.opcode) {
            RDMA_OP_SEND_FIRST: accept;
            RDMA_OP_SEND_MIDDLE: accept;
            RDMA_OP_SEND_LAST: accept;
            RDMA_OP_SEND_LAST_WITH_IMM : accept;
            RDMA_OP_SEND_ONLY: accept;
            RDMA_OP_SEND_ONLY_WITH_IMM : accept;
            RDMA_OP_WRITE_FIRST: parse_reth;
            RDMA_OP_WRITE_MIDDLE: accept;
            RDMA_OP_WRITE_LAST: accept; 
            RDMA_OP_WRITE_LAST_WITH_IMM: accept;
            RDMA_OP_WRITE_ONLY: parse_reth;
            RDMA_OP_WRITE_ONLY_WITH_IMM: accept;
            RDMA_OP_READ_REQ: parse_reth;
            RDMA_OP_READ_RES_FIRST: parse_aeth;
            RDMA_OP_READ_RES_MIDDLE: accept;
            RDMA_OP_READ_RES_LAST: parse_aeth;
            RDMA_OP_READ_RES_ONLY: parse_aeth;
            RDMA_OP_ACK: parse_aeth;
            RDMA_OP_CNP: accept;
            RDMA_OP_REPL: parse_repl;
            default : accept;
        }
    }
    
    state parse_repl {
        pkt.extract(hdr.reth);
        pkt.extract(hdr.repl);
        pkt.extract(hdr.item0);
        transition accept;
    }

    state parse_reth {
        pkt.extract(hdr.reth);
        transition accept;
    }

    state parse_aeth {
        pkt.extract(hdr.aeth);
        // pkt.extract(hdr.icrc);
        transition accept;
    }
}

control ShuffleEgressDeparser(packet_out pkt,
    /* User */
    inout eg_header_t                       hdr,
    in    eg_metadata_t                      eg_md,
    /* Intrinsic */
    in    egress_intrinsic_metadata_for_deparser_t  eg_dprsr_md)
{
    apply {
        pkt.emit(hdr);
    }
}
