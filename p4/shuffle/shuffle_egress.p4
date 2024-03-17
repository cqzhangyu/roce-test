#include "shuffle_header.p4"

control ShuffleEgress(
        inout eg_header_t hdr,
        inout eg_metadata_t eg_md,
        in egress_intrinsic_metadata_t eg_intr_md,
        in egress_intrinsic_metadata_from_parser_t eg_intr_md_from_prsr,
        inout egress_intrinsic_metadata_for_deparser_t ig_intr_dprs_md,
        inout egress_intrinsic_metadata_for_output_port_t eg_intr_oport_md) {

    apply {
        if (hdr.mirror_bridge.pkt_type == PKT_TYPE_MIRROR) {
            // add replication id to item_id
            hdr.eth.ether_type = ETHERTYPE_REPL;
        }

        hdr.mirror_bridge.setInvalid();
    }
}
