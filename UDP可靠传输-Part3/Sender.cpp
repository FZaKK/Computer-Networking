#include <winsock2.h>
#include <Ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <string>
#include <Windows.h>
#include <time.h>
#include <queue>
using namespace std;

#define WIN32_LEAN_AND_MEAN
#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT 27015
#define DEFAULT_BUFLEN 4096 // 2^12大小
#define DEFAULT_SEQNUM 65536
#define UDP_LEN sizeof(my_udp)
#define MAX_FILESIZE 1024 * 1024 * 10
#define MAX_TIME 0.2 * CLOCKS_PER_SEC // 超时重传

// 连接上ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

const uint16_t SYN = 0x1; // SYN = 1 ACK = 0 FIN = 0
const uint16_t ACK = 0x2; // SYN = 0, ACK = 1，FIN = 0
const uint16_t SYN_ACK = 0x3; // SYN = 1, ACK = 1
const uint16_t START = 0x10;  // 标记文件的第一个数据包，也就是文件名
const uint16_t OVER = 0x8;  // 标记最后一个数据包
const uint16_t FIN = 0x4; // FIN = 1 ACK = 0
const uint16_t FIN_ACK = 0x6; // FIN = 1 ACK = 1
const uint16_t START_OVER = 0x18; // START = 1 OVER = 1

uint16_t seq_order = 0;
uint16_t stream_seq_order = 0;
long file_size = 0;

struct HEADER {
    uint16_t datasize;
    uint16_t cksum;
    // 这里需要注意的是，顺序为STREAM SYN ACK FIN
    uint16_t Flag; // 使用8位的话，就很麻烦，uint16_t去指向0x7+0x0结果还是0x7
    uint16_t STREAM_SEQ;
    uint16_t SEQ;

    // 初始化函数，STREAM标记的是文件的开始与结束（应当在应用层设计，这里就放到udp里标识吧）
    // 开始的时候需要把文件名传输过去，所以给两位标志位分别为START OVER
    // 000 START OVER FIN ACK SYN
    HEADER() {
        this->datasize = 0;
        this->cksum = 0;
        this->Flag = 0;
        this->STREAM_SEQ = 0;
        this->SEQ = 0;
    }
    
    HEADER(uint16_t datasize, uint16_t cksum, uint16_t Flag, uint16_t STREAM_SEQ, uint16_t SEQ) {
        this->datasize = datasize;
        this->cksum = cksum;
        this->Flag = Flag;
        this->STREAM_SEQ = STREAM_SEQ;
        this->SEQ = SEQ;
    }

    void set_value(uint16_t datasize, uint16_t cksum, uint16_t Flag, uint16_t STREAM_SEQ, uint16_t SEQ) {
        this->datasize = datasize;
        this->cksum = cksum;
        this->Flag = Flag;
        this->STREAM_SEQ = STREAM_SEQ;
        this->SEQ = SEQ;
    }
};

class my_udp {
public:
    HEADER udp_header;
    char buffer[DEFAULT_BUFLEN] = ""; // 这里可以+1设置\0，也可以不+1
public:
    my_udp() {};
    my_udp(HEADER& header);
    my_udp(HEADER& header, string data_segment);
    void set_value(HEADER header, char* data_segment, int size); // 这里一定要注意
};

// 针对三次握手和四次挥手的初始化函数
my_udp::my_udp(HEADER& header) {
    udp_header = header;
};

my_udp::my_udp(HEADER& header, string data_segment) {
    udp_header = header;
    for (int i = 0; i < data_segment.length(); i++) {
        buffer[i] = data_segment[i];
    }
    buffer[data_segment.length()] = '\0';
};

void my_udp::set_value(HEADER header, char* data_segment , int size) {
    udp_header = header;
    memcpy(buffer, data_segment, size);
}

// 计算校验和，这里的字符数组运算过程，充分展现了windows小端存储的特点
uint16_t checksum(uint16_t* udp, int size) {
    int count = (size + 1) / 2;
    uint16_t* buf = (uint16_t*)malloc(size); // 可以+1也可以不+1
    memset(buf, 0, size);
    memcpy(buf, udp, size);
    u_long sum = 0;
    while (count--) {
        sum += *buf++;
        if (sum & 0xffff0000) {
            sum &= 0xffff;
            sum++;
        }
    }
    return ~(sum & 0xffff);
}

// 检验全局变量的序列号有没有超过最大值
void check_seq() {
    if (seq_order >= DEFAULT_SEQNUM) {
        seq_order = seq_order % DEFAULT_SEQNUM;
    }
}

void check_stream_seq() {
    if (stream_seq_order >= DEFAULT_SEQNUM) {
        stream_seq_order = stream_seq_order % DEFAULT_SEQNUM;
    }
}

void print_Send_information(my_udp& udp2show, string statue) {
    cout << statue << " Message " << udp2show.udp_header.datasize << " bytes!";
    cout << " Flag:" << udp2show.udp_header.Flag << " STREAM_SEQ:" << udp2show.udp_header.STREAM_SEQ << " SEQ:" << udp2show.udp_header.SEQ;
    cout << " Check Sum:" << udp2show.udp_header.cksum << endl;
}

