#include "buffer.c"

enum STATE { CLOSED, LISTEN, SYN_RECD, ESTABLISHED, CLOSE_WAIT, LAST_ACK };

int seq_num = 154;
int ack_num = 0;
enum STATE state = CLOSED;

int data_transferred = 0;
int data_segments = 0;
int duplicate_segments = 0;

int port;
const char* file;

const char* log_file = "Receiver_log.txt";

int main(int argc, char* argv[]) {
  if (argc != 3) {
    printf("Usage: receiver.py receiver_port file.txt");
    return -1;
  }

  port = atoi(argv[1]);
  file = argv[2];

  FILE* fp = fopen(file, "w");
  fclose(fp);

  while (1) {
    if (state == CLOSED) {
      stp_init();
      stp_bind(port);
      state = LISTEN;
    }

    if (state == LISTEN) {
      struct stp_vo* pkt = stp_recv();

      if (pkt->packet.SYN) {
        ack_num = pkt->packet.seq_num + 1;
        free(pkt);

        stp_send_empty_pkt(seq_num, ack_num, 1, 1, 0);
        seq_num += 1;
        state = SYN_RECD;
      }
    }

    if (state == SYN_RECD) {
      struct stp_vo* pkt = stp_recv();

      if (pkt->packet.ACK && pkt->packet.ack_num == seq_num) {
        free(pkt);
        state = ESTABLISHED;
      }
    }

    if (state == ESTABLISHED) {
      struct stp_vo* pkt = stp_recv();

      if (!pkt->packet.FIN) {
        if (pkt->packet.seq_num < ack_num || buffer_in(pkt->packet.seq_num) != -1) {
          free(pkt);
          duplicate_segments += 1;
          continue;
        }
        data_segments += 1;
        buffer_add(pkt);
        int pos;
        while ((pos = buffer_in(ack_num)) != -1) {
          struct stp_vo* pkt = buffer_pop(pos);
          ack_num += pkt->data_size;
          data_transferred += pkt->data_size;
          FILE* fp = fopen(file, "a");
          fwrite(pkt->packet.data, 1, pkt->data_size, fp);
          fclose(fp);
          free(pkt);
        }

        stp_send_empty_pkt(seq_num, ack_num, 0, 1, 0);
      } else {
        free(pkt);
        state = CLOSE_WAIT;
      }
    }

    if (state == CLOSE_WAIT) {
      ack_num += 1;
      stp_send_empty_pkt(seq_num, ack_num, 0, 1, 1);
      seq_num += 1;
      state = LAST_ACK;
    }

    if (state == LAST_ACK) {
      struct stp_vo* pkt = stp_recv();

      if (pkt->packet.ACK && pkt->packet.ack_num == seq_num) {
        stp_close();
        state = CLOSED;
        break;
      }
    }
  }

    sprintf(log_buffer, "\n");
    stp_log();
    sprintf(log_buffer, "Amount of Data Received: %d", data_transferred);
    stp_log();
    sprintf(log_buffer, "Number of Data Segments Received: %d", data_segments);
    stp_log();
    sprintf(log_buffer, "Number of duplicate segments received: %d", duplicate_segments);
    stp_log();
}