import queue
import numpy as np
import copy

class Packet:
    def __init__(self, 
                 opcode : str = "", 
                 smac : int = 0, # src mac
                 dmac : int = 0, # dst mac
                 psn : int = 0, 
                 dqpn : int = 0,
                 ackreq : int = 0,
                 addr : int = 0,
                 len : int = 0,
                 msn : int = 0, 
                 si : int = 0,
                 data : list = []):
        # generic attributes
        self.opcode = opcode
        self.smac = smac
        self.dmac = dmac
        self.psn = psn
        self.dqpn = dqpn
        self.ackreq = ackreq

        # reth
        self.addr = addr
        self.len = len

        # aeth
        self.msn = msn

        # loopback
        self.si = si

        # payload
        self.data = data.copy()
    
    def to_str(self):
        
        r = "%s mac:%d->%d" % (self.opcode, self.smac, self.dmac)
        if self.opcode != "LOOPBACK":
            r += " psn:%d dqp:%d" % (self.psn, self.dqpn)
        else:
            r += " si:%d" % (self.si)
        if self.opcode in ["READ", "WRITE_FIRST", "WRITE_ONLY"]:
            r += " addr:%d len:%d" % (self.addr, self.len)
            if self.ackreq == 1:
                r += " ackreq:1"
        if self.opcode in ["ACK", "READ_RESPONSE"]:
            r += " msn:%d" % (self.msn)
        if self.opcode in ["LOOPBACK", "READ_RESPONSE", "WRITE_FIRST", "WRITE_ONLY", "WRITE_MIDDLE", "WRITE_LAST"]:
            r += " data:%s" % (str(self.data))
        return r


        
class MR:
    def __init__(self, size : int):
        self.size = size

        self.buf = [None for i in range(size)]

class WR:
    def __init__(self,
                 opcode : str,
                 s_addr : int,
                 d_addr : int,
                 size : int):
        
        self.opcode = opcode # only support WRITE now
        self.s_addr = s_addr
        self.d_addr = d_addr
        self.size = size

class QP:
    def __init__(self, 
                 name : str = "",
                 smac : int = 0,
                 dmac : int = 0,
                 qpn : int = 0, 
                 psn : int = 0,
                 dqpn : int = 0,
                 epsn : int = 0,
                 mr : MR = None, 
                 cq : queue.Queue = None,
                 tx_wait : int = 0,
                 retry_wait : int = 0,
                 mtu : int = 0):
        self.name = name
        self.smac = smac
        self.dmac = dmac
        self.qpn = qpn
        self.psn = psn
        self.dqpn = dqpn
        self.epsn = epsn
        
        self.msn = 0    # used to generate ack or read_response

        self.mr = mr
        self.cq = cq
        self.sq = []

        self.rx_queue = queue.Queue()
        self.tx_queue = queue.Queue()

        self.tx_until = 0
        self.retry_timer = retry_wait
        self.tx_wait = tx_wait
        self.retry_wait = retry_wait
        self.retry_cnt = 0

        self.old_req_psn = 0
        self.old_unack_psn = 0
        self.max_fwd_psn = 0

        self.mtu = mtu
    
    def gen_packet(self, *args, **kwargs) -> Packet:
        return Packet(smac=self.smac, 
                      dmac=self.dmac, 
                      dqpn=self.dqpn,
                      *args, **kwargs)

                
    def tx_once(self):
        if self.sq.__len__() == 0:
            return
        
        req_psn = self.old_req_psn
        for wr_id in range(0, self.sq.__len__()):
            wr : WR = self.sq[wr_id]

            # currently only support WRITE
            if wr.opcode == "WRITE":
                if wr.size <= self.mtu * (self.psn - req_psn):
                    req_psn += (wr.size + self.mtu - 1) // self.mtu
                    continue

                p = self.gen_packet(psn=self.psn)
                self.psn += 1
                self.max_fwd_psn = max(self.max_fwd_psn, p.psn)
                if wr.size <= self.mtu * (p.psn - req_psn + 1):
                    if p.psn == req_psn:
                        p.opcode = "WRITE_ONLY"
                        p.len = wr.size
                        p.addr = wr.d_addr
                    else:
                        p.opcode = "WRITE_LAST"
                    cur_len = wr.size - self.mtu * (p.psn - req_psn)
                    
                else:
                    if p.psn == req_psn:
                        p.opcode = "WRITE_FIRST"
                        p.len = wr.size
                        p.addr = wr.d_addr
                    else:
                        p.opcode = "WRITE_MIDDLE"
                    cur_len = self.mtu
                
                s_addr = wr.s_addr + self.mtu * (p.psn - req_psn)
                for i in range(s_addr, s_addr + cur_len):
                    p.data.append(self.mr.buf[i])
                
                self.tx_queue.put(p)
            else:
                raise ValueError("Only support write")

    def rx(self, p : Packet):
        if p.opcode in ["ACK"]:
            # receive a response
            if p.psn < self.old_req_psn or p.psn > self.max_fwd_psn:
                # invalid response, drop packet
                pass
            elif p.psn >= self.old_req_psn and p.psn < self.old_unack_psn:
                # duplicate response, ignore
                pass
            elif p.psn >= self.old_unack_psn:
                self.old_unack_psn = p.psn + 1

                while self.sq.__len__() > 0:
                    wr : WR = self.sq[0]

                    wr_max_psn = self.old_req_psn + (wr.size + self.mtu - 1) // self.mtu - 1

                    if self.old_unack_psn > wr_max_psn:
                        # current wr finishes
                        self.sq.remove(wr)
                        self.cq.put(wr)

                        self.old_req_psn = wr_max_psn + 1
                        self.retry_timer = self.retry_wait
                        self.retry_cnt = 0
                    else:
                        break
            return
        elif p.opcode == "NAK":
            raise ValueError("Cannot handle NAK at endpoint")

        if p.psn < self.epsn:
            # received a duplicate packet
            if p.opcode == "READ":
                read_res = self.gen_packet(opcode="READ_RESPONSE",
                                           psn=self.epsn,
                                           data=self.mr.buf[p.addr:p.addr + p.len])
                self.tx_queue.put(read_res)
            else:
                if p.ackreq == 1:
                    ack = self.gen_packet(opcode="ACK",
                                        psn=p.psn,
                                        msn=self.msn)
                    self.tx_queue.put(ack)

        elif p.psn > self.epsn:
            # received an out-of-order packet
            nak = self.gen_packet(opcode="NAK",
                                  psn=self.epsn,
                                  msn=self.msn)
            self.tx_queue.put(nak)
            
        else:
            # packet order is correct
            self.epsn += 1
            self.msn += 1
            if p.opcode == "READ":
                read_res = self.gen_packet(opcode="READ_RESPONSE",
                                           psn=p.psn,
                                           data=self.mr.buf[p.addr:p.addr + p.len])
                
                self.tx_queue.put(read_res)
            elif p.opcode == "WRITE_ONLY":
                self.mr.buf[p.addr] = p.data[0]
                if p.ackreq == 1:
                    ack = self.gen_packet(opcode="ACK",
                                          psn=p.psn,
                                          msn=self.msn)
                    self.tx_queue.put(ack)
                
    def rx_once(self):
        if not self.rx_queue.empty():
            p = self.rx_queue.get()
            self.rx(p)

    def retry(self):
        self.psn = self.old_unack_psn
        self.retry_timer = self.retry_wait
        self.retry_cnt += 1
    
    def post_wr(self, wr):
        self.sq.append(copy.copy(wr))