void send_packet_GBN(my_udp& Packet, SOCKET& SendSocket, sockaddr_in& RecvAddr) {
    int iResult;
    int RecvAddrSize = sizeof(RecvAddr);
    char* SendBuf = new char[UDP_LEN];

    memcpy(SendBuf, &Packet, UDP_LEN);
    iResult = sendto(SendSocket, SendBuf, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
    if (iResult == SOCKET_ERROR) {
        cout << "Sendto failed with error: " << WSAGetLastError() << endl;
    }

    delete[] SendBuf;
}


// 返回值设成ERROR_CODE试试，要使用memcpy把整体发过去
void send_packet(my_udp& Packet, SOCKET& SendSocket, sockaddr_in& RecvAddr) {
    int iResult;
    int RecvAddrSize = sizeof(RecvAddr);
    my_udp Recv_udp;
    char* SendBuf = new char[UDP_LEN];
    char* RecvBuf = new char[UDP_LEN];
    memcpy(SendBuf, &Packet, UDP_LEN);
    iResult = sendto(SendSocket, SendBuf, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
    if (iResult == SOCKET_ERROR) {
        cout << "Sendto failed with error: " << WSAGetLastError() << endl;
    }

    // 测试一下文件的各个内容
    print_Send_information(Packet, "Send");

    // 记录发送时间，超时重传
    // 等待接收ACK信息，验证序列号
    clock_t start = clock();

    while (true) {
        while (recvfrom(SendSocket, RecvBuf, UDP_LEN, 0, (sockaddr*)&RecvAddr, &RecvAddrSize) <= 0) {
            if (clock() - start > MAX_TIME) {
                cout << "*** TIME OUT! ReSend Message *** " << endl;

                // 仅作为调试
                Packet.udp_header.SEQ = seq_order;
                memcpy(SendBuf, &Packet, UDP_LEN);
                iResult = sendto(SendSocket, SendBuf, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));

                print_Send_information(Packet, "ReSend");

                start = clock(); // 重设开始时间
                if (iResult == SOCKET_ERROR) {
                    cout << "Sendto failed with error: " << WSAGetLastError() << endl;
                }
            }
        }

        // 三个条件要同时满足，这里调试时判断用packet的seq
        memcpy(&Recv_udp, RecvBuf, UDP_LEN);
        if (Recv_udp.udp_header.SEQ == Packet.udp_header.SEQ && Recv_udp.udp_header.Flag == ACK && checksum((uint16_t*)&Recv_udp, UDP_LEN) == 0) {
            cout << "Send has been confirmed! Flag:" << Recv_udp.udp_header.Flag << " STREAM_SEQ:" << Recv_udp.udp_header.STREAM_SEQ << " SEQ:" << Recv_udp.udp_header.SEQ << endl;
            seq_order++; // 全局变量的序列号
            check_seq();
            break;
        }
        else {
            continue;
        }
    }

    delete[] SendBuf; // ?
    delete[] RecvBuf;
}

// GBN部分的全局变量
int N = 1; // 窗口大小
uint16_t base = 1; // 初始为1
uint16_t next_seqnum = 1;
int ACK_index = 0; // 判断结束
queue<my_udp> message_queue;

void GBN_init() {
    base = 1;
    next_seqnum = 1;
    ACK_index = 0;
    N = 5;
}

// Reno的一些全局变量
int cwnd = 1;
int recv_window = 2560; // 根据接收端的缓冲区计算得，每次接收到ACK -1
int ssthresh = 16; // 阈值
int dupACKcount = 0; // 重复ACK计数
int RenoState = 0; // 标志状态机  0：慢启动 1：拥塞避免 2：快速恢复
uint16_t recover = 1;

void Reno_init() {
    base = 1;
    next_seqnum = 1;
    ACK_index = 0;
    cwnd = 1;
    recv_window = 2560;
    dupACKcount = 0;
    RenoState = 0;
}

// 拥塞控制的Reno发送文件函数
void send_file_Reno(string filename, SOCKET& SendSocket, sockaddr_in& RecvAddr) {
    // 每次文件发送预先初始化
    Reno_init();
    int RecvAddrSize = sizeof(RecvAddr);

    ifstream fin(filename.c_str(), ifstream::binary);
    fin.seekg(0, std::ifstream::end);
    long size = fin.tellg();
    file_size = size;
    fin.seekg(0);

    char* binary_file_buf = new char[size];
    cout << " ** 文件大小：" << size << " bytes" << endl;
    fin.read(&binary_file_buf[0], size);
    fin.close();

    HEADER udp_header(filename.length(), 0, START, stream_seq_order, 0);
    my_udp udp_packets(udp_header, filename.c_str());
    uint16_t check = checksum((uint16_t*)&udp_packets, UDP_LEN); // 计算校验和
    udp_packets.udp_header.cksum = check;
    int packet_num = size / DEFAULT_BUFLEN + 1;
    cout << " ** 文件名校验和：" << check << endl;
    cout << " ** 发送数据包的数量：" << packet_num << endl;
    cout << " ** Windows窗口大小：" << N << endl;

    // 正常发送第一个文件名数据包
    send_packet(udp_packets, SendSocket, RecvAddr);

    clock_t start; // 超时重传计时器
    char* RecvBuf = new char[UDP_LEN];
    my_udp Recv_udp;
    start = clock();

    // 处理SEQ回环，mod运算，商和余数
    // uint16_t quotient = 0;
    uint16_t remainder = 0;
    int RTT_ACK = 0; 
    while (ACK_index < packet_num) {
        if (next_seqnum < base + N && next_seqnum <= packet_num) {
            // quotient = next_seqnum / DEFAULT_SEQNUM;
            remainder = next_seqnum % DEFAULT_SEQNUM;
            if (next_seqnum == packet_num) {
                udp_header.set_value(size - (next_seqnum - 1) * DEFAULT_BUFLEN, 0, OVER, stream_seq_order, remainder);
                udp_packets.set_value(udp_header, binary_file_buf + (next_seqnum - 1) * DEFAULT_BUFLEN, size - (next_seqnum - 1) * DEFAULT_BUFLEN);
                check = checksum((uint16_t*)&udp_packets, UDP_LEN);
                udp_packets.udp_header.cksum = check;

                // cout << "send window size: " << N << endl;
                send_packet_GBN(udp_packets, SendSocket, RecvAddr);
                print_Send_information(udp_packets, "Send");
                message_queue.push(udp_packets);
                next_seqnum++;
            }
            else {
                udp_header.set_value(DEFAULT_BUFLEN, 0, 0, stream_seq_order, remainder);
                udp_packets.set_value(udp_header, binary_file_buf + (next_seqnum - 1) * DEFAULT_BUFLEN, DEFAULT_BUFLEN);
                check = checksum((uint16_t*)&udp_packets, UDP_LEN);
                udp_packets.udp_header.cksum = check;

                // cout << "send window size: " << N << endl;
                send_packet_GBN(udp_packets, SendSocket, RecvAddr);
                print_Send_information(udp_packets, "Send");
                message_queue.push(udp_packets);
                next_seqnum++;
            }
        }

        // 超时重传计时器超时了，也就是说很久没收到ACK数据包了
        if (clock() - start > MAX_TIME) {
            cout << "*** TIME OUT! ReSend Message *** " << endl;
            ssthresh = cwnd / 2; // 阈值
            cwnd = 1;
            dupACKcount = 0; // 重新计数
            N = min(cwnd, recv_window);
            RenoState = 0; // 慢启动阶段
            cout << "*** 进入慢启动阶段 窗口大小重设为" << N << " ***" << endl;
            start = clock();

            // 依旧是超时重传，全部重发 !!
            for (int i = 0; i < message_queue.size(); i++) {
                send_packet_GBN(message_queue.front(), SendSocket, RecvAddr);
                print_Send_information(message_queue.front(), "ReSend");
                message_queue.push(message_queue.front());
                message_queue.pop();
                // Sleep(10);
            }
        }

        // 循环之中也接收ACK消息，可封装
        if (recvfrom(SendSocket, RecvBuf, UDP_LEN, 0, (sockaddr*)&RecvAddr, &RecvAddrSize) > 0) {
            memcpy(&Recv_udp, RecvBuf, UDP_LEN);

            // 没有校验和的错误
            if (Recv_udp.udp_header.Flag == ACK && checksum((uint16_t*)&Recv_udp, UDP_LEN) == 0) {
                switch (RenoState) {
                // 慢启动
                case 0:
                    cout << "***** 慢启动阶段 *****" << endl;
                    cout << "Send Window Size: " << N << endl;
                    cout << "base: " << base << endl;
                    cout << "nextseqnum " << next_seqnum << endl;
                    // 丢弃重复响应的ACK，取模防止回环问题
                    if ((base % DEFAULT_SEQNUM) == Recv_udp.udp_header.SEQ) {
                        if (cwnd < ssthresh) {
                            cwnd++;
                            N = min(cwnd, recv_window); // 需要更新发送窗口大小
                            cout << "*** 慢启动阶段接收到新ACK，cwnd++ ***" << endl;
                        }
                        else {
                            RenoState = 1;
                            cout << "*** 慢启动阶段结束，进入拥塞避免阶段 ***" << endl;
                        }

                        dupACKcount = 0; // 接收新的ACK，dup=0
                        base = base + 1; // 确认一个移动一个位置
                        cout << "Send has been confirmed! Flag:" << Recv_udp.udp_header.Flag;
                        cout << " STREAM_SEQ:" << Recv_udp.udp_header.STREAM_SEQ << " SEQ:" << Recv_udp.udp_header.SEQ << endl;
                        recv_window--;
                        N = min(cwnd, recv_window);
                        ACK_index++;
                        message_queue.pop();
                        start = clock();
                    }
                    // 重复的ACK
                    else {
                        dupACKcount++;
                        cout << "Repetitive ACK! Flag:" << Recv_udp.udp_header.Flag;
                        cout << " STREAM_SEQ:" << Recv_udp.udp_header.STREAM_SEQ << " SEQ:" << Recv_udp.udp_header.SEQ << endl;
                        cout << "dupACKcount: " << dupACKcount << endl;
                    }
                    // 重复三次ACK
                    if (dupACKcount == 3) {
                        // 检测到丢包，开始提前重传
                        cout << "*** ACK重复三次，开始重传 ***" << endl;
                        start = clock();
                        for (int i = 0; i < message_queue.size(); i++) {
                            send_packet_GBN(message_queue.front(), SendSocket, RecvAddr);
                            print_Send_information(message_queue.front(), "ReSend");
                            message_queue.push(message_queue.front());
                            message_queue.pop();
                            // Sleep(10);
                        }
                        // 快速恢复阶段
                        ssthresh = cwnd / 2;
                        cwnd = ssthresh + 3;
                        RenoState = 2; 
                        recover = message_queue.back().udp_header.SEQ;
                        dupACKcount = 0;
                        N = min(cwnd, recv_window); // 需要更新发送窗口大小
                    }
                    break;
                
                // 拥塞避免阶段
                case 1:
                    cout << "***** 拥塞避免阶段 *****" << endl;
                    cout << "Send Window Size: " << N << endl;
                    // cout << "RTT_ACK: " << RTT_ACK << endl;
                    cout << "base: " << base << endl;
                    cout << "nextseqnum " << next_seqnum << endl;
                    // 丢弃重复响应的ACK，取模防止回环问题
                    if ((base % DEFAULT_SEQNUM) == Recv_udp.udp_header.SEQ) {
                        RTT_ACK++;
                        // 这里设置cwnd或者给定值应该都可以，但是cwnd快
                        if (RTT_ACK == cwnd) {
                            cwnd++;
                            N = min(cwnd, recv_window); // 需要更新发送窗口大小
                            RTT_ACK = 0;
                            cout << "*** 拥塞避免阶段，线性递增+1 ***" << endl;
                        }

                        dupACKcount = 0; // 接收新的ACK，dup=0
                        base = base + 1; // 确认一个移动一个位置
                        cout << "Send has been confirmed! Flag:" << Recv_udp.udp_header.Flag;
                        cout << " STREAM_SEQ:" << Recv_udp.udp_header.STREAM_SEQ << " SEQ:" << Recv_udp.udp_header.SEQ << endl;
                        recv_window--;
                        N = min(cwnd, recv_window);
                        ACK_index++;
                        message_queue.pop();
                        start = clock();
                    }
                    // 重复的ACK
                    else {
                        dupACKcount++;
                        RTT_ACK = 0; // 保证RTT_ACK值的正确性
                        cout << "Repetitive ACK! Flag:" << Recv_udp.udp_header.Flag;
                        cout << " STREAM_SEQ:" << Recv_udp.udp_header.STREAM_SEQ << " SEQ:" << Recv_udp.udp_header.SEQ << endl;
                        cout << "dupACKcount: " << dupACKcount << endl;
                    }
                    if (dupACKcount == 3) {
                        // 检测到丢包，开始提前重传
                        cout << "*** ACK重复三次，开始重传 ***" << endl;
                        start = clock();
                        for (int i = 0; i < message_queue.size(); i++) {
                            send_packet_GBN(message_queue.front(), SendSocket, RecvAddr);
                            print_Send_information(message_queue.front(), "ReSend");
                            message_queue.push(message_queue.front());
                            message_queue.pop();
                            // Sleep(10);
                        }
                        ssthresh = cwnd / 2;
                        cwnd = ssthresh + 3;
                        RenoState = 2; // 快速恢复阶段
                        recover = message_queue.back().udp_header.SEQ; // 设置New Reno的覆盖
                        RTT_ACK = 0;
                        dupACKcount = 0;
                        N = min(cwnd, recv_window); // 需要更新发送窗口大小
                    }
                    break;
                
                // 快速恢复阶段
                case 2: 
                    cout << "***** 快速恢复阶段 *****" << endl;
                    cout << "Send Window Size: " << N << endl;
                    cout << "base: " << base << endl;
                    cout << "nextseqnum " << next_seqnum << endl;
                    // 所以要保留之前传输的最大序列号
                    // uint16_t recover = next_seqnum - 1;
                    if ((base % DEFAULT_SEQNUM) == Recv_udp.udp_header.SEQ) {
                        // 新的ACK，New Reno会去判断是否发送的消息都已经被确认
                        if (Recv_udp.udp_header.SEQ < recover) {
                            RenoState = 2; // 维持快速恢复
                        }
                        else {
                            RenoState = 1; // 进入拥塞避免阶段
                            cwnd = ssthresh;
                            N = min(cwnd, recv_window); // 需要更新发送窗口大小
                            // dupACKcount = 0;
                        }

                        dupACKcount = 0;
                        base = base + 1; // 确认一个移动一个位置
                        cwnd++; // 成功接收就是网络状态好呗
                        cout << "Send has been confirmed! Flag:" << Recv_udp.udp_header.Flag;
                        cout << " STREAM_SEQ:" << Recv_udp.udp_header.STREAM_SEQ << " SEQ:" << Recv_udp.udp_header.SEQ << endl;
                        recv_window--;
                        N = min(cwnd, recv_window);
                        ACK_index++;
                        message_queue.pop();
                        start = clock();
                    }
                    else {
                        // 要是这块儿卡住了，要是这块儿卡住了就返回慢启动吧(不行）
                        // 要是不返回慢启动的话，会导致快速恢复阶段时间过长，阻塞窗口过大
                        dupACKcount++;
                        /*
                        // 这儿真的有待思考
                        if (dupACKcount <= 3)
                            cwnd++;
                        else;
                        */
                        N = min(cwnd, recv_window); // 需要更新发送窗口大小
                        cout << "*** 快速恢复阶段，cwnd: " << cwnd << endl;
                        cout << "Repetitive ACK! Flag:" << Recv_udp.udp_header.Flag;
                        cout << " STREAM_SEQ:" << Recv_udp.udp_header.STREAM_SEQ << " SEQ:" << Recv_udp.udp_header.SEQ << endl;
                    }
                    // 重复六次ACK，宽容一点
                    if (dupACKcount == 6) {
                        // 检测到快速恢复阶段还在丢包，那么直接进入慢启动
                        cout << "*** ACK重复六次，进入慢启动 ***" << endl;
                        ssthresh = cwnd / 2; // 阈值
                        cwnd = 1;
                        dupACKcount = 0; // 重新计数
                        N = min(cwnd, recv_window);
                        RenoState = 0; // 慢启动阶段
                        cout << "*** 重新进入慢启动阶段 窗口大小重设为" << N << " ***" << endl;
                        start = clock();

                        for (int i = 0; i < message_queue.size(); i++) {
                            send_packet_GBN(message_queue.front(), SendSocket, RecvAddr);
                            print_Send_information(message_queue.front(), "ReSend");
                            message_queue.push(message_queue.front());
                            message_queue.pop();
                            // Sleep(10);
                        }
                    }
                    break; 

                default:
                    cout << "Error RenoState!!!" << endl;
                    break;
                }
            }
            else;
        }
    }

    cout << "----- ***对方已成功接收文件！***----- " << endl << endl;
    stream_seq_order++;
    check_stream_seq();
    delete[] binary_file_buf;
}

// GBN的发送文件函数
void send_file_GBN(string filename, SOCKET& SendSocket, sockaddr_in& RecvAddr) {
    // 每次文件发送预先初始化
    GBN_init();
    int RecvAddrSize = sizeof(RecvAddr);

    ifstream fin(filename.c_str(), ifstream::binary);
    fin.seekg(0, std::ifstream::end);
    long size = fin.tellg();
    file_size = size;
    fin.seekg(0);

    char* binary_file_buf = new char[size];
    cout << " ** 文件大小：" << size << " bytes" << endl;
    fin.read(&binary_file_buf[0], size);
    fin.close();

    HEADER udp_header(filename.length(), 0, START, stream_seq_order, 0);
    my_udp udp_packets(udp_header, filename.c_str());
    uint16_t check = checksum((uint16_t*)&udp_packets, UDP_LEN); // 计算校验和
    udp_packets.udp_header.cksum = check;

    int packet_num = size / DEFAULT_BUFLEN + 1;
    cout << " ** 文件名校验和：" << check << endl;
    cout << " ** 发送数据包的数量：" << packet_num << endl;
    cout << " ** Windows窗口大小：" << N << endl;
    
    // 正常发送第一个文件名数据包
    send_packet(udp_packets, SendSocket, RecvAddr);

    clock_t start;
    char* RecvBuf = new char[UDP_LEN];
    my_udp Recv_udp;
    start = clock();

    // 处理SEQ回环，mod运算，商和余数
    // uint16_t quotient = 0;
    uint16_t remainder = 0;
    while (ACK_index < packet_num) {
        if (next_seqnum < base + N && next_seqnum <= packet_num) {
            // quotient = next_seqnum / DEFAULT_SEQNUM;
            remainder = next_seqnum % DEFAULT_SEQNUM;
            if (next_seqnum == packet_num) {
                udp_header.set_value(size - (next_seqnum - 1) * DEFAULT_BUFLEN, 0, OVER, stream_seq_order, remainder);
                udp_packets.set_value(udp_header, binary_file_buf + (next_seqnum - 1) * DEFAULT_BUFLEN, size - (next_seqnum - 1) * DEFAULT_BUFLEN);
                check = checksum((uint16_t*)&udp_packets, UDP_LEN);
                udp_packets.udp_header.cksum = check;

                send_packet_GBN(udp_packets, SendSocket, RecvAddr);
                print_Send_information(udp_packets, "Send");
                message_queue.push(udp_packets);
                next_seqnum++;
            }
            else {
                udp_header.set_value(DEFAULT_BUFLEN, 0, 0, stream_seq_order, remainder);
                udp_packets.set_value(udp_header, binary_file_buf + (next_seqnum - 1) * DEFAULT_BUFLEN, DEFAULT_BUFLEN);
                check = checksum((uint16_t*)&udp_packets, UDP_LEN);
                udp_packets.udp_header.cksum = check;

                send_packet_GBN(udp_packets, SendSocket, RecvAddr);
                print_Send_information(udp_packets, "Send");
                message_queue.push(udp_packets);
                next_seqnum++;
            }
        }

        if (clock() - start > MAX_TIME) {
            cout << "*** TIME OUT! ReSend Message *** " << endl;
            start = clock();

            for (int i = 0; i < message_queue.size(); i++) {
                send_packet_GBN(message_queue.front(), SendSocket, RecvAddr);
                print_Send_information(message_queue.front(), "ReSend");
                message_queue.push(message_queue.front());
                message_queue.pop();
                // Sleep(10);
            }
        }

        // 循环之中也接收ACK消息，可封装
        if (recvfrom(SendSocket, RecvBuf, UDP_LEN, 0, (sockaddr*)&RecvAddr, &RecvAddrSize) > 0) {
            memcpy(&Recv_udp, RecvBuf, UDP_LEN);
            if (Recv_udp.udp_header.Flag == ACK && checksum((uint16_t*)&Recv_udp, UDP_LEN) == 0) {
                cout << "base: " << base << endl;
                cout << "nextseqnum " << next_seqnum << endl;
                // 丢弃重复响应的ACK，取模防止回环问题
                if ((base % DEFAULT_SEQNUM) == Recv_udp.udp_header.SEQ) {
                    base = base + 1; // 确认一个移动一个位置
                    cout << "Send has been confirmed! Flag:" << Recv_udp.udp_header.Flag;
                    cout << " STREAM_SEQ:" << Recv_udp.udp_header.STREAM_SEQ << " SEQ:" << Recv_udp.udp_header.SEQ << endl;
                    ACK_index++;
                    message_queue.pop();
                    start = clock();
                }
                else {
                    cout << "Repetitive ACK! Flag:" << Recv_udp.udp_header.Flag;
                    cout << " STREAM_SEQ:" << Recv_udp.udp_header.STREAM_SEQ << " SEQ:" << Recv_udp.udp_header.SEQ << endl;
                }
            }
            else;
        }
    }

    cout << "----- ***对方已成功接收文件！***----- " << endl << endl;
    stream_seq_order++;
    check_stream_seq();
    delete[] binary_file_buf;
}

void send_file(string filename, SOCKET& SendSocket, sockaddr_in& RecvAddr) {
    // 每次发送一个文件的时候，先把seq_order序列号清零
    // 每次完成一个文件的发送后，stream_seq_order + 1
    seq_order = 0;
    ifstream fin(filename.c_str(), ifstream::binary);

    // 获取文件大小
    fin.seekg(0, std::ifstream::end);
    long size = fin.tellg();
    file_size = size;
    fin.seekg(0);

    char* binary_file_buf = new char[size];

    cout << " ** 文件大小：" << size << " bytes" << endl;

    fin.read(&binary_file_buf[0], size);
    fin.close();

    // datasize cksum flag stream_seq seq
    // 第一个数据包要发送文件名，并且先只标记START
    HEADER udp_header(filename.length(), 0, START, stream_seq_order, seq_order);
    my_udp udp_packets(udp_header, filename.c_str());
    
    // 测试校验和 sizeof(HEADER): 8  sizeof(my_udp): 4104 = 4096 + 8
    uint16_t check = checksum((uint16_t*)&udp_packets, UDP_LEN); // 计算校验和
    udp_packets.udp_header.cksum = check;
    int packet_num = size / DEFAULT_BUFLEN + 1;

    cout << " ** 文件名校验和：" << check << endl;
    cout << " ** 发送数据包的数量：" << packet_num << endl;

    send_packet(udp_packets, SendSocket, RecvAddr);

    // 包含第一个文件名以及START标志，最后一个数据包带OVER标志，分包的问题
    for (int index = 0; index < packet_num; index++) {
        if (index == packet_num - 1) {
            udp_header.set_value(size - index * DEFAULT_BUFLEN, 0, OVER, stream_seq_order, seq_order); // seq序列号
            udp_packets.set_value(udp_header, binary_file_buf + index * DEFAULT_BUFLEN, size - index * DEFAULT_BUFLEN); // ??
            check = checksum((uint16_t*)&udp_packets, UDP_LEN);
            udp_packets.udp_header.cksum = check;
        }
        else {
            udp_header.set_value(DEFAULT_BUFLEN, 0, 0, stream_seq_order, seq_order);
            udp_packets.set_value(udp_header, binary_file_buf + index * DEFAULT_BUFLEN, DEFAULT_BUFLEN);
            check = checksum((uint16_t*)&udp_packets, UDP_LEN);
            udp_packets.udp_header.cksum = check;
        }

        // 增加测试部分，80%的概率按照正确的进行发送，20%的概率按照错误的进行发送
        int error_probability = rand() % 10;
        if (error_probability < 1) {
            cout << error_probability << endl;
            udp_packets.udp_header.SEQ++; // 手动添加错误
            send_packet(udp_packets, SendSocket, RecvAddr);
        }
        else {
            cout << error_probability << endl;
            send_packet(udp_packets, SendSocket, RecvAddr);
        }
        Sleep(10);

        // 图片二进制输出不了
        // cout << udp_packets.buffer << endl;
    }

    cout << "----- ***对方已成功接收文件！***----- " << endl << endl;
    stream_seq_order++;
    check_stream_seq();
    delete[] binary_file_buf;
}

// 三次握手建立连接，目前就先不发随机序列号，连接序列号先设为-1，16位SEQ也就是FFFF
bool Connect(SOCKET& SendSocket, sockaddr_in& RecvAddr) {
    HEADER udp_header;
    udp_header.set_value(0, 0, SYN, 0, 0xFFFF); // 初始的SEQ给0xFFFF
    my_udp first_connect(udp_header); // 第一次握手

    uint16_t temp = checksum((uint16_t*)&first_connect, UDP_LEN);
    first_connect.udp_header.cksum = temp;

    int iResult = 0;
    int RecvAddrSize = sizeof(RecvAddr);
    char* connect_buffer = new char[UDP_LEN];

    memcpy(connect_buffer, &first_connect, UDP_LEN);// 深拷贝准备发送
    iResult = sendto(SendSocket, connect_buffer, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, RecvAddrSize);
    if (iResult == SOCKET_ERROR) {
        cout << "-----*** 第一次握手Error，请重启Sender ***-----:(" << endl;
        return 0;
    }

    clock_t start = clock(); //记录发送第一次握手发出时间
    u_long mode = 1;
    ioctlsocket(SendSocket, FIONBIO, &mode); // 设置成阻塞模式等待ACK响应

    // 接收第二次握手ACK响应，其中的ACK应当为0xFFFF+1 = 0
    while (recvfrom(SendSocket, connect_buffer, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, &RecvAddrSize) <= 0) {
        // rdt3.0: 超时，重新传输第一次握手
        if (clock() - start > MAX_TIME) {
            cout << "-----*** 第一次握手超时，正在重传 ***-----:(" << endl;
            // memcpy(connect_buffer, &first_connect, UDP_LEN); // 这一句好像可以不用
            iResult = sendto(SendSocket, connect_buffer, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, RecvAddrSize);
            if (iResult == SOCKET_ERROR) {
                cout << "-----*** 第一次握手重传Error，请重启Sender ***-----:(" << endl;
                return 0;
            }
            start = clock(); // 重设时间
        }
    }

    cout << "-----*** 完成第一次握手 ***-----" << endl;

    // 获取到了第二次握手的ACK消息，检查校验和，这时接收到的在connect_buffer之中
    memcpy(&first_connect, connect_buffer, UDP_LEN);
    // 保存SYN_ACK的SEQ信息，完成k+1的验证
    uint16_t Recv_connect_Seq = 0x0;
    if (first_connect.udp_header.Flag == SYN_ACK && checksum((uint16_t*)&first_connect, UDP_LEN) == 0 && first_connect.udp_header.SEQ == 0xFFFF){
        Recv_connect_Seq = 0xFFFF; // first_connect.udp_header.SEQ
        cout << "-----*** 完成第二次握手 ***-----" << endl;
    }
    else{
        cout << "-----*** 第二次握手Error，请重启Sender ***-----:(" << endl;
        return 0;
    }

    // 第三次握手ACK，先清空缓冲区
    memset(&first_connect, 0, UDP_LEN);
    memset(connect_buffer, 0, UDP_LEN);
    first_connect.udp_header.Flag = ACK;
    first_connect.udp_header.SEQ = Recv_connect_Seq + 1; // 0
    first_connect.udp_header.cksum = checksum((uint16_t*)&first_connect, UDP_LEN);
    memcpy(connect_buffer, &first_connect, UDP_LEN);

    iResult = sendto(SendSocket, connect_buffer, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, RecvAddrSize);
    if (iResult == SOCKET_ERROR) {
        cout << "-----*** 第三次握手Error，请重启Sender ***-----:(" << endl;
        return 0;
    }

    cout << "-----*** 接收端成功连接！可以发送文件！***-----" << endl;

    delete[] connect_buffer;
    return 1;
}

bool disConnect(SOCKET& SendSocket, sockaddr_in& RecvAddr) {
    int iResult = 0;
    int RecvAddrSize = sizeof(RecvAddr);
    char* disconnect_buffer = new char[UDP_LEN];

    HEADER udp_header;
    udp_header.set_value(0, 0, FIN, 0, 0xFFFF); // 同样的四次挥手，这里的设计是序列号互相均发送0xFFFF
    my_udp last_connect(udp_header); // 第一次FIN
    uint16_t temp = checksum((uint16_t*)&last_connect, UDP_LEN);
    last_connect.udp_header.cksum = temp;

    memcpy(disconnect_buffer, &last_connect, UDP_LEN);// 深拷贝准备发送
    iResult = sendto(SendSocket, disconnect_buffer, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, RecvAddrSize);
    if (iResult == SOCKET_ERROR) {
        cout << "-----*** 第一次挥手Error，请重启Sender ***-----:(" << endl;
        return 0;
    }

    clock_t start = clock(); 
    u_long mode = 1;
    ioctlsocket(SendSocket, FIONBIO, &mode); // 设置成阻塞模式等待ACK响应

    // 接收第二次挥手
    while (recvfrom(SendSocket, disconnect_buffer, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, &RecvAddrSize) <= 0) {
        // rdt3.0: 超时重传
        if (clock() - start > MAX_TIME) {
            cout << "-----*** 第一次挥手超时，正在重传 ***-----:(" << endl;
            iResult = sendto(SendSocket, disconnect_buffer, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, RecvAddrSize);
            if (iResult == SOCKET_ERROR) {
                cout << "-----*** 第一次挥手Error，请重启Sender ***-----:(" << endl;
                return 0;
            }
            start = clock(); // 重设时间
        }
    }

    cout << "-----*** 完成第一次挥手 ***-----" << endl;
    cout << "-----*** 接收到第二次挥手消息，进行验证 ***-----" << endl;

    memcpy(&last_connect, disconnect_buffer, UDP_LEN);
    if (last_connect.udp_header.Flag == ACK && checksum((uint16_t*)&last_connect, UDP_LEN) == 0 && last_connect.udp_header.SEQ == 0x0) {
        cout << "-----*** 完成第二次挥手 ***-----" << endl;
    }
    else {
        cout << "-----*** 第二次挥手Error，请重启Sender ***-----:(" << endl;
        return 0;
    }

    // 第三次挥手等待接收消息
    iResult = recvfrom(SendSocket, disconnect_buffer, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, &RecvAddrSize);
    if (iResult == SOCKET_ERROR) {
        cout << "-----*** 第三次挥手Error，请重启Sender ***-----:(" << endl;
        return 0;
    }
    else {
        cout << "-----*** 接收到第三次挥手消息，进行验证 ***-----" << endl;
        memcpy(&last_connect, disconnect_buffer, UDP_LEN);
        if (last_connect.udp_header.Flag == FIN_ACK && checksum((uint16_t*)&last_connect, UDP_LEN) == 0 && last_connect.udp_header.SEQ == 0xFFFF) {
            cout << "-----*** 完成第三次挥手 ***-----" << endl;
        }
        else {
            cout << "-----*** 第三次挥手Error，请重启Sender ***-----:(" << endl;
            return 0;
        }
    }

    // 发送第四次挥手消息
    memset(&last_connect, 0, UDP_LEN);
    memset(disconnect_buffer, 0, UDP_LEN);
    last_connect.udp_header.Flag = ACK;
    last_connect.udp_header.SEQ = 0x0;
    last_connect.udp_header.cksum = checksum((uint16_t*)&last_connect, UDP_LEN);

    memcpy(disconnect_buffer, &last_connect, UDP_LEN);
    iResult = sendto(SendSocket, disconnect_buffer, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, RecvAddrSize);
    if (iResult == SOCKET_ERROR) {
        cout << "-----*** 第三次挥手Error，请重启Sender ***-----:(" << endl;
        return 0;
    }

    cout << "-----*** 完成第四次挥手 ***-----" << endl;
    return 1;
}

int main()
{

    int iResult;
    WSADATA wsaData;

    // 先初始化Socket的时候，初始化为Invalid
    SOCKET SendSocket = INVALID_SOCKET;
    sockaddr_in RecvAddr;

    //----------------------
    // 初始化WinSock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        cout << "WSAStartup failed with error: " << iResult << endl;
        return 1;
    }

    //---------------------------------------------
    // 创建发送socket来传输数据
    SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (SendSocket == INVALID_SOCKET) {
        cout << "Socket failed with error: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    //---------------------------------------------
    // 创建RecvAddr结构用本机IP和指定的端口号
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(DEFAULT_PORT);
    inet_pton(AF_INET, DEFAULT_IP, &RecvAddr.sin_addr.s_addr);

    //---------------------------------------------
    //建立连接
    if (Connect(SendSocket, RecvAddr)) {
        cout << "zzekun okk" << endl;
    }
    else {
        cout << "kkkkkkkkkk" << endl;
        return 0;
    }

    //---------------------------------------------
    // Send发送阶段，可以设计为双线程，send和recv都为双线程
    // HANDLE hThread[2];
    // hThread[0] = CreateThread(NULL, 0, Recv, (LPVOID)&ConnectSocket, 0, NULL);
    // hThread[1] = CreateThread(NULL, 0, Send, (LPVOID)&ConnectSocket, 0, NULL);
    cout << "******** You can use quit command to disconnect!!! ********" << endl;
    cout << "***********************************************************" << endl << endl;
    while (true) {
        string command;
        cout << "-----*** 请输入想要发送的文件名 ***-----" << endl;
        cin >> command;
        cout << endl;
        if (command == "quit") {
            HEADER command_header;
            command_header.set_value(4, 0, START_OVER, 0, 0);
            my_udp command_udp(command_header);
            char* command_buffer = new char[UDP_LEN];
            memcpy(command_buffer, &command_udp, UDP_LEN);

            if (sendto(SendSocket, command_buffer, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr)) == SOCKET_ERROR) {
                cout << "-----*** quit命令发送失败 ***-----" << endl;
                return 0;
            }
            else {
                cout << "-----*** quit命令发送成功 ***-----" << endl;
            }

            break;
        }
        else {
            clock_t start = clock();
            send_file_Reno(command, SendSocket, RecvAddr);
            clock_t end = clock();
            cout << "**传输文件时间为：" << (end - start) / CLOCKS_PER_SEC << "s" << endl;
            cout << "**吞吐率为:" << ((float)file_size) / ((end - start) / CLOCKS_PER_SEC) << " bytes/s " << endl << endl;
            continue;
        }
    }
    
    //---------------------------------------------
    // 四次挥手断连，FIN
    if (disConnect(SendSocket, RecvAddr)) {
        cout << "zzekun okk" << endl << endl;
    }
    else {
        cout << "kkkkkkkkkk" << endl << endl;
        return 0;
    }

    //---------------------------------------------
    // 当完成了相关的传输工作后，关闭socket，考虑可以补充选择文件传输
    iResult = closesocket(SendSocket);
    if (iResult == SOCKET_ERROR) {
        cout << "Closesocket failed with error: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    //---------------------------------------------
    // 清理退出
    cout << "Exiting..." << endl;
    WSACleanup();
    system("pause");
    return 0;
}