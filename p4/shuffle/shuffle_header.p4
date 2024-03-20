#ifndef _SHUFFLE_HEADER_
#define _SHUFFLE_HEADER_

#include "../common/header.p4"

#if __TARGET_TOFINO__ == 1
typedef bit<3> mirror_type_t;
#else
typedef bit<4> mirror_type_t;
#endif
const mirror_type_t MIRROR_TYPE_I2E = 1;
const ether_type_t ETHERTYPE_REPL = 16w0x0771;

#define REQ_MTU_SHIFT       8
#define REQ_MTU             (1<<REQ_MTU_SHIFT)
#define ITEM_SIZE_SHIFT     4
#define ITEM_SIZE           (1<<ITEM_SIZE_SHIFT)
#define ITEM_SHIFT          (REQ_MTU_SHIFT-ITEM_SIZE_SHIFT)
#define ITEM_PER_UNIT       (1<<ITEM_SHIFT)

#define ENDP_SHIFT          3
#define MAX_NUM_ENDP        (1<<ENDP_SHIFT)

#define UNIT_SHIFT          4
#define UNIT_PER_ENDP       (1<<UNIT_SHIFT)
#define UNIT_MASK           (UNIT_PER_ENDP - 1)
#define SHL_UNIT_SHIFT      (16-ENDP_SHIFT-UNIT_SHIFT)

#define READ_RING_SHIFT     9
#define READ_RING_SIZE      (1<<READ_RING_SHIFT)
#define READ_RING_MASK      (READ_RING_SIZE-1)
#define WRITE_RING_SHIFT    (UNIT_SHIFT+ITEM_SHIFT)

#define WRITE_RING_MASK     (UNIT_PER_ENDP*ITEM_PER_UNIT-1)

#define MCAST_GRP_SIZE      8

#define REPL_FLAG_SETSTATE  1

#define CASE_BYPASS         0

#define CASE_REQUEST        1

#define CASE_READ_RESPONSE  11
#define CASE_READ_NAK       12
#define CASE_WRITE_ACK      13
#define CASE_WRITE_NAK      14

#define CASE_REPL_ONLY      20
#define CASE_REPL_MANY      21

const bit<8> PKT_TYPE_BYPASS = 0;
const bit<8> PKT_TYPE_ORIGIN = 1;
const bit<8> PKT_TYPE_MIRROR = 2;

header mirror_bridged_metadata_h {
    bit<8> pkt_type;
    // bit<8> _mb_pad;
}
header mirror_h {
    bit<8>  pkt_type;
    // bit<8> _mb_pad;

    bit<8>  flag;
    // bit<8>  _repl_pad;
    bit<16> item_cnt;
    bit<16> item_id;

    bit<16> src_id;
    bit<16> len;
    bit<32> write_off;
    bit<64> src_addr;
}

header repl_h {
    bit<8>  flag;
    // bit<8>  _repl_pad;
    bit<16> item_cnt;
    bit<16> item_id;
}


header item_h {
    bit<16> src_id;
    bit<16> len;
    bit<32> write_off;
    bit<64> src_addr;
}

struct ig_header_t {
    mirror_bridged_metadata_h mirror_bridge;
    eth_h eth;

    repl_h repl;

    ipv4_h ipv4;
    udp_h udp;

    bth_h bth;
    aeth_h aeth;
    reth_h reth;

    item_h item0;

    icrc_h icrc;
}

struct ig_metadata_t {
    MirrorId_t sess;

    bit<8>  pkt_type;
    // bit<8> _mb_pad;

    bit<8>  flag;
    // bit<8>  _repl_pad;
    bit<16> item_cnt;
    bit<16> item_id;

    bit<16> src_id;
    bit<16> len;
    bit<32> write_off;
    bit<64> src_addr;
}

struct eg_header_t {
    mirror_bridged_metadata_h mirror_bridge;
    eth_h eth;
    repl_h repl;
    item_h item0;
}

struct eg_metadata_t {}

#endif
