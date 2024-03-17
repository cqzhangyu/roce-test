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
        pkt.extract(hdr.mirror_bridge);
        transition select(hdr.mirror_bridge.pkt_type) {
            PKT_TYPE_BYPASS : accept;
            PKT_TYPE_MIRROR : parser_repl;
            default : reject;
        }
    }
    state parser_repl {
        pkt.extract(hdr.repl);
        pkt.extract(hdr.eth);
        transition accept;
    }
    state parse_ethernet {
        pkt.extract(hdr.eth);
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
