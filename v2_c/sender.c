/*
 *   Simple Transport Protocol Sender
 *
 *   sjysd(Tentative)  2019/05/10
 *
 */

#include "STP.c"

int main(int argc, char *argv[]) {

    //判断参数个数
    if (argc != 12) {
        printf("Usage: ./sender file ip port pDrop seedDrop maxDelay pDelay seedDelay MSS MWS/MSS initalTimeout");
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
    int MSS = (int) strtol(argv[9], NULL, 10);
    int window = (int) strtol(argv[10], NULL, 10);
    int timeout = (int) strtol(argv[11], NULL, 10);

    //读取文件，计算文件长度，读入 all_data
    FILE *fp = fopen(file, "r");
    fseek(fp, 0, SEEK_END);
    int data_len = (int) ftell(fp);
    rewind(fp);
    unsigned char *all_data = malloc(data_len);
    fread(all_data, 1, data_len, fp);
    fclose(fp);

    //buffer 用于储存固定窗口的包
    struct stp_vo **unAcked = malloc(sizeof(struct stp_vo *) * window);
    int unAcked_pos = 0;

    //超时相关
    int SRTT = timeout;
    int DevRTT = 0;
    int RTO = timeout;
    int retry = 0;

    //初始化 socket
    int seq_base = 121;
    int seq_num = seq_base;
    stp_init();
    stp_set_addr(ip, port);
    int established = 0;

    //开始发送
    while (1) {

        //建立连接
        while (!established) {

            //发送 SYN
            stp_send_empty_pkt(seq_num, 1, 0, 0, 0);
            retry++;

            //尝试接收数据
            if (stp_select(RTO << (retry > 3 ? 3 : retry))) {
                struct stp_vo *pkt = stp_recv();
                retry = 0;

                //收到 ACK
                if (pkt->packet.ACK && pkt->packet.num == seq_num + 1) {
                    free(pkt);
                    seq_num++;
                    seq_base++;
                    established = 1;
                }
            }
        }

        //传送数据
        while (seq_num - seq_base < data_len) {

            //这是一个发送窗口内的事件

            //发送此窗口所有的数据
            while (unAcked_pos < window && seq_num - seq_base < data_len) {
                int rest_data = data_len - seq_num + seq_base;
                int data_split_len = rest_data < MSS ? rest_data : MSS;

                //数据打包
                struct stp_packet packet = {seq_num, 0, 0, 0, 0};
                struct stp_vo *pkt = malloc(sizeof(struct stp_vo) + data_split_len);
                pkt->dataSize = data_split_len;
                pkt->packet = packet;
                memcpy(pkt->packet.data, all_data + seq_num - seq_base, data_split_len);

                //发送并添加至 buffer，等待被确认
                unAcked[unAcked_pos++] = pkt;
                stp_pld_send(pkt, 0);
                seq_num += data_split_len;
            }

            //尝试接收数据
            while (unAcked_pos > 0) {
                if (stp_select(0)) {
                    struct stp_vo *pkt = stp_recv();
                    retry = 0;

                    //接收到确认包，从 buffer 中找到对应的数据包
                    int pos;
                    for (pos = unAcked_pos - 1; pos >= 0; pos--)
                        if (unAcked[pos]->packet.num + unAcked[pos]->dataSize == pkt->packet.num)
                            break;

                    if (pkt->packet.ACK && pos >= 0) {

                        //删除对应的数据包
                        struct stp_vo *o_pkt = unAcked[pos];
                        int i;
                        for (i = pos; i < unAcked_pos - 1; i++)
                            unAcked[i] = unAcked[i + 1];
                        unAcked_pos--;

                        //更新超时时间，Jacobson / Karels 算法
                        if (!o_pkt->packet.RET) {
                            int RTT = (int) (pkt->oTime - o_pkt->oTime);
                            SRTT = SRTT + ((RTT - SRTT) >> 3);
                            DevRTT = (3 * DevRTT + abs(RTT - SRTT)) >> 2;
                            RTO = (SRTT + (DevRTT << 2)) ?: 1;
                        }

                        //释放内存
                        free(o_pkt);
                        free(pkt);
                    }
                }

                //检查超时包并重新发送
                int pos;
                for (pos = 0; pos < unAcked_pos; pos++) {
                    if (nowTime() - unAcked[pos]->oTime >= RTO << (retry > 3 ? 3 : retry)) {
                        retry++;
                        unAcked[pos]->packet.RET = 1;
                        stp_pld_send(unAcked[pos], 0);
                    }
                }
            }
        }

        //发送完毕，释放内存
        free(all_data);
        free(unAcked);

        //发送 FIN，最多重试 5 次
        retry = 0;
        while (retry < 5) {
            stp_send_empty_pkt(seq_num, 0, 0, 1, 0);
            retry++;

            //尝试接收数据
            if (stp_select(RTO << (retry > 3 ? 3 : retry))) {
                struct stp_vo *pkt = stp_recv();

                //收到 ACK
                if (pkt->packet.ACK && pkt->packet.num == seq_num + 1) {
                    free(pkt);
                    break;
                }

                free(pkt);
            }
        }

        //关闭 socket，结束循环
        stp_close();
        break;
    }
}