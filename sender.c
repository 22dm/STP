#include "buffer.c"

enum STATE {
    CLOSED, SYN_SEND, ESTABLISHED, FIN_WAIT, TIME_WAIT
};

int seq_base = 0;
int seq_num = 121;
int ack_num = 0;
int acked_num = 0;
int state = CLOSED;

int data_transferred = 0;
int data_segments = 0;
int data_dropped = 0;
int retransmitted_segments = 0;
int duplicate_ack = 0;

const char *recv_ip;
int recv_port;
const char *file;
int MWS;
int MSS;
float timeout;
float pdrop;
int seed;

const char* log_file = "Sender_log.txt";

unsigned char *all_data;
int data_len;

int pld_send(struct stp_vo *stp_pkt_vo) {
    if (rand() / ((float) (RAND_MAX) + 1) < pdrop) {
        stp_drop(stp_pkt_vo);
        data_dropped += 1;
    } else {
        stp_send(stp_pkt_vo);
    }
}

int min(int a, int b) {
    return a < b ? a : b;
}

int send_seq_data(int pkt_seq_num){
    int pos;
    struct stp_vo* pkt;
    if((pos = buffer_in(pkt_seq_num)) != -1) {
        pkt = buffer_pop(pos);
        retransmitted_segments++;
    } else {
        int data_split_len = min(data_len - seq_num + seq_base, MSS);
        if (data_split_len + seq_num - acked_num <= MWS) {
            struct stp_packet packet = {seq_num, ack_num, 0, 1, 0};
            pkt = malloc(sizeof(struct stp_vo) + data_split_len);
            pkt->data_size = data_split_len;
            pkt->packet = packet;
            memcpy(pkt->packet.data, all_data + seq_num - seq_base, data_split_len);
            data_segments++;
        }
        else {
            return 0;
        }
    }
    pld_send(pkt);
    pkt->opt_time = 0;
    buffer_add(pkt);
    return pkt->data_size;
}

int main(int argc, char* argv[]) {
    if (argc != 9) {
        printf("Usage: sender.py receiver_host_ip receiver_port file.txt MWS MSS timeout pdrop seed");
        return -1;
    }

    recv_ip = argv[1];
    recv_port = atoi(argv[2]);
    file = argv[3];
    MWS = atoi(argv[4]);
    MSS = atoi(argv[5]);
    timeout = atof(argv[6]) / 1000;
    pdrop = atof(argv[7]);
    seed = atoi(argv[8]);

    stp_init();

    memset(&peeraddr, 0, sizeof(peeraddr));
    peeraddr.sin_family = AF_INET;
    inet_pton(AF_INET, recv_ip, &peeraddr.sin_addr);
    peeraddr.sin_port = htons(recv_port);

    srand(seed);

    FILE* fp = fopen(file, "r");
    fseek(fp, 0, SEEK_END);
    data_len = ftell(fp);
    rewind(fp);

    all_data = malloc(data_len);

    fread(all_data, 1, data_len, fp);
    fclose(fp);

    while(1){
        if(state == CLOSED){
            stp_send_empty_pkt(seq_num, ack_num, 1, 0, 0);
            seq_num++;
            state = SYN_SEND;
        }

        if(state == SYN_SEND){
            struct stp_vo* pkt = stp_recv();

            if(pkt->packet.ACK && pkt->packet.SYN && pkt->packet.ack_num == seq_num) {
                ack_num = pkt->packet.seq_num + 1;
                acked_num = pkt->packet.ack_num;
                free(pkt);
                seq_base = seq_num;

                stp_send_empty_pkt(seq_num, ack_num, 0, 1, 0);
                state = ESTABLISHED;
            }
        }

        if(state == ESTABLISHED) {
            int same_ack_num = 0;
            while(acked_num < data_len + seq_base) {
                if(seq_num < data_len + seq_base) {
                    int trans_len = send_seq_data(seq_num);
                    data_transferred += trans_len;
                    seq_num += trans_len;
                }

                //SELECT
                if(true){
                    struct stp_vo* pkt = stp_recv();

                    if(pkt->packet.ack_num > acked_num) {
                        acked_num = pkt->packet.ack_num;
                        same_ack_num = 1;
                    } else if (pkt->packet.ack_num == acked_num) {
                        duplicate_ack += 1;
                        same_ack_num += 1;
                        if(same_ack_num >= 3){
                            send_seq_data(acked_num);
                        }
                    }

                    free(pkt);
                }

                //TIME
                float now_time = 0;

                int pos = 0;
                while(pos < buffer_pos) {
                    if(buffer[pos] ->packet.seq_num < acked_num){
                        struct stp_vo* pkt = buffer_pop(pos);
                        free(pkt);
                    } else if (now_time - buffer[pos]->opt_time >= timeout){
                        send_seq_data(buffer[pos] ->packet.seq_num);
                        pos++;
                    } else {
                        pos++;
                    }
                }
            }

            free(all_data);

            stp_send_empty_pkt(seq_num, ack_num, 0, 0, 1);
            seq_num++;
            state = FIN_WAIT;
        }

        if(state == FIN_WAIT) {
            struct stp_vo* pkt = stp_recv();

            if(pkt->packet.FIN && pkt->packet.ACK && pkt->packet.ack_num == seq_num) {
                free(pkt);
                ack_num ++;

                stp_send_empty_pkt(seq_num, ack_num, 0, 1, 0);
                state = TIME_WAIT;
            }
        }

        if(state == TIME_WAIT) {
            stp_close();
            state = CLOSED;
            break;
        }
    }

    sprintf(log_buffer, "\n");
    stp_log();
    sprintf(log_buffer, "Amount of Data Transferred: %d", data_transferred);
    stp_log();
    sprintf(log_buffer, "Number of Data Segments Sent: %d", data_segments);
    stp_log();
    sprintf(log_buffer, "Number of Packets Dropped: %d", data_dropped);
    stp_log();
    sprintf(log_buffer, "Number of Retransmitted Segments: %d", retransmitted_segments);
    stp_log();
    sprintf(log_buffer, "Number of Duplicate Ack received: %d", duplicate_ack);
    stp_log();

}
