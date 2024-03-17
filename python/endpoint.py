import queue
import numpy as np
from rdma import *

class Endpoint:
    def __init__(self, 
                 name : str,
                 mac : int, 
                 se_num : int, 
                 mr_size : int):
        self.name = name
        self.sread_qps = []
        self.mac = mac
        self.se_num = se_num

        self.mr = MR(mr_size)
        self.cq = queue.Queue()
        for i in range(se_num):
            self.sread_qps.append(QP(name="%s_read_%d" % (self.name, i),
                                     smac=mac, 
                                     dmac=-1,
                                     qpn=i, 
                                     psn=0, 
                                     dqpn=i, # used in switch to identify the shuffle requester
                                     epsn=0,
                                     mr=self.mr, 
                                     cq=self.cq, 
                                     tx_wait=10,
                                     retry_wait=100,
                                     mtu=16))
        
        self.sreq_qp = QP(name="%s_req" % self.name,
                        smac=mac,
                        dmac=-1,
                        qpn=se_num+1,
                        psn=0,
                        dqpn=-1,
                        epsn=0,
                        mr=self.mr,
                        cq=self.cq,
                        tx_wait=100,
                        retry_wait=1000,
                        mtu=4)
        
        self.swrite_qp = QP(name="%s_write" % self.name,
                            smac=mac,
                            dmac=-1,
                            qpn=se_num+2,
                            psn=0,
                            dqpn=-2,
                            epsn=0,
                            mr=self.mr,
                            cq=self.cq,
                            tx_wait=10,
                            retry_wait=100,
                            mtu=16)
