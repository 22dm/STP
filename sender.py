#!/usr/bin/env python

######################################
#  Simple Transport Protocol Sender  #
#      sjysd(Tentative)  2019/05/07  #
######################################

import random
import select
import sys

from STP import *

if len(sys.argv) != 9:
    print("Usage: sender.py receiver_host_ip receiver_port file.txt MWS MSS timeout pdrop seed")
    sys.exit(1)
recv_ip, recv_port, file, MWS, MSS, timeout, pdrop, seed = sys.argv[1:]
recv_port = int(recv_port)
MWS = int(MWS)
MSS = int(MSS)
timeout = int(timeout) / 1000
pdrop = float(pdrop)
seed = int(seed)

addr = (recv_ip, recv_port)
random.seed(seed)

seq_base = 0
seq_num = 1000
ack_num = 0
acked_num = 0
state = "CLOSED"

data_transferred = 0
data_segments = 0
data_dropped = 0
retransmitted_segments = 0
duplicate_ack = 0

sender = STPServer("Sender_log.txt")
buffer = {}

all_data = open(file, "rb").read()
data_len = len(all_data)


def pld_send(pld_pkt, pld_addr):
    global data_dropped
    if random.random() < pdrop:
        sender.drop(pld_pkt)
        data_dropped += 1
    else:
        sender.send(pld_pkt, pld_addr)


def send_seq_data(pkt_seq_num):
    global data_segments, retransmitted_segments
    if pkt_seq_num in buffer:
        pkt_to_send = buffer[pkt_seq_num][0]
        retransmitted_segments += 1
    else:
        data_split_end = min(data_len, pkt_seq_num - seq_base + int(MSS))
        if data_split_end + seq_base - acked_num <= int(MWS):
            data = all_data[pkt_seq_num - seq_base:data_split_end]
            pkt_to_send = STPPacket(pkt_seq_num, ack_num, ack=True, data=data)
            data_segments += 1
        else:
            return 0
    pld_send(pkt_to_send, addr)
    buffer[pkt_seq_num] = (pkt_to_send, time.process_time())
    return len(pkt_to_send.data)


while True:
    if state == "CLOSED":
        sender.send(STPPacket(seq_num, ack_num, syn=True), addr)
        seq_num += 1
        state = "SYN_SEND"

    if state == "SYN_SEND":
        pkt = sender.recv()

        if pkt.ack and pkt.syn and pkt.ack_num == seq_num:
            ack_num = pkt.seq_num + 1
            acked_num = pkt.ack_num
            seq_base = seq_num
            sender.send(STPPacket(seq_num, ack_num, ack=True), addr)
            state = "ESTABLISHED"

    if state == "ESTABLISHED":
        same_ack_num = 0
        while acked_num < data_len + seq_base:
            if seq_num < data_len + seq_base:
                trans_len = send_seq_data(seq_num)
                data_transferred += trans_len
                seq_num += trans_len

            if select.select([sender.socket, ], [], [], 0)[0]:
                pkt = sender.recv()

                if pkt.ack_num > acked_num:
                    acked_num = pkt.ack_num
                    same_ack_num = 1

                elif pkt.ack_num == acked_num:
                    duplicate_ack += 1
                    same_ack_num += 1
                    if same_ack_num >= 3:
                        send_seq_data(acked_num)

            now_time = time.process_time()
            for seq in list(buffer.keys()):
                if seq < acked_num:
                    buffer.pop(seq)
                elif now_time - buffer[seq][1] >= timeout:
                    send_seq_data(seq)

        sender.send(STPPacket(seq_num, ack_num, fin=True), addr)
        seq_num += 1
        state = "FIN_WAIT"

    if state == "FIN_WAIT":
        pkt = sender.recv()

        if pkt.fin and pkt.ack and pkt.ack_num == seq_num:
            ack_num += 1
            sender.send(STPPacket(seq_num, ack_num, ack=True), addr)
            state = "TIME_WAIT"

    if state == "TIME_WAIT":
        sender.close()
        state = "CLOSED"
        break

sender.log("")
sender.log("Amount of Data Transferred: " + str(data_transferred))
sender.log("Number of Data Segments Sent: " + str(data_segments))
sender.log("Number of Packets Dropped: " + str(data_dropped))
sender.log("Number of Retransmitted Segments: " + str(retransmitted_segments))
sender.log("Number of Duplicate Ack received: " + str(duplicate_ack))
