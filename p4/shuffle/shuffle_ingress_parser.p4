#ifndef _SHUFFLE_INGRESS_PARSER_
#define _SHUFFLE_INGRESS_PARSER_

#include "shuffle_header.p4"
parser ShuffleIngressParser(
        packet_in pkt,
        out ig_header_t hdr,
        out ig_metadata_t ig_md,
        out ingress_intrinsic_metadata_t ig_intr_md) {
            

    state start {
        pkt.extract(ig_intr_md);
        hdr.mirror_bridge.setValid();
        hdr.mirror_bridge.pkt_type = PKT_TYPE_BYPASS;
        pkt.extract(hdr.eth);
        transition select(hdr.eth.ether_type) {
            ETHERTYPE_IPV4 : parse_ipv4;
            default : reject;
        }
    }

    state parse_ipv4 {
        pkt.extract(hdr.ipv4);
        transition select(hdr.ipv4.protocol) {
            IP_PROTOCOLS_UDP : parse_udp;
            default : accept;
        }
    }

    state parse_udp {
        pkt.extract(hdr.udp);
        transition select(hdr.udp.dst_port) {
            UDP_PORT_ROCE : parse_bth;
            default : accept;
        }
    }

    
    state parse_bth {
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
            default : accept;
        }
    }

    state parse_reth {
        pkt.extract(hdr.reth);
        transition accept;
    }

    state parse_aeth {
        pkt.extract(hdr.aeth);
        transition accept;
    }
}


control ShuffleIngressDeparser(packet_out pkt,
                              inout ig_header_t hdr,
                              in ig_metadata_t ig_md,
                              in ingress_intrinsic_metadata_for_deparser_t 
                                ig_intr_dprsr_md
                              ) {
    Mirror() mirror;
    Checksum() csum;
    apply {
        pkt.emit(hdr);
        if (ig_intr_dprsr_md.mirror_type == MIRROR_TYPE_I2E) {
            mirror.emit<mirror_h>(ig_md.sess, {
                ig_md.pkt_type,
                ig_md.flag,
                ig_md.item_cnt,
                ig_md.item_id});
        }
        if (hdr.ipv4.isValid()) {
            hdr.ipv4.hdr_checksum = csum.update({
                hdr.ipv4.ver_ihl,
                hdr.ipv4.diffserv,
                hdr.ipv4.total_len,
                hdr.ipv4.identification,
                hdr.ipv4.flag_offset,
                hdr.ipv4.ttl,
                hdr.ipv4.protocol,
                hdr.ipv4.src_addr,
                hdr.ipv4.dst_addr
            });
        }
    }
}


#endif
