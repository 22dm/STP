#include "buffer.c"

enum STATE {
    CLOSED, LISTEN, SYN_RECD, ESTABLISHED, CLOSE_WAIT, LAST_ACK
};

int seq_num = 154;
int ack_num = 0;
enum STATE state = CLOSED;

int data_transferred = 0;
int data_segments = 0;
int duplicate_segments = 0;

int port;
const char *file;

const char *log_file = "Receiver_log.txt";

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: ./receiver.py receiver_port file.txt");
        return -1;
    }

    port = (int) strtol(argv[1], NULL, 10);
    file = argv[2];

    FILE *fp = fopen(file, "w");
    fclose(fp);

    while (true) {
        if (state == CLOSED) {
            stp_init();
            stp_bind(port);
            state = LISTEN;
        }

        if (state == LISTEN) {
            struct stp_vo *pkt = stp_recv();

            if (pkt->packet.SYN) {
                ack_num = pkt->packet.seq_num + 1;
                free(pkt);

                stp_send_empty_pkt(seq_num, ack_num, 1, 1, 0);
                seq_num++;
                state = SYN_RECD;
            }
        }

        if (state == SYN_RECD) {
            struct stp_vo *pkt = stp_recv();

            if (pkt->packet.ACK && pkt->packet.ack_num == seq_num) {
                free(pkt);
                state = ESTABLISHED;
            }
        }

        if (state == ESTABLISHED) {
            struct stp_vo *pkt = stp_recv();

            if (!pkt->packet.FIN) {
                if (pkt->packet.seq_num < ack_num || buffer_in(pkt->packet.seq_num) != -1) {
                    free(pkt);
                    duplicate_segments++;
                    continue;
                }

                data_segments++;
                buffer_add(pkt);
                int pos;

                while ((pos = buffer_in(ack_num)) != -1) {
                    struct stp_vo *data_pkt = buffer_pop(pos);
                    ack_num += data_pkt->data_size;
                    data_transferred += data_pkt->data_size;

                    FILE *output_file = fopen(file, "a");
                    fwrite(data_pkt->packet.data, 1, data_pkt->data_size, fp);
                    fclose(output_file);

                    free(data_pkt);
                }

                stp_send_empty_pkt(seq_num, ack_num, 0, 1, 0);
            } else {
                free(pkt);
                state = CLOSE_WAIT;
            }
        }

        if (state == CLOSE_WAIT) {
            ack_num++;
            stp_send_empty_pkt(seq_num, ack_num, 0, 1, 1);
            seq_num++;
            state = LAST_ACK;
        }

        if (state == LAST_ACK) {
            struct stp_vo *pkt = stp_recv();

            if (pkt->packet.ACK && pkt->packet.ack_num == seq_num) {
                stp_close();
                state = CLOSED;
                break;
            }
        }
    }

    stp_log("");
    stp_log("Amount of Data Received: %d", data_transferred);
    stp_log("Number of Data Segments Received: %d", data_segments);
    stp_log("Number of duplicate segments received: %d", duplicate_segments);
}