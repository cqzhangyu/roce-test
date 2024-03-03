#include "rdma_master.hpp"
#include "../common/config.hpp"

int main(int argc, char **argv) {
    Config cfg;
    cfg.parse(argc, argv);

    RDMAMaster master(cfg);
    master.run();
}
