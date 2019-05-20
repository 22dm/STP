/*
 *   Simple Transport Protocol Receiver
 *
 *   sjysd(Tentative)  2019/05/20
 *
 */

#include "STP.c"

int main(int argc, char *argv[])
{
	//判断参数个数
	if (argc != 9) {
		printf("Usage: ./receiver.py file ip port pDrop seedDrop "
		       "maxDelay pDelay seedDelay");
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

	//清空目标文件
	fclose(fopen(file, "w"));
	FILE *output = fopen(file, "r+");

	//初始化 socket
	STPInit();
	STPBind(ip, port);
	int seqBase = 0;
	STPVO *vo = NULL;

	//开始接收
	while (1) {
		//接收一个包，并立即确认，FIN 包需要阻塞确认
		free(vo);
		vo = STPRecv(1, 0);

		if (vo->packet.SYN) {
			//如果是第一次收到 SYN 包，确认并记录 seqBase
			STPSend(vo->packet.num + 1, 0, 1, 0, 0, NULL, 0);
			seqBase = max(seqBase, vo->packet.num + 1);
			continue;
		} else if (vo->packet.FIN) {
			//如果是 FIN 包，确认并关闭 socket，结束循环
			STPSend(vo->packet.num + 1, 0, 1, 0, 0, NULL, 1);
			STPClose();
			break;
		}
		//如果是数据包，确认并写入数据
		STPSend(vo->packet.num + vo->dataSize, 0, 1, 0, 0, NULL, 0);
		fseek(output, vo->packet.num - seqBase, SEEK_SET);
		fwrite(vo->packet.data, 1, vo->dataSize, output);
	}

	fclose(output);
}