######################################
#  Simple Transport Protocol Server  #
#      sjysd(Tentative)  2019/05/07  #
######################################

import pickle
import time
from socket import *


class STPPacket:
    def __init__(self, seq_num, ack_num, syn=False, ack=False, fin=False, data=''):
        self.seq_num = seq_num
        self.ack_num = ack_num
        self.syn = syn
        self.ack = ack
        self.fin = fin
        self.data = data


class STPServer:

    def __init__(self, log_file):
        self.log_file = log_file
        open(self.log_file, "w").close()
        self.socket = socket(AF_INET, SOCK_DGRAM)

    def recv(self):
        return self.recv_with_udp_header()[0]

    def recv_with_udp_header(self):
        data, addr = self.socket.recvfrom(4096)
        pkt = pickle.loads(data)
        self.log_pkt("rcv", pkt)
        return pkt, addr

    def bind(self, port):
        self.socket.bind(('', port))

    def send(self, pkt, addr):
        self.log_pkt("snd", pkt)
        self.socket.sendto(pickle.dumps(pkt), addr)

    def drop(self, pkt):
        self.log_pkt("drop", pkt)

    def close(self):
        self.socket.close()

    def log_pkt(self, action, pkt):
        now = time.process_time() * 1000

        pkt_type = ''
        pkt_type += 'S' if pkt.syn else ''
        pkt_type += 'F' if pkt.fin else ''
        pkt_type += 'A' if pkt.ack else ''
        pkt_type = 'D' if len(pkt.data) != 0 else pkt_type

        log = "{:<5}{:<8.2f}{:<4}{:<8}{:<8}{}".format(action, now, pkt_type, pkt.seq_num, len(pkt.data), pkt.ack_num)

        self.log(log)

    def log(self, log):
        print(log)
        f = open(self.log_file, "a+")
        f.write(log + "\n")
        f.close()
