#include "STP.h"

void stp_init() {
    socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    FILE *fp = fopen(log_file, "w");
    fclose(fp);
}

void stp_bind(int port) {
    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, addr_len);
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = htons(port);
    bind(socket_fd, (struct sockaddr *) &bind_addr, addr_len);
}

int stp_select() {
    static fd_set sockets;
    static struct timeval zero;
    memset(&zero, 0, sizeof(zero));

    FD_SET(socket_fd, &sockets);

    return select(FD_SETSIZE, &sockets, NULL, NULL, &zero);
}

struct stp_vo *stp_recv() {
    size_t size = recvfrom(socket_fd, recv_buffer, BUF_SIZE, 0, (struct sockaddr *) &addr, &addr_len);
    size -= sizeof(struct stp_packet);
    struct stp_vo *stp_pkt_vo = malloc(sizeof(struct stp_vo) + size);
    stp_pkt_vo->data_size = size;
    stp_pkt_vo->opt_time = (float) clock() / CLOCKS_PER_SEC * 1000;
    memcpy(&(stp_pkt_vo->packet), recv_buffer, size + sizeof(struct stp_packet));

    stp_log_pkt("rcv", stp_pkt_vo);
    return stp_pkt_vo;
}


void stp_set_addr(const char *ip, int port) {
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons(port);
}

void stp_send(struct stp_vo *stp_pkt_vo) {
    stp_pkt_vo->opt_time = (float) clock() / CLOCKS_PER_SEC * 1000;
    sendto(socket_fd, &(stp_pkt_vo->packet), stp_pkt_vo->data_size + sizeof(struct stp_packet), 0,
           (struct sockaddr *) &addr, addr_len);
    stp_log_pkt("snd", stp_pkt_vo);
}

void stp_send_empty_pkt(int seq_num, int ack_num, bool SYN, bool ACK, bool FIN) {
    struct stp_packet header = {seq_num, ack_num, SYN, ACK, FIN};
    struct stp_vo *pkt = malloc(sizeof(struct stp_vo));
    pkt->data_size = 0;
    pkt->packet = header;
    stp_send(pkt);
    free(pkt);
}

void stp_drop(struct stp_vo *stp_pkt_vo) {
    stp_pkt_vo->opt_time = (float) clock() / CLOCKS_PER_SEC * 1000;
    stp_log_pkt("drop", stp_pkt_vo);
}

void stp_close() { close(socket_fd); }

void stp_log_pkt(const char *action, struct stp_vo *stp_pkt_vo) {
    const char *stp_flags;
    bool SYN = stp_pkt_vo->packet.SYN;
    bool ACK = stp_pkt_vo->packet.ACK;
    bool FIN = stp_pkt_vo->packet.FIN;

    if (stp_pkt_vo->data_size > 0) {
        stp_flags = "D";
    } else if (ACK) {
        if (SYN) {
            stp_flags = "SA";
        } else if (FIN) {
            stp_flags = "FA";
        } else {
            stp_flags = "A";
        }
    } else if (SYN) {
        stp_flags = "S";
    } else if (FIN) {
        stp_flags = "F";
    } else {
        stp_flags = "ERR";
    }

    stp_log("%-5s%-9.3f%-4s%-8d%-6d%-d", action, stp_pkt_vo->opt_time, stp_flags,
            stp_pkt_vo->packet.seq_num, stp_pkt_vo->data_size, stp_pkt_vo->packet.ack_num);
}

void stp_log(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    vsprintf(log_buffer, format, ap);
    va_end(ap);

    puts(log_buffer);
    FILE *fp = fopen(log_file, "a");
    fputs(log_buffer, fp);
    fclose(fp);
}