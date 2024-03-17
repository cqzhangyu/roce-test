import queue
import numpy as np
from rdma import *

class Switch:
    def __init__(self, se_num, su_p_se, psn_ring_size, mcast_grp_size, req_mtu):
        # queue of unprocessed rx_queue
        self.rx_queue = queue.Queue()
        self.tx_queue = queue.Queue()

        # number of shuffle endpoints
        self.se_num = se_num
        # number of shuffle units per shuffle endpoint
        self.su_p_se = su_p_se
        # size of psn ring
        self.psn_ring_size = psn_ring_size
        # size of multicast group
        self.mcast_group_size = mcast_grp_size

        self.req_mtu = req_mtu

        # status for each shuffle endpoint
        # 0: down, 1: active
        self.se_state = np.zeros(se_num, dtype=int) + 1
        # next PSN to ack, minimum unacked PSN
        self.sreq_unack_psn = np.zeros(se_num, dtype=int)
        # expected (max) PSN to be received from shuffle requester
        self.se_req_epsn = np.zeros(se_num, dtype=int)
        # se MSN
        self.se_req_msn = np.zeros(se_num, dtype=int)
        # write back address of the latest request
        self.se_wr_addr = np.zeros(se_num, dtype=int)

        # su unfinished packets
        self.su_rem = np.zeros((se_num, su_p_se), dtype=int)
        # req PSN of su
        self.su_req_psn = np.zeros((se_num, su_p_se), dtype=int)
        # shuffle request packet MSN, used in responder's ACK
        self.su_req_msn = np.zeros((se_num, su_p_se), dtype=int)
        # write back base address of su
        self.su_wr_addr = np.zeros((se_num, su_p_se), dtype=int)

        # next PSN for each shuffle read qp
        # when se is down, it becomes the sread_ack_psn
        self.sread_psn = np.zeros((se_num, se_num), dtype=int)
        # max unacked PSN of a shuffle read endpoint
        self.sread_unack_psn = np.zeros((se_num, se_num), dtype=int)
        # map a shuffle read PSN to its su
        self.sread_psn_to_si = np.zeros((se_num, se_num, self.psn_ring_size), dtype=int)
        # write back offset for a shuffle item
        self.si_wr_off = np.zeros((se_num, su_p_se, self.req_mtu), dtype=int)
        # next PSN for each shuffle write qp
        self.swrite_psn = np.zeros(se_num, dtype=int)
        # map a shuffle write PSN to its su
        self.swrite_psn_to_su = np.zeros((se_num, su_p_se * self.req_mtu), dtype=int)

    def process_loopback(self, p : Packet):
        se_id = p.smac
        if p.data.__len__() == 1:
            dmac, len, wr_off, dst_addr = p.data[0]

            # register action of sread_psn
            if self.sread_psn[se_id, dmac] >= self.sread_unack_psn[se_id, dmac] + self.psn_ring_size:
                # read psn ring overflows
                # drop packet?
                # or loopback packet?
                print("***WARNING***: PSN ring is not enough!")
                if self.se_state[se_id] == 0:
                    return
                self.tx_queue.put(p)
            else:
                psn = self.sread_psn[se_id, dmac]
                self.sread_psn[se_id, dmac] += 1

                self.sread_psn_to_si[se_id, dmac, psn % self.psn_ring_size] = p.si
                self.si_wr_off[se_id, p.si // self.req_mtu, p.si % self.req_mtu] = wr_off

                if self.se_state[se_id] == 0:
                    return
                # generate shuffle read packet
                rd = Packet(opcode="READ",
                            smac=-1,
                            dmac=dmac,
                            psn=psn,
                            dqpn=se_id,
                            addr=dst_addr,
                            len=len)
                
                self.tx_queue.put(rd)

        elif p.data.__len__() <= self.mcast_group_size:
            # generate loopback packet
            # multicast only
            if self.se_state[se_id] == 0:
                return
            for i in range(p.data.__len__()):
                mc_p = copy.copy(p)
                # multicast: change packet format at igress
                mc_p.opcode = "LOOPBACK"
                mc_p.si = p.si + i
                mc_p.data = [p.data[i]]
                self.tx_queue.put(mc_p)
        else:
            # mirror + multicast
            if self.se_state[se_id] == 0:
                return
            for i in range(self.mcast_group_size):
                mc_p = copy.copy(p)
                # multicast: change packet format at igress
                mc_p.opcode = "LOOPBACK"
                mc_p.si = p.si + i
                mc_p.data = [p.data[i]]
                self.tx_queue.put(mc_p)
            
            # mirror: change packet format at egress
            mir_p = copy.copy(p)
            mir_p.opcode = "LOOPBACK"
            mir_p.si = p.si + self.mcast_group_size
            mir_p.data = p.data[self.mcast_group_size:].copy()
            self.tx_queue.put(mir_p)

    def process(self, p : Packet):

        if p.opcode == "LOOPBACK":
            se_id = p.smac
            # generate shuffle read packet
            self.process_loopback(p)

        elif p.dqpn == -1:
            # shuffle request packet
            # MAT
            se_id = p.smac

            if p.psn > self.se_req_epsn[se_id]:
                # out-of-order request, drop
                return

            elif p.psn < self.sreq_unack_psn[se_id]:
                # duplicate request
                ack = Packet(opcode="ACK",
                             smac=-1,
                             dmac=p.smac,
                             dqpn=self.se_num + 1,
                             psn=self.sreq_unack_psn[se_id] - 1,
                             msn=self.se_req_msn[se_id])
                self.tx_queue.put(ack)
                return
            elif p.psn == self.sreq_unack_psn[se_id] + self.su_p_se:
                # do not have available su
                print("***WARNING***: Do not have available su!")
                return
            elif p.psn == self.se_req_epsn[se_id]:
                # expected request
                self.se_req_epsn[se_id] += 1
                if p.opcode in ["WRITE_LAST", "WRITE_ONLY"]:
                    self.se_req_msn[se_id] += 1
                    
                if p.opcode in ["WRITE_FIRST", "WRITE_ONLY"]:
                    # initialize the se wr_addr
                    self.se_wr_addr[se_id] = p.addr

                su_id = p.psn % self.su_p_se
                # initialize su states
                self.su_wr_addr[se_id, su_id] = self.se_wr_addr[se_id]
                self.su_req_psn[se_id, su_id] = p.psn
                self.su_req_msn[se_id, su_id] = self.se_req_msn[se_id]

                self.su_rem[se_id, su_id] = p.data.__len__()
                
                if self.se_state[se_id] == 0:
                    # it's okay if we do not return
                    pass# return

            else:
                # only initialize su rem
                su_id = p.psn % self.su_p_se
                self.su_rem[se_id, su_id] = p.data.__len__()

                # retry request, restart the se
                self.se_state[se_id] = 1

            
            p.opcode = "LOOPBACK"
            p.si = su_id * self.req_mtu
            self.process_loopback(p)
                
        elif p.dqpn == -2:
            # shuffle write's ack or nack
            # shuffle read acks and shuffle write acks can be out-of-order!
            if p.opcode == "ACK":
                # MAT
                se_id = p.smac
                su_id = self.swrite_psn_to_su[se_id, p.psn % (self.su_p_se * self.req_mtu)]
                if self.sreq_unack_psn[se_id] % self.su_p_se == su_id:
                    self.sreq_unack_psn[se_id] += 1
                else:
                    # ACK is out-of-order, considered as failure
                    return
                if self.se_state[se_id] == 0:
                    # down or error
                    # Can this happen???
                    # simply drop the packet
                    return
                # send ack
                ack = Packet(opcode="ACK",
                                smac=-1,
                                dmac=p.smac,
                                psn=self.su_req_psn[se_id, su_id],
                                msn=self.su_req_msn[se_id, su_id],
                                dqpn=self.se_num + 1)
        
                self.tx_queue.put(ack)

            elif p.opcode == "NAK":
                # this turn fails

                # MAT
                se_id = p.smac

                # set se as down
                self.se_state[se_id] = 0
                self.swrite_psn[se_id] = p.psn

                # su_id = self.swrite_psn_to_su[se_id, p.psn % (self.su_p_se * self.req_mtu)]
                # self.su_rem[se_id, su_id] = -1

                # drop the packet, wait for shuffle requester timeout
            
            else:
                raise ValueError("unexpected opcode: %s" % p.opcode)
        
        elif p.dqpn >= 0 and p.dqpn < self.se_num:
            # shuffle read packet's response or NAK
            if p.opcode == "READ_RESPONSE":
                # MAT
                se_id = p.dqpn
                read_unack_psn = self.sread_unack_psn[se_id, p.smac]
                self.sread_unack_psn[se_id, p.smac] = p.psn + 1

                if self.se_state[se_id] == 0:
                    # se is down
                    # drop packet
                    # we consider all read requests successful
                    return
                if read_unack_psn != p.psn:
                    # out-of-order response, indicating packet loss
                    pass# self.se_state[se_id] = 0
                else:
                    
                    si = self.sread_psn_to_si[se_id, p.smac, p.psn % self.psn_ring_size]
                    su_id = si // self.req_mtu

                    write_psn = self.swrite_psn[se_id]
                    self.swrite_psn[se_id] += 1

                    # there must be available swrite_psn slots
                    self.swrite_psn_to_su[se_id, write_psn % (self.su_p_se * self.req_mtu)] = su_id

                    wr_addr = self.su_wr_addr[se_id, su_id] + self.si_wr_off[se_id, su_id, si % self.req_mtu]

                    ackreq = 0
                    self.su_rem[se_id, su_id] -= 1
                    if self.su_rem[se_id, su_id] == 0:
                        # the last read response packet
                        ackreq = 1

                    wb = Packet(opcode="WRITE_ONLY",
                                smac=-1,
                                dmac=se_id,
                                psn=write_psn,
                                dqpn=self.se_num+2,
                                ackreq=ackreq,
                                data=p.data,
                                addr=wr_addr,
                                len=p.data.__len__())
                    
                    self.tx_queue.put(wb)
            
            elif p.opcode == "NAK":
                se_id = p.dqpn
                # an error occurs, shut down se
                self.se_state[se_id] = 0
                self.sread_psn[se_id, p.smac] = p.psn
                self.sread_unack_psn[se_id, p.smac] = p.psn
            else:
                raise ValueError("unexpected opcode: %s" % p.opcode)

        else:
            raise ValueError("unexpected dqpn: %d" % p.dqpn)
    
    def rx_once(self):
        if not self.rx_queue.empty():
            p = self.rx_queue.get()
            self.process(p)
