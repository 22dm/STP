/*
 *   Simple Transport Protocol Server
 *
 *   sjysd(Tentative)  2019/05/10
 *
 */

#include <libnet.h>
#include <pthread.h>

#define BUF_SIZE 65536

//STP 包
struct stp_packet {
    __uint32_t num;
    int SYN : 1;
    int ACK : 1;
    int FIN : 1;
    int RET : 1;
    __uint16_t : 12;
    __uint8_t data[0];
};

//STP 包本地操作对象
struct stp_vo {
    __uint32_t dataSize;
    float oTime;
    struct stp_packet packet;
};

//子线程传入参数
struct stp_pkt_delay {
    struct stp_vo *vo;
    float delay;
};

//socket 相关
int socket_fd;
unsigned char recv_buffer[BUF_SIZE];
struct sockaddr_in addr;
socklen_t addr_len = sizeof(addr);

//PLD 模块相关
float pDrop;
unsigned seedDrop;
int maxDelay;
float pDelay;
unsigned seedDelay;

//时间相关
struct timeval start, now;

//返回当前程序运行的时间（以毫秒为单位）
float nowTime() {
    gettimeofday(&now, NULL);
    return (now.tv_sec - start.tv_sec) * 1000 + (float) (now.tv_usec - start.tv_usec) / 1000;
}

//记录日志
void stp_log_pkt(const char *action, struct stp_vo *stp_pkt_vo) {
    const char *stp_flags = stp_pkt_vo->dataSize > 0 ? "D" :
                            stp_pkt_vo->packet.ACK ? "A" :
                            stp_pkt_vo->packet.SYN ? "S" :
                            stp_pkt_vo->packet.FIN ? "F" : "E";

    fprintf(stderr, "%-8s%-2s%-12.3f%-8d%-d\n", action, stp_flags, stp_pkt_vo->oTime,
            stp_pkt_vo->packet.num, stp_pkt_vo->dataSize);
}

//初始化
void stp_init() {
    gettimeofday(&start, NULL);
    socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

//设置目标 socket
void stp_set_addr(const char *ip, int port) {
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons(port);
}

//绑定 socket
void stp_bind(const char *ip, int port) {
    stp_set_addr(ip, port);
    bind(socket_fd, (struct sockaddr *) &addr, addr_len);
}

//查看是否有包可接收
int stp_select(int timeout) {
    static fd_set sockets;
    struct timeval to = {timeout / 1000, timeout % 1000 * 1000};
    FD_SET(socket_fd, &sockets);
    return select(FD_SETSIZE, &sockets, NULL, NULL, &to);
}

//接收一个包
struct stp_vo *stp_recv() {
    __uint32_t size = recvfrom(socket_fd, recv_buffer, BUF_SIZE, 0, (struct sockaddr *) &addr, &addr_len);
    size -= sizeof(struct stp_packet);
    struct stp_vo *stp_pkt_vo = malloc(sizeof(struct stp_vo) + size);
    stp_pkt_vo->dataSize = size;
    stp_pkt_vo->oTime = nowTime();
    memcpy(&(stp_pkt_vo->packet), recv_buffer, size + sizeof(struct stp_packet));

    stp_log_pkt("receive", stp_pkt_vo);
    return stp_pkt_vo;
}

//发送包（子线程调用）
void *stp_send(void *pkt_delay) {
    usleep((int) (((struct stp_pkt_delay *) pkt_delay)->delay) * 1000);
    sendto(socket_fd, &(((struct stp_pkt_delay *) pkt_delay)->vo->packet),
           ((struct stp_pkt_delay *) pkt_delay)->vo->dataSize + sizeof(struct stp_packet), 0,
           (struct sockaddr *) &addr, addr_len);

    if (((struct stp_pkt_delay *) pkt_delay)->vo->dataSize == 0)
        free(((struct stp_pkt_delay *) pkt_delay)->vo);
    free(pkt_delay);

    return NULL;
}

//发送包（使用 pld 模块）
void stp_pld_send(struct stp_vo *stp_pkt_vo, int block) {
    stp_pkt_vo->oTime = nowTime();
    if ((seedDrop = rand_r(&seedDrop)) / (float) RAND_MAX < pDrop)

        //丢弃
        stp_log_pkt("drop", stp_pkt_vo);
    else {

        //建立子线程
        struct stp_pkt_delay *pd = malloc(sizeof(struct stp_pkt_delay));
        pd->vo = stp_pkt_vo;
        float sDelay = ((seedDelay = rand_r(&seedDelay)) / (float) RAND_MAX - 1.0 + pDelay) / pDelay * maxDelay;
        pd->delay = sDelay > 0 ? sDelay : 0;
        stp_log_pkt(pd->delay == 0 ? "send" : "delay", stp_pkt_vo);

        pthread_t th;
        pthread_create(&th, NULL, stp_send, pd);
        if(block)
            pthread_join(th, NULL);
    }
}

//发送数据部分为空的包
void stp_send_empty_pkt(int num, int SYN, int ACK, int FIN, int block) {
    struct stp_packet header = {num, SYN, ACK, FIN, 0};
    struct stp_vo *pkt = malloc(sizeof(struct stp_vo));
    pkt->dataSize = 0;
    pkt->packet = header;
    stp_pld_send(pkt, block);
}

//关闭 socket
void stp_close() { close(socket_fd); }