#include "../rdma/rdma_utils.hpp"
#include "../common/config.hpp"

int main(int argc, char **argv) {
    Config cfg;
    int ret;
    cfg.parse(argc, argv);
    
    string dev_name = get_dev_by_ip(cfg.bind_ip);
    string ib_dev_name = dev_to_ib_dev(dev_name);
    struct ibv_device *ib_dev = find_ib_device(ib_dev_name);
    struct ibv_context *context = ibv_open_device(ib_dev);
    logassert(context == NULL, "Cannot open ib device", ib_dev_name);

    struct ibv_device_attr device_attr;
    ret = ibv_query_device(context, &device_attr);
    logassert(ret != 0, "failed to query device");

    loginfo("max_mr_size ", device_attr.max_mr_size);
    loginfo("max_qp ", device_attr.max_qp);
    loginfo("max_qp_wr ", device_attr.max_qp_wr);
    loginfo("max_cq ", device_attr.max_cq);
    loginfo("max_cqe ", device_attr.max_cqe);
    loginfo("max_mr ", device_attr.max_mr);

    struct ibv_port_attr port_attr;
    ret = ibv_query_port(context, cfg.ib_port, &port_attr);
    logassert(ret != 0, "failed to query port");
    loginfo("state ", port_attr.state);
    loginfo("max_mtu ", port_attr.max_mtu);
    loginfo("max_msg_sz ", port_attr.max_msg_sz);
    loginfo("lid ", port_attr.lid);
    loginfo("link_layer ", port_attr.link_layer);

    ibv_close_device(context);
}
