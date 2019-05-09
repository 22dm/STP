#!/usr/bin/env python

########################################
#  Simple Transport Protocol Receiver  #
#        sjysd(Tentative)  2019/05/07  #
########################################

import sys

from STP import *

if len(sys.argv) != 3:
    print("Usage: receiver.py receiver_port file.txt")
    sys.exit(1)
port, file = sys.argv[1:]
port = int(port)

addr = None

seq_num = 100
ack_num = 0
state = "CLOSED"

data_transferred = 0
data_segments = 0
duplicate_segments = 0

receiver = STPServer("Receiver_log.txt")
buffer = {}

open(file, "w").close()

while True:
    if state == "CLOSED":
        receiver.bind(port)
        state = "LISTEN"

    if state == "LISTEN":
        pkt, addr = receiver.recv_with_udp_header()

        if pkt.syn:
            ack_num = pkt.seq_num + 1
            receiver.send(STPPacket(seq_num, ack_num, syn=True, ack=True), addr)
            seq_num += 1
            state = "SYN_RECD"

    if state == "SYN_RECD":
        pkt = receiver.recv()

        if pkt.ack and pkt.ack_num == seq_num:
            state = "ESTABLISHED"

    if state == "ESTABLISHED":
        pkt = receiver.recv()

        if not pkt.fin:
            if pkt.seq_num < ack_num or pkt.seq_num in buffer:
                duplicate_segments += 1
                continue
            data_segments += 1
            buffer[pkt.seq_num] = pkt
            while ack_num in buffer:
                pkt = buffer.pop(ack_num)
                ack_num += len(pkt.data)
                data_transferred += len(pkt.data)
                f = open(file, "a+b")
                f.write(pkt.data)
                f.close()
            receiver.send(STPPacket(seq_num, ack_num, ack=True), addr)

        else:
            state = "CLOSE_WAIT"

    if state == "CLOSE_WAIT":
        ack_num += 1
        receiver.send(STPPacket(seq_num, ack_num, ack=True, fin=True), addr)
        seq_num += 1
        state = "LAST_ACK"

    if state == "LAST_ACK":
        pkt = receiver.recv()

        if pkt.ack and pkt.ack_num == seq_num:
            receiver.close()
            state = "CLOSED"
            break

receiver.log("")
receiver.log("Amount of Data Received " + str(data_transferred))
receiver.log("Number of Data Segments Received: " + str(data_segments))
receiver.log("Number of duplicate segments received: " + str(duplicate_segments))
