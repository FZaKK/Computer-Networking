#include <winsock2.h>
#include <Ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <windows.h>
using namespace std;

#define WIN32_LEAN_AND_MEAN
#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT 27015
#define DEFAULT_BUFLEN 4096 // 2^12大小
#define DEFAULT_SEQNUM 65536
#define UDP_LEN sizeof(my_udp)

// 连接上ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

const uint8_t START = 0x10;  // 标记文件的第一个数据包，也就是文件名
const uint8_t OVER = 0x8;  // 标记最后一个数据包

struct HEADER {
    uint16_t datasize;
    uint16_t cksum;
    // 这里需要注意的是，顺序为STREAM SYN ACK FIN
    uint8_t Flag;
    uint8_t STREAM_SEQ;
    uint16_t SEQ;

    // 初始化函数，STREAM标记的是文件的开始与结束（应当在应用层设计，这里就放到udp里标识吧）
    // 开始的时候需要把文件名传输过去，所以给两位标志位分别为START OVER
    // 000 START OVER SYN ACK FIN
    HEADER() {
        this->datasize = 0;
        this->cksum = 0;
        this->Flag = 0;
        this->STREAM_SEQ = 0;
        this->SEQ = 0;
    }

    HEADER(uint16_t datasize, uint16_t cksum, uint8_t Flag, uint8_t STREAM_SEQ, uint16_t SEQ) {
        this->datasize = datasize;
        this->cksum = cksum;
        this->Flag = Flag;
        this->STREAM_SEQ = STREAM_SEQ;
        this->SEQ = SEQ;
    }

    void set_value(uint16_t datasize, uint16_t cksum, uint8_t Flag, uint8_t STREAM_SEQ, uint16_t SEQ) {
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
    char buffer[DEFAULT_BUFLEN + 1] = ""; // 虽然这里+1了，但是根本不影响4096的大小，因为\0不算strlen
public:
    my_udp() {};
    my_udp(HEADER& header);
    my_udp(HEADER& header, string data_segment);
    void set_value(HEADER header, char* data_segment, int size); // 这里一定要注意
    uint16_t checksum() {};
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
    cout << size << endl;
    fin.read(&binary_file_buf[0], size);
    fin.close();

    // 第一个数据包要发送文件名，并且先只标记START
    HEADER udp_header(filename.length(), 0, START, 0, 0);
    my_udp udp_packets(udp_header, filename.c_str());
    send_packet(udp_packets, SendSocket, RecvAddr);

    int packet_num = size / DEFAULT_BUFLEN + 1;
    cout << packet_num << endl;

    // 包含第一个文件名以及START标志，最后一个数据包带OVER标志，分包的问题
    for (int index = 0; index < packet_num; index++) {
        if (index == packet_num - 1) {
            // string tempdata(binary_file_buf, (index * DEFAULT_BUFLEN + 1), size); // tempdata就是截取char数组的一段
            udp_header.set_value(size - index * DEFAULT_BUFLEN, 0, OVER, 0, (index + 1) % DEFAULT_SEQNUM); // index + 1: filename，取模
            udp_packets.set_value(udp_header, binary_file_buf + index * DEFAULT_BUFLEN, size - index * DEFAULT_BUFLEN); // ??
        }
        else {
            // string tempdata(binary_file_buf, (index * DEFAULT_BUFLEN + 1), ((index + 1) * DEFAULT_BUFLEN));
            udp_header.set_value(DEFAULT_BUFLEN, 0, 0, 0, (index + 1) % DEFAULT_SEQNUM);
            udp_packets.set_value(udp_header, binary_file_buf + index * DEFAULT_BUFLEN, DEFAULT_BUFLEN);
        }
        send_packet(udp_packets, SendSocket, RecvAddr);
        Sleep(10);

        // 图片二进制输出不了
        // cout << udp_packets.buffer << endl;
    }

    delete[] binary_file_buf;
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
    /*
    HEADER test_header;
    string log = "zzekun";
    my_udp test(test_header, log);
    send_packet(test, SendSocket, RecvAddr);
    */
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