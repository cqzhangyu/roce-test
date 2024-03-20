#include "shuffle_master.hpp"
#include "../common/config.hpp"

ShuffleMaster *master = nullptr;

// signal handler
void signal_handler(int sig) {
    if (master) {
        master->stop();
    }
    exit(0);
    return;
}

int main(int argc, char **argv) {
    Config cfg;
    cfg.parse(argc, argv);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    master = new ShuffleMaster(cfg);
    master->run();
    return 0;
}
