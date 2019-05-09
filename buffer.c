#include "STP.c"

struct stp_vo* buffer[100];
int buffer_pos = 0;

int buffer_add(struct stp_vo* stp_pkt_vo) {
  buffer[buffer_pos] = stp_pkt_vo;
  buffer_pos++;
}

struct stp_vo* buffer_pop(int pos) {
  struct stp_vo* stp_pkt_vo = buffer[pos];
  for(int i = pos; i < buffer_pos; i++) {
    buffer[i] = buffer[i + 1];
  }
  buffer_pos--;
  return stp_pkt_vo;
}

int buffer_in(int seq_num) {
  int pos;
  for(pos = 0; pos < buffer_pos; pos++) {
    if(buffer[pos]->packet.seq_num == seq_num) {
      return pos;
    }
  }
  if(pos == buffer_pos) {
    return -1;
  }
}