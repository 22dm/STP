#include <arpa/inet.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define BUF_SIZE 16384
#define LOG_LINE_SIZE 128

struct stp_packet {
    int seq_num;
    int ack_num;
    bool SYN : 1;
    bool ACK : 1;
    bool FIN : 1;
    int : 5;
    unsigned char data[0];
};

struct stp_vo {
    int data_size;
    float opt_time;
    struct stp_packet packet;
};

const char *log_file;
int socket_fd;
unsigned char recv_buffer[BUF_SIZE];
char log_buffer[LOG_LINE_SIZE];
struct sockaddr_in addr;
socklen_t addr_len = sizeof(addr);

void stp_init();

void stp_bind(int port);

int stp_select();

struct stp_vo *stp_recv();

void stp_set_addr(const char *ip, int port);

void stp_send(struct stp_vo *stp_pkt_vo);

void stp_send_empty_pkt(int seq_num, int ack_num, bool SYN, bool ACK, bool FIN);

void stp_drop(struct stp_vo *stp_pkt_vo);

void stp_close();

void stp_log_pkt(const char *action, struct stp_vo *stp_pkt_vo);

void stp_log(const char *format, ...);
