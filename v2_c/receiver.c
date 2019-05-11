/*
 *   Simple Transport Protocol Receiver
 *
 *   sjysd(Tentative)  2019/05/10
 *
 */

#include "STP.c"

int main(int argc, char *argv[]) {

    //判断参数个数
    if (argc != 9) {
        printf("Usage: ./receiver.py file ip port pDrop seedDrop maxDelay pDelay seedDelay");
        return -1;
    }

    //读取参数
    const char *file = argv[1];
    const char *ip = argv[2];
    int port = (int) strtol(argv[3], NULL, 10);
    pDrop = (float) strtod(argv[4], NULL);
    seedDrop = (int) strtol(argv[5], NULL, 10);
    maxDelay = (int) strtol(argv[6], NULL, 10);
    pDelay = (float) strtod(argv[7], NULL);
    seedDelay = (int) strtol(argv[8], NULL, 10);

    //清空目标文件
    FILE *fp = fopen(file, "w");
    fclose(fp);
    int seq_base = 0;

    //buffer 用于储存已接收的包的序号
    const int acked_size = 1024;
    int acked[acked_size];
    int acked_pos = 0;

    //初始化 socket
    stp_init();
    stp_bind(ip, port);
    int established = 0;

    //开始接收
    while (1) {
        //接收一个包，立即确认
        struct stp_vo *pkt = stp_recv();

        //如果是 SYN 包
        if (pkt->packet.SYN) {
            stp_send_empty_pkt(pkt->packet.num + 1, 0, 1, 0, 0);
            if (!established) {
                seq_base = pkt->packet.num + 1;
                established = 1;
            }
            free(pkt);

            //如果是数据包
        } else if (!pkt->packet.FIN && !pkt->packet.SYN) {
            stp_send_empty_pkt(pkt->packet.num + pkt->dataSize, 0, 1, 0, 0);

            //判断是否已收到过此包
            int pos;
            for (pos = acked_pos - 1; pos >= 0; pos--)
                if (acked[pos] == pkt->packet.num)
                    break;

            //如果未收到过此包
            if (pos < 0) {
                //写入数据
                FILE *output_file = fopen(file, "r+");
                fseek(output_file, pkt->packet.num - seq_base, SEEK_SET);
                fwrite(pkt->packet.data, 1, pkt->dataSize, output_file);
                fclose(output_file);

                //序号存入 buffer
                acked[acked_pos++] = pkt->packet.num;
            }

            //释放内存
            free(pkt);

            //如果是 FIN 包
        } else if (pkt->packet.FIN) {

            //阻塞发送最后 ACK
            stp_send_empty_pkt(pkt->packet.num + 1, 0, 1, 0, 1);
            free(pkt);

            //关闭 socket，结束循环
            stp_close();
            break;
        }
    }
}