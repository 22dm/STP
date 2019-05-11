#include "STP.c"

#define PKT_BUF_SIZE 128

struct stp_vo *pkt_buffer[PKT_BUF_SIZE];
int buffer_pos = 0;

void buffer_add(struct stp_vo *stp_pkt_vo) {
    pkt_buffer[buffer_pos] = stp_pkt_vo;
    buffer_pos++;
}

struct stp_vo *buffer_pop(int pos) {
    struct stp_vo *stp_pkt_vo = pkt_buffer[pos];
    int i;
    for (i = pos; i < buffer_pos; i++) {
        pkt_buffer[i] = pkt_buffer[i + 1];
    }
    buffer_pos--;
    return stp_pkt_vo;
}

int buffer_in(int seq_num) {
    int pos;
    for (pos = 0; pos < buffer_pos; pos++) {
        if (pkt_buffer[pos]->packet.seq_num == seq_num) {
            return pos;
        }
    }
    return -1;
}