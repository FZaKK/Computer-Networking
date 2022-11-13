#include <winsock2.h>
#include <Ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <windows.h>
#include <time.h>
using namespace std;

#define WIN32_LEAN_AND_MEAN
#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT 27015
#define DEFAULT_BUFLEN 4096 // 2^12大小
#define DEFAULT_SEQNUM 65536
#define UDP_LEN sizeof(my_udp)
#define MAX_FILESIZE 1024 * 1024 * 10
#define MAX_TIME 20

// 连接上ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

const uint16_t SYN = 0x1; // SYN = 1 ACK = 0 FIN = 0
const uint16_t ACK = 0x2; // SYN = 0, ACK = 1，FIN = 0
const uint16_t SYN_ACK = 0x3; // SYN = 1, ACK = 1
const uint16_t START = 0x10;  // 标记文件的第一个数据包，也就是文件名
const uint16_t OVER = 0x8;  // 标记最后一个数据包

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

void my_udp::set_value(HEADER header, char* data_segment, int size) {
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

// 返回值设成ERROR_CODE试试，要使用memcpy把整体发过去
void send_packet(my_udp& Packet, SOCKET& SendSocket, sockaddr_in& RecvAddr) {
    int iResult;
    char* SendBuf = new char[UDP_LEN];
    memcpy(SendBuf, &Packet, UDP_LEN);
    iResult = sendto(SendSocket, SendBuf, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));

    // 直接把提示信息也封装进函数吧
    if (iResult == SOCKET_ERROR) {
        cout << "Sendto failed with error: " << WSAGetLastError() << endl;
        closesocket(SendSocket);
        WSACleanup();
    }
}

void send_file(string filename, SOCKET& SendSocket, sockaddr_in& RecvAddr) {
    ifstream fin(filename.c_str(), ifstream::binary);

    // 获取文件大小
    fin.seekg(0, std::ifstream::end);
    long size = fin.tellg();
    fin.seekg(0);

    char* binary_file_buf = new char[size];

    cout << "文件大小：" << size << endl;

    fin.read(&binary_file_buf[0], size);
    fin.close();

    // 第一个数据包要发送文件名，并且先只标记START
    HEADER udp_header(filename.length(), 0, START, 0, 0);
    my_udp udp_packets(udp_header, filename.c_str());

    // 测试校验和 sizeof(HEADER): 8  sizeof(my_udp): 4104 = 4096 + 8
    uint16_t check = checksum((uint16_t*)&udp_packets, UDP_LEN); // 计算校验和

    cout << "校验和：" << check << endl;

    udp_packets.udp_header.cksum = check;
    send_packet(udp_packets, SendSocket, RecvAddr);

    int packet_num = size / DEFAULT_BUFLEN + 1;

    cout << "发送数据包的数量：" << packet_num << endl;

    // 包含第一个文件名以及START标志，最后一个数据包带OVER标志，分包的问题
    for (int index = 0; index < packet_num; index++) {
        if (index == packet_num - 1) {
            udp_header.set_value(size - index * DEFAULT_BUFLEN, 0, OVER, 0, (index + 1) % DEFAULT_SEQNUM); // index + 1: filename，取模
            udp_packets.set_value(udp_header, binary_file_buf + index * DEFAULT_BUFLEN, size - index * DEFAULT_BUFLEN); // ??

            check = checksum((uint16_t*)&udp_packets, UDP_LEN);
            udp_packets.udp_header.cksum = check;
        }
        else {

            udp_header.set_value(DEFAULT_BUFLEN, 0, 0, 0, (index + 1) % DEFAULT_SEQNUM);
            udp_packets.set_value(udp_header, binary_file_buf + index * DEFAULT_BUFLEN, DEFAULT_BUFLEN);

            check = checksum((uint16_t*)&udp_packets, UDP_LEN);
            udp_packets.udp_header.cksum = check;
        }
        send_packet(udp_packets, SendSocket, RecvAddr);
        Sleep(MAX_TIME);

        // 图片二进制输出不了
        // cout << udp_packets.buffer << endl;
    }

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
        cout << "-------第一次握手发生错误，请重启发送端-------:(" << endl;
        return 0;
    }

    clock_t start = clock(); //记录发送第一次握手发出时间
    u_long mode = 1;
    ioctlsocket(SendSocket, FIONBIO, &mode); // 设置成阻塞模式等待ACK相应

    // 接收第二次握手ACK响应，其中的ACK应当为0xFFFF+1 = 0
    while (recvfrom(SendSocket, connect_buffer, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, &RecvAddrSize) <= 0) {
        // rdt3.0: 超时，重新传输第一次握手
        if (clock() - start > MAX_TIME) {
            cout << "-------第一次握手超时，正在重传-------:(" << endl;
            // memcpy(connect_buffer, &first_connect, UDP_LEN); // 这一句好像可以不用
            iResult = sendto(SendSocket, connect_buffer, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, RecvAddrSize);
            if (iResult == SOCKET_ERROR) {
                cout << "-------第一次握手重传失败，正在重传-------:(" << endl;
            }
            start = clock(); // 重设时间
        }
    }

    // 获取到了第二次握手的ACK消息，检查校验和，这时接收到的在connect_buffer之中
    memcpy(&first_connect, connect_buffer, UDP_LEN);
    // 保存SYN_ACK的SEQ信息，完成k+1的验证
    uint16_t Recv_connect_Seq = 0x0;
    if (first_connect.udp_header.Flag == SYN_ACK && checksum((uint16_t*)&first_connect, UDP_LEN) == 0 && first_connect.udp_header.SEQ == 0xFFFF) {
        Recv_connect_Seq = 0xFFFF; // first_connect.udp_header.SEQ
        cout << "-------完成第二次握手-------" << endl;
    }
    else {
        cout << "-------第二次握手发生错误，请重启发送端-------:(" << endl;
        return 0;
    }

    // 第三次握手ACK，先清空缓冲区
    memset(&first_connect, 0, UDP_LEN);
    memset(connect_buffer, 0, UDP_LEN);
    first_connect.udp_header.Flag = ACK;
    first_connect.udp_header.SEQ = Recv_connect_Seq + 1; // 0
    first_connect.udp_header.cksum = checksum((uint16_t*)&first_connect, UDP_LEN);
    memcpy(&first_connect, connect_buffer, UDP_LEN);

    iResult = sendto(SendSocket, connect_buffer, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, RecvAddrSize);
    if (iResult == SOCKET_ERROR) {
        cout << "-------第三次握手发生错误，请重启发送端-------:(" << endl;
        return 0;
    }

    cout << "-------接收端成功连接！可以发送数据！-------" << endl;
    return 1;
}

int main()
{

    int iResult;
    WSADATA wsaData;

    // 先初始化Socket的时候，初始化为Invalid
    SOCKET SendSocket = INVALID_SOCKET;
    sockaddr_in RecvAddr;

    // 确定发送缓冲区的长度为1024
    // char SendBuf[DEFAULT_BUFLEN] = "zzekun";

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
    // 发送数据包，需要注意的是如果发送文件需要多次，需要改进，特征字符串断开连接
    // iResult = sendto(SendSocket, SendBuf, DEFAULT_BUFLEN, 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));

    string filename;
    cout << "请输入想要发送的文件路径：" << endl;
    cin >> filename;
    send_file(filename, SendSocket, RecvAddr);

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
    cout << "Exiting." << endl;
    WSACleanup();
    return 0;
}