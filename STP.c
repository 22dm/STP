#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF_SIZE 102400
#define LOG_LINE_SIZE 100

struct stp_packet {
    int seq_num;
    int ack_num;
    int SYN : 1;
    int ACK : 1;
    int FIN : 1;
    int : 5;
    unsigned char data[0];
};

struct stp_vo {
    int data_size;
    float opt_time;
    struct stp_packet packet;
};

const char *log_file;
int sockfd;
unsigned char recv_buffer[BUF_SIZE];
char log_buffer[LOG_LINE_SIZE];
struct sockaddr_in peeraddr;
socklen_t addr_len = sizeof(peeraddr);

int stp_log() {
    puts(log_buffer);
    FILE *fp = fopen(log_file, "a");
    fputs(log_buffer, fp);
    fclose(fp);
}

int stp_log_pkt(const char *action, struct stp_vo *stp_pkt_vo) {
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

    sprintf(log_buffer, "%-5s%-8.2f%-4s%-8d%-6d%-d", action, 1234.5678901, stp_flags, stp_pkt_vo->packet.seq_num,
            stp_pkt_vo->data_size,
            stp_pkt_vo->packet.ack_num);

    stp_log();
}

int stp_init() {
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == -1) {
        perror("create_sockfd error");
    }
    FILE *fp = fopen(log_file, "w");
    fclose(fp);
    return sockfd;
}

int stp_bind(int port) {
    struct sockaddr_in bindaddr;
    memset(&bindaddr, 0, addr_len);
    bindaddr.sin_family = AF_INET;
    bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    bindaddr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *) &bindaddr, addr_len) < 0) {
        perror("bind_sockfd error");
    }
}

struct stp_vo *stp_recv() {
    int nbuf = recvfrom(sockfd, recv_buffer, BUF_SIZE, 0, (struct sockaddr *) &peeraddr, &addr_len);

    struct stp_vo *stp_pkt_vo = malloc(sizeof(int) + sizeof(float) + nbuf);
    stp_pkt_vo->data_size = nbuf - sizeof(struct stp_packet);
    stp_pkt_vo->opt_time = 0;
    memcpy(&(stp_pkt_vo->packet), recv_buffer, nbuf);

    stp_log_pkt("rcv", stp_pkt_vo);
    return stp_pkt_vo;
}


int stp_send(struct stp_vo *stp_pkt_vo) {
    stp_pkt_vo->opt_time = 0;
    sendto(sockfd, &(stp_pkt_vo->packet), stp_pkt_vo->data_size + sizeof(struct stp_packet), 0,
           (struct sockaddr *) &peeraddr, addr_len);
    stp_log_pkt("snd", stp_pkt_vo);
}

int stp_send_empty_pkt(int seq_num, int ack_num, bool SYN, bool ACK, bool FIN) {
    struct stp_packet header = {seq_num, ack_num, SYN, ACK, FIN};
    struct stp_vo* pkt = malloc(sizeof(struct stp_vo));
    pkt->data_size = 0;
    pkt->packet = header;
    stp_send(pkt);
    free(pkt);
}

int stp_drop(struct stp_vo *stp_pkt_vo) {
    stp_pkt_vo->opt_time = 0;
    stp_log_pkt("drop", stp_pkt_vo);
}

int stp_close() { return close(sockfd); }