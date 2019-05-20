/*
 *   Simple Transport Protocol Sender
 *
 *   sjysd(Tentative)  2019/05/20
 *
 */

#include "STP.c"

int main(int argc, char *argv[])
{
	//判断参数个数
	if (argc != 12) {
		printf("Usage: ./sender file ip port pDrop seedDrop maxDelay "
		       "pDelay seedDelay MSS MWS/MSS initalTimeout");
		return -1;
	}

	//读取参数
	const char *file = argv[1];
	const char *ip = argv[2];
	int port = (int)strtol(argv[3], NULL, 10);
	pDrop = (float)strtod(argv[4], NULL);
	seedDrop = (int)strtol(argv[5], NULL, 10);
	maxDelay = (int)strtol(argv[6], NULL, 10);
	pDelay = (float)strtod(argv[7], NULL);
	seedDelay = (int)strtol(argv[8], NULL, 10);
	int MSS = (int)strtol(argv[9], NULL, 10);
	int window = (int)strtol(argv[10], NULL, 10);
	int timeout = (int)strtol(argv[11], NULL, 10);

	//读取文件，计算文件长度，读入 data
	FILE *input = fopen(file, "r");
	fseek(input, 0, SEEK_END);
	int dataRst = (int)ftell(input);
	rewind(input);
	__uint8_t *data = malloc(dataRst);
	fread(data, 1, dataRst, input);
	fclose(input);

	// unAck 用于储存固定窗口的包
	STPVO **unAck = calloc(window, sizeof(STPVO *));
	int unAckN = 0;

	//超时相关
	int SRTT = timeout;
	int DRTT = 0;
	int RTO = timeout;

	//初始化 socket
	int seqBase = rand() % 32768;
	int dataSent = 0;
	STPInit();
	STPAddr(ip, port);
	srand(time(0));
	STPVO *vo = NULL;

	while (1) {
		//建立连接
		STPSend(seqBase, 1, 0, 0, 0, NULL, 0);
		if ((vo = STPRecv(0, RTO)) && vo->packet.ACK &&
		    vo->packet.num == seqBase + 1) {
			free(vo);
			seqBase++;
			break;
		}
		free(vo);
	}

	vo = NULL;

	while (unAckN != 0 || dataRst > 0) {
		//发送数据
		if (unAckN == 0) {
			//上一窗口已传输完毕，发送此窗口所有的数据
			while (unAckN < window && dataRst > 0) {
				int dataSize = min(dataRst, MSS);
				unAck[unAckN++] =
				    STPSend(dataSent + seqBase, 0, 0, 0,
					    dataSize, data + dataSent, 0);
				dataSent += dataSize;
				dataRst -= dataSize;
			}
		}

		//尝试接收包并遍历 unAck
		free(vo);
		vo = STPRecv(0, 0);
		int pos;
		for (pos = 0; pos < window; pos++) {
			if (!unAck[pos])
				continue;
			if (vo && vo->packet.ACK &&
			    unAck[pos]->packet.num + unAck[pos]->dataSize ==
				vo->packet.num) {
				//如果此包已被确认
				if (!unAck[pos]->packet.RET) {
					//不是重传包，更新 RTO，Jacobson-Karels
					int RTT = vo->oTime - unAck[pos]->oTime;
					SRTT = SRTT + (RTT - SRTT) / 8;
					DRTT = (3 * DRTT + abs(RTT - SRTT)) / 4;
					RTO = max(SRTT + DRTT * 4, 10);
				}

				//删除对应的数据包，释放内存
				free(unAck[pos]);
				unAck[pos] = NULL;
				free(vo);
				vo = NULL;
				unAckN--;
			} else if (nowTime() - unAck[pos]->oTime >= RTO) {
				//如果此包超时，重传
				unAck[pos]->packet.RET = 1;
				STPSendVO(unAck[pos], 0);
			}
		}
	}

	//发送完毕，释放内存
	free(data);
	free(unAck);

	int retry = 0;
	while (retry++ < 5) {
		//发送 FIN，最多重试 5 次
		STPSend(seqBase + dataSent, 0, 0, 1, 0, NULL, 0);
		//尝试接收 ACK
		if ((vo = STPRecv(0, RTO)) && vo->packet.ACK &&
		    vo->packet.num == seqBase + dataSent + 1)
			break;
		free(vo);
		vo = NULL;
	}

	//关闭 socket，结束循环
	free(vo);
	STPClose();
}