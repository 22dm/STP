/*
 *   Simple Transport Protocol Server
 *
 *   sjysd(Tentative)  2019/05/20
 *
 */

#include <libnet.h>
#include <math.h>
#include <pthread.h>

#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))

//接收缓冲区大小
#define BUF_SIZE 65536

// STP 包
typedef struct STPPacket {
	__uint32_t num;
	int SYN : 1;
	int ACK : 1;
	int FIN : 1;
	int RET : 1;
	__uint16_t : 12;
	__uint8_t data[0];
} STPPacket;

// STP 包本地操作对象
typedef struct STPVO {
	__uint32_t dataSize;
	float oTime;
	STPPacket packet;
} STPVO;

// socket 相关
int socketFd;
fd_set sockets;
__uint8_t recvBuf[BUF_SIZE];
struct sockaddr *addr;
socklen_t addrLen = sizeof(struct sockaddr);

// PLD 模块相关
float pDrop;
unsigned seedDrop;
int maxDelay;
float pDelay;
unsigned seedDelay;

//时间相关
struct timeval start, now;

//返回当前程序运行的时间（以毫秒为单位）
float nowTime()
{
	gettimeofday(&now, NULL);
	return (now.tv_sec - start.tv_sec) * 1000 +
	       (float)(now.tv_usec - start.tv_usec) / 1000;
}

//记录日志
void stpLogPkt(const char *action, STPVO *vo)
{
	const char *flag = "";
	if (vo->dataSize > 0)
		flag = "D";
	else if (vo->packet.ACK)
		flag = "A";
	else if (vo->packet.SYN)
		flag = "S";
	else if (vo->packet.FIN)
		flag = "F";
	fprintf(stderr, "%-8s%-2s%-12.3f%-8d%-d\n", action, flag, vo->oTime,
		vo->packet.num, vo->dataSize);
}

//初始化
void STPInit()
{
	gettimeofday(&start, NULL);
	socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

//设置目标 socket
void STPAddr(const char *ip, int port)
{
	addr = calloc(1, addrLen);
	addr->sa_family = AF_INET;
	((struct sockaddr_in *)addr)->sin_addr.s_addr = inet_addr(ip);
	((struct sockaddr_in *)addr)->sin_port = htons(port);
}

//绑定 socket
void STPBind(const char *ip, int port)
{
	STPAddr(ip, port);
	bind(socketFd, addr, addrLen);
}

//接收一个包
STPVO *STPRecv(int block, int timeout)
{
	if (!block) {
		//非阻塞模式，使用 select 查询是否有包可接收
		struct timeval to = {timeout / 1000, timeout % 1000 * 1000};
		FD_SET(socketFd, &sockets);
		//如果没有包可接收，返回 NULL
		if (!select(FD_SETSIZE, &sockets, NULL, NULL, &to))
			return NULL;
	}
	__uint32_t size =
	    recvfrom(socketFd, recvBuf, BUF_SIZE, 0, addr, &addrLen);
	//将接收到的数据打包
	STPVO *vo = malloc(sizeof(STPVO) + size - sizeof(STPPacket));
	vo->dataSize = size - sizeof(STPPacket);
	vo->oTime = nowTime();
	memcpy(&(vo->packet), recvBuf, size);

	stpLogPkt("receive", vo);
	return vo;
}

//子线程传入参数
typedef struct STPPacketDelay {
	STPVO *vo;
	float delay;
} STPPacketDelay;

//发送包（子线程调用）
void *STPSendThread(void *pkt_delay)
{
	STPVO *vo = ((STPPacketDelay *)pkt_delay)->vo;
	usleep((int)(((STPPacketDelay *)pkt_delay)->delay) * 1000);
	sendto(socketFd, &(vo->packet), vo->dataSize + sizeof(STPPacket), 0,
	       addr, addrLen);
	free(((STPPacketDelay *)pkt_delay)->vo);
	free(pkt_delay);
	return NULL;
}

//发送包（使用 STPVO）
void STPSendVO(STPVO *vo, int block)
{
	vo->oTime = nowTime();

	//判断是否丢弃
	if ((seedDrop = rand_r(&seedDrop)) / (float)RAND_MAX < pDrop) {
		stpLogPkt("drop", vo);
		return;
	}
	//计算延迟
	STPPacketDelay *pd = malloc(sizeof(STPPacketDelay));
	pd->vo = malloc(sizeof(STPVO) + vo->dataSize);
	memcpy(pd->vo, vo, sizeof(STPVO) + vo->dataSize);
	seedDelay = rand_r(&seedDelay);
	float uDelay = seedDelay / (float)RAND_MAX;
	pd->delay = fmax((uDelay - 1.0 + pDelay) / pDelay * maxDelay, 0);
	stpLogPkt(pd->delay == 0 ? "send" : "delay", vo);
	//调用子线程处理
	pthread_t th;
	pthread_create(&th, NULL, STPSendThread, pd);
	if (block)
		//如果以阻塞形式运行，则需等待子线程结束
		pthread_join(th, NULL);
	if (vo->dataSize == 0)
		//无数据的包将直接被 free
		free(vo);
}

//发送包（使用详细参数）
STPVO *STPSend(int num, int SYN, int ACK, int FIN, int dataSize,
	       __uint8_t *data, int block)
{
	//数据打包
	STPPacket header = {num, SYN, ACK, FIN, 0};
	STPVO *vo = malloc(sizeof(STPVO) + dataSize);
	vo->dataSize = dataSize;
	vo->packet = header;
	memcpy(vo->packet.data, data, dataSize);

	STPSendVO(vo, block);
	return vo;
}

//关闭 socket
void STPClose() { close(socketFd); }