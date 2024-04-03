#ifndef _SHUFFLE_HEADER_
#define _SHUFFLE_HEADER_

#include "../common/header.p4"

#if __TARGET_TOFINO__ == 1
typedef bit<3> mirror_type_t;
#else
typedef bit<4> mirror_type_t;
#endif
const mirror_type_t MIRROR_TYPE_I2E = 1;
const bit<8> RDMA_OP_REPL = 8w0x15; // reserved

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
#define SHL_UNIT_SHIFT      (16-ENDP_SHIFT-UNIT_SHIFT)

#define READ_RING_SHIFT     6
#define READ_RING_SIZE      (1<<READ_RING_SHIFT)
#define WRITE_RING_SHIFT    8
#define WRITE_RING_SIZE     (1<<WRITE_RING_SHIFT)

#define MCAST_GRP_SIZE      8

#define REPL_FLAG_SETSTATE  1

#define CASE_BYPASS         0
#define CASE_DEBUG          100
#define CASE_DROP           255

#define CASE_REQUEST        1

#define CASE_READ_RESPONSE  11
#define CASE_READ_NAK       12
#define CASE_WRITE_ACK      13
#define CASE_WRITE_NAK      14

#define CASE_REPL_ONLY      20
#define CASE_REPL_MANY      21
#define CASE_REPL_LPBK      22

#define BRI_FLAGBIT_REQ_UNACK_UNIT_SET      0
#define BRI_FLAGBIT_REQ_EPSN_ACC            1
#define BRI_FLAGBIT_REQ_MSN_INC             2
#define BRI_FLAGBIT_REQ_DST_ADDR_SET        3
#define BRI_FLAGBIT_UNIT_DST_ADDR_SET       4
#define BRI_FLAGBIT_UNIT_REQ_PSN_SET        5
#define BRI_FLAGBIT_UNIT_REQ_MSN_SET        6
#define BRI_FLAGBIT_UNIT_REMAIN_SET         7
#define BRI_FLAGBIT_UNIT_REMAIN_ACC         8
#define BRI_FLAGBIT_ITEM_WRITE_OFFSET_SET   9
#define BRI_FLAGBIT_WRITE_PSN_TO_UNIT_SET   10

#define BRI_FLAG_REQ_UNACK_UNIT_SET     (1<<BRI_FLAGBIT_REQ_UNACK_UNIT_SET)
#define BRI_FLAG_REQ_EPSN_ACC           (1<<BRI_FLAGBIT_REQ_EPSN_ACC)
#define BRI_FLAG_REQ_MSN_INC            (1<<BRI_FLAGBIT_REQ_MSN_INC)
#define BRI_FLAG_REQ_DST_ADDR_SET       (1<<BRI_FLAGBIT_REQ_DST_ADDR_SET)
#define BRI_FLAG_UNIT_DST_ADDR_SET      (1<<BRI_FLAGBIT_UNIT_DST_ADDR_SET)
#define BRI_FLAG_UNIT_REQ_PSN_SET       (1<<BRI_FLAGBIT_UNIT_REQ_PSN_SET)
#define BRI_FLAG_UNIT_REQ_MSN_SET       (1<<BRI_FLAGBIT_UNIT_REQ_MSN_SET)
#define BRI_FLAG_UNIT_REMAIN_SET        (1<<BRI_FLAGBIT_UNIT_REMAIN_SET)
#define BRI_FLAG_UNIT_REMAIN_ACC        (1<<BRI_FLAGBIT_UNIT_REMAIN_ACC)
#define BRI_FLAG_ITEM_WRITE_OFFSET_SET  (1<<BRI_FLAGBIT_ITEM_WRITE_OFFSET_SET)
#define BRI_FLAG_WRITE_PSN_TO_UNIT_SET  (1<<BRI_FLAGBIT_WRITE_PSN_TO_UNIT_SET)

#define TX_CASE_BYPASS      0
#define TX_CASE_LOOPBACK    1
#define TX_CASE_READ_SRC    2

const bit<8> PKT_TYPE_BYPASS = 0;
const bit<8> PKT_TYPE_BRIDGE = 1;
const bit<8> PKT_TYPE_MIRROR = 2;

header mirror_bridged_metadata_h {
    bit<8> pkt_type;
    // bit<8> _mb_pad;
}
header bridge_h {
    bit<8> pkt_type;
    // bit<8> _mb_pad;

    bit<8> rx_case;
    bit<8> item_cnt;
    bit<16> flag;
    bit<16> dst_id;
    bit<16> unit_id;
    bit<16> item_id;
}

header mirror_h {
    bit<8> pkt_type;
    // bit<8> _mb_pad;
}

header repl_h {
    bit<8>  flag;
    // bit<8>  _repl_pad;
    bit<8> item_cnt;
    bit<16> item_id;
}


header item_h {
    bit<16> src_id;
    bit<16> len;
    bit<32> write_off;
    bit<64> src_addr;
}

header debug_h {
    bit<16> d0;
    bit<16> d1;
    bit<16> d2;
    bit<16> d3;
}

struct ig_header_t {
    mirror_bridged_metadata_h mirror_bridge;
    bridge_h bridge;
    eth_h eth;

    ipv4_h ipv4;
    udp_h udp;

    bth_h bth;
    aeth_h aeth;
    reth_h reth;

    repl_h repl;
    item_h item0;

    debug_h debug;

    icrc_h icrc;
}

struct port_metadata_t {
    bit<16> unused; 
}

struct ig_metadata_t {
    port_metadata_t port_metadata;
    MirrorId_t sess;

    mirror_h mirror;
}

struct eg_header_t {
    mirror_bridged_metadata_h mirror_bridge;
    mirror_h mirror;
    bridge_h bridge;
    eth_h eth;
    ipv4_h ipv4;
    udp_h udp;
    bth_h bth;
    aeth_h aeth;
    reth_h reth;

    repl_h repl;
    item_h item0;

    debug_h debug;

    icrc_h icrc;
}

struct eg_metadata_t {
    mirror_bridged_metadata_h mirror_bridge;
}

#endif
