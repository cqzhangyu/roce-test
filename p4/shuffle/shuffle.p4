#include <core.p4>
#if __TARGET_TOFINO__ == 2
#include <t2na.p4>
#else
#include <tna.p4>
#endif

#include "shuffle_header.p4"
#include "shuffle_ingress.p4"
#include "shuffle_ingress_parser.p4"
#include "shuffle_egress.p4"
#include "shuffle_egress_parser.p4"

Pipeline(
    ShuffleIngressParser(),
    ShuffleIngress(),
    ShuffleIngressDeparser(),
    ShuffleEgressParser(),
    ShuffleEgress(),
    ShuffleEgressDeparser()
) pipe;

Switch(pipe) main;
