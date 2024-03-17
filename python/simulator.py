from switch import *
from endpoint import *
from rdma import *
from config import *
import traceback
import random

# number of shuffle endpoints
se_num = 1
# number of su per se
su_p_se = 8
# start psn
psn = 0
# number of elements
ele_num = 128
# size of MR
mr_size = ele_num * 3
# shuffle request's message size
sreq_msg_size = ele_num
# size of PSN ring
psn_ring_size = 1024
# size of multicast group
mcast_grp_size = 8

def simulate(qps, switch : Switch):
    all_empty = False
    ts = 0

    while not all_empty:

        # tx
        for qp in qps:
            qp : QP
            
            if qp.sq.__len__() > 0:
                qp.retry_timer -= 1
                if qp.retry_timer == 0:
                    qp.retry()

                    print("<%d> QP(%s) Retry (%d)" % (ts, qp.name, qp.retry_cnt))
                    if qp.retry_cnt == 5:
                        print("Too many retries")
                        raise Exception("Too many retries")
                    
            if ts >= qp.tx_until:
                qp.tx_until = ts + qp.tx_wait
                qp.tx_once()
                
                if not qp.tx_queue.empty():
                    p = qp.tx_queue.get()
                    if ts < 2000 and random.randint(0, 100) == 0:
                        print("***LOST*** <%d> QP(%s)->Switch %s" % (ts, qp.name, p.to_str()))
                        continue
                    print("<%d> QP(%s)->Switch %s" % (ts, qp.name, p.to_str()))
                    switch.rx_queue.put(p)
                    
                    qp.last_tx = ts
            
        if not switch.tx_queue.empty():
            p : Packet = switch.tx_queue.get()
            if ts < 2000 and random.randint(0, 100) == 0:
                if p.dmac == -1:
                    print("***LOST*** <%d> Switch loopback %s" % (ts, p.to_str()))

                else:
                    for qp in qps:
                        qp : QP
                        if qp.smac == p.dmac and qp.qpn == p.dqpn:
                            print("***LOST*** <%d> Switch->QP(%s) %s" % (ts, qp.name, p.to_str()))
                            break
                continue
            if p.dmac == -1:
                print("<%d> Switch loopback %s" % (ts, p.to_str()))
                switch.rx_queue.put(p)

            else:
                for qp in qps:
                    qp : QP
                    if qp.smac == p.dmac and qp.qpn == p.dqpn:
                        print("<%d> Switch->QP(%s) %s" % (ts, qp.name, p.to_str()))
                        qp.rx_queue.put(p)
                        break
        
        # rx
        for qp in qps:
            qp : QP
            qp.rx_once()
            
        switch.rx_once()
        
        ts += 1
        all_empty = True
        for qp in qps:
            qp : QP
            if qp.sq.__len__() > 0 or (not qp.tx_queue.empty()) or (not qp.rx_queue.empty()):
                all_empty = False
                break
        if (not switch.rx_queue.empty()) or (not switch.tx_queue.empty()):
            all_empty = False
            

if __name__ == '__main__':
    eps = []
    qps = []

    seed = random.randint(0, 2147483647)
    print("seed: ", seed)
    random.seed(seed)
    
    for i in range(se_num):
        ep = Endpoint(name="ep%d" % i,
                      mac=i,
                      se_num=se_num,
                      mr_size=mr_size)
        qps += ep.sread_qps
        qps += [ep.swrite_qp, ep.sreq_qp]
        eps.append(ep)

    switch = Switch(se_num=se_num,
                    su_p_se=su_p_se,
                    psn_ring_size=psn_ring_size,
                    mcast_grp_size=mcast_grp_size,
                    req_mtu=eps[0].sreq_qp.mtu)
    
    for se_id in range(se_num):
        ep : Endpoint = eps[se_id]
        for i in range(0, ele_num):
            ep.mr.buf[i] = se_id * ele_num + i
            ep.mr.buf[i + ele_num] = (i % se_num, 1, i % sreq_msg_size, i)
        
    for se_id in range(se_num):
        ep : Endpoint = eps[se_id]
        for i in range(0, (ele_num + sreq_msg_size - 1) // sreq_msg_size):
            wr = WR(opcode="WRITE",
                    s_addr=i * sreq_msg_size + ele_num,
                    d_addr=i * sreq_msg_size + ele_num * 2,
                    size=min(sreq_msg_size, ele_num - i * sreq_msg_size))
            ep.sreq_qp.post_wr(wr)

    try:
        simulate(qps, switch)
    except Exception as e:
        print(e)
        traceback.print_exc()

    for se_id in range(se_num):
        ep : Endpoint = eps[se_id]
        print("Endpoint %d" % se_id)
        print(ep.mr.buf)
    
    for ep in eps:
        ep : Endpoint
        while not ep.cq.empty():
            wr : WR = ep.cq.get()
            if wr.opcode == "WRITE":
                for i in range(wr.size):
                    dmac, len, wb_off, dst_addr = ep.mr.buf[wr.s_addr + i]
                    dst_ep : Endpoint = eps[dmac]
                    for j in range(len):
                        if dst_ep.mr.buf[dst_addr] != ep.mr.buf[wr.d_addr + wb_off + j]:
                            raise ValueError("Wrong result")
