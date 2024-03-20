#pragma once

#include "cmdline.h"
#include "logger.hpp"
#include <unistd.h>

using std::string;
using std::vector;

class Config {
public:
    string bind_ip;
    int ib_port;
    int gid_index;
    int n_core;
    int psn;
    int n_endpoint;
    int q_size;
    size_t mr_size;
    int mtu;
    string master_ip;
    int master_port;

    string p4_name;

    string log_file;
    string log_level;
    
    cmdline::parser parser;

    Config() {
        parser.add<string>("bind_ip", 'b', "bind ip address", false, "localhost");
        parser.add<int>("ib_port", 'p', "IB port", false, 1);
        parser.add<int>("gid_index", 'g', "gid index", false, 2);
        parser.add<int>("n_core", 'c', "number of cores", false, 1);
        parser.add<int>("psn", 0, "initiate PSN", false, 1);
        parser.add<int>("n_endpoint", 'e', "number of endpoints", false, 1);
        parser.add<int>("q_size", 0, "WQ size of each QP", false, 64);
        parser.add<size_t>("mr_size",'s', "mr size", false, 1024 * 1024 * 1024);
        parser.add<int>("mtu", 0, "mtu", false, 1024);

        parser.add<string>("master_ip", 0, "master IP", false, "");
        parser.add<int>("master_port", 0, "master port", false, 1234);

        parser.add<string>("p4_name", 0, "P4 program name", false, "shuffle");

        parser.add<string>("log_file", 0, "log file", false, "");
        parser.add<string>("log_level", 0, "log level", false, "info");
    }

    void parse(int argc, char *argv[]) {
        parser.parse_check(argc, argv);

        bind_ip = parser.get<string>("bind_ip");
        ib_port = parser.get<int>("ib_port");
        gid_index = parser.get<int>("gid_index");
        n_core = parser.get<int>("n_core");
        psn = parser.get<int>("psn");
        n_endpoint = parser.get<int>("n_endpoint");
        q_size = parser.get<int>("q_size");
        mr_size = parser.get<size_t>("mr_size");
        mtu = parser.get<int>("mtu");
        master_ip = parser.get<string>("master_ip");
        master_port = parser.get<int>("master_port");
        p4_name = parser.get<string>("p4_name");

        log_file = parser.get<string>("log_file");
        log_level = parser.get<string>("log_level");

        if (log_file != "") {
            logger.open(log_file);
        }
        logger.set_level(log_level);
    }
};
