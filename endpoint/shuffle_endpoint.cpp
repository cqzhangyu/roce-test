#include "shuffle_endpoint.hpp"
#include "../common/config.hpp"

int main(int argc, char **argv) {
    Config cfg;
    cfg.parse(argc, argv);

    ShuffleEndpoint ep(cfg);
    ep.initialize();

    int n_item = 4;
    uintptr_t mr_begin = (uintptr_t)ep.mr_mmap;
    int *src_addr = (int *)mr_begin;
    mr_begin += sizeof(int) * n_item;
    int *dst_addr = (int *)mr_begin;
    mr_begin += sizeof(int) * n_item;
    struct shuffle_request *req_addr = (struct shuffle_request *)mr_begin;
    mr_begin += sizeof(struct shuffle_request) * n_item;

    for (int i = 0; i < n_item; i++) {
        src_addr[i] = i;
        dst_addr[i] = 0;
        req_addr[i].src_addr = htobe64((uintptr_t)src_addr + i * sizeof(int));
        req_addr[i].src_id = htons(0);
        req_addr[i].len = htons(sizeof(int));
        req_addr[i].write_off = htonl((n_item - i - 1) * (int)sizeof(int));
    }

    int ret;
    ret = ep.post_shuffle_request(0, 0, req_addr, dst_addr, n_item);
    logassert(ret != 0, "Cannot post shuffle request");
    loginfo("Post shuffle request succeeded");
    while (true) {
        usleep(1000000);

        uint64_t cqe;
        ret = ep.poll_shuffle_request_cq(0, cqe);
        if (ret == 0) {
            loginfo("Poll shuffle request CQ succeeded");
            logassert(cqe != 0, "Wrong cqe");
            break;
        }
        else {
            loginfo("Poll shuffle request CQ failed");
        }
    }

    for (int i = 0; i < n_item; i ++) {
        logassert(dst_addr[i] != src_addr[n_item - i - 1], "Wrong value at ", i, ": ", dst_addr[i], " != ",  src_addr[n_item - i - 1]);
    }
    ep.finish();
    return 0;
}
