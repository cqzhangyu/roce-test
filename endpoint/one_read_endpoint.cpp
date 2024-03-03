#include "rdma_endpoint.hpp"
#include "../common/config.hpp"

int main(int argc, char **argv) {
    int ret;
    Config cfg;
    cfg.parse(argc, argv);

    RDMAEndpoint ep(cfg);
    ep.run();


    if (ep.my_addr.rank == 0) {
        ret = ep.post_read(0, 0, 0, ep.dst_addrs[0].addr, ep.my_addr.addr, 1024);
        logassert(ret != 0, "Cannot post read");
        loginfo("Post read succeeded");
        // wait for 100ms
        while (true) {

            usleep(1000000);

            uint64_t cqe;
            ret = ep.poll_cq(0, 0, cqe);
            if (ret == 0) {
                loginfo("Poll CQ succeeded");
                logassert(cqe != 0, "Wrong cqe");
                break;
            }
            else {
                loginfo("Poll CQ failed");
            }
        }
    }
    else {
        usleep(10000000);
    }
    return 0;
}
