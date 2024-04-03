/* -*- P4_16 -*- */
#include <core.p4>
#if __TARGET_TOFINO__ == 2
#include <t2na.p4>
#else
#include <tna.p4>
#endif

typedef bit<9> egress_spec_t;
typedef bit<48> mac_addr_t;

/*************************************************************************
*********************** H E A D E R S  ***********************************
*************************************************************************/

header eth_t {
    mac_addr_t dst_addr;
    mac_addr_t src_addr;
    bit<16> ether_type;
}

struct headers {
    eth_t eth;
}

struct port_metadata_t {
    bit<16> unused; 
}

struct metadata {
    port_metadata_t port_metadata;
}

/*************************************************************************
*********************** P A R S E R  ***********************************
*************************************************************************/

parser IngressParser(packet_in packet,
               out headers hdr,
               out metadata meta,
               out ingress_intrinsic_metadata_t ig_intr_md) {

    state start {
        packet.extract(ig_intr_md);
        transition select(ig_intr_md.resubmit_flag) {
            0 : parse_port_metadata;
        }
    }

    state parse_port_metadata {
        meta.port_metadata = port_metadata_unpack<port_metadata_t>(packet);
        transition parse_eth;
    }

    state parse_eth {
        packet.extract(hdr.eth);
        transition accept;
    }
}


/*************************************************************************
**************  I N G R E S S   P R O C E S S I N G   *******************
*************************************************************************/

control ShuffleIngress(
        inout headers hdr,
        inout metadata meta,
        in ingress_intrinsic_metadata_t ig_intr_md,
        in ingress_intrinsic_metadata_from_parser_t ig_intr_prsr_md,
        inout ingress_intrinsic_metadata_for_deparser_t ig_intr_dprs_md,
        inout ingress_intrinsic_metadata_for_tm_t ig_intr_tm_md) {
    
    action drop() {
        ig_intr_dprs_md.drop_ctl = 0x1;
    }

    action l2_forward(bit<9> port) {
        ig_intr_tm_md.ucast_egress_port = port;
    }

    table l2_route{
        key = {
            hdr.eth.dst_addr: exact;
        }
        actions = {
            l2_forward;
            drop;
        }
        size = 32;
        default_action = drop();
    }

    bit<32> partition_id;

    @atcam_partition_index("partition_id")
    @atcam_number_partitions(256)
    table atcam_tbl {
        key = {
            partition_id : exact;
            hdr.eth.dst_addr : ternary;
        }
        actions = {
            l2_forward;
        }
        size = 65536;
        default_action = l2_forward(0);
    }

    apply {
        // ig_intr_tm_md.bypass_egress = 1;
        l2_route.apply();
        partition_id = hdr.eth.src_addr[31:0];
        atcam_tbl.apply();
    }
}

/*************************************************************************
****************  E G R E S S   P R O C E S S I N G   *******************
*************************************************************************/

control IngressDeparser(
        packet_out packet,
        inout headers hdr,
        in metadata meta,
        in ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md) {

    apply{
        packet.emit(hdr.eth);
    }
}

parser EgressParser(packet_in packet,
               out headers hdr,
               out metadata meta,
               out egress_intrinsic_metadata_t eg_intr_md) {
    state start {
        packet.extract(eg_intr_md);
        transition accept;
    }
}

control Egress(
        inout headers hdr,
        inout metadata meta,
        in egress_intrinsic_metadata_t eg_intr_md,
        in egress_intrinsic_metadata_from_parser_t eg_intr_prsr_md,
        inout egress_intrinsic_metadata_for_deparser_t ig_intr_dprs_md,
        inout egress_intrinsic_metadata_for_output_port_t eg_intr_oport_md) {
    apply {
    }
}

control EgressDeparser(packet_out pkt,
                  inout headers hdr,
                  in metadata meta,
                  in egress_intrinsic_metadata_for_deparser_t ig_intr_dprs_md) {
    apply {
        
        pkt.emit(hdr);
    }
}

Pipeline(IngressParser(), ShuffleIngress(), IngressDeparser(), EgressParser(), Egress(), EgressDeparser()) pipe;

Switch(pipe) main;
