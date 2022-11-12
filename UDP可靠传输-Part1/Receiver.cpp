#include <winsock2.h>
#include <Ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
using namespace std;

#define WIN32_LEAN_AND_MEAN
#define DEFAULT_PORT 27015
#define DEFAULT_BUFLEN 4096
#define UDP_LEN sizeof(my_udp)
#define MAX_FILESIZE 1024 * 1024 * 10

#pragma comment(lib, "Ws2_32.lib")

const uint8_t START = 0x10;
const uint8_t OVER = 0x8;

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

// 目前都是在工作路径下操作，可以修改成“/测试文件/”
void recv_file(SOCKET& RecvSocket, sockaddr_in& SenderAddr, int& SenderAddrSize) {
    char* file_content = new char[MAX_FILESIZE]; // 偷懒了，直接调大
    string filename = "";
    long size = 0;
    int iResult = 0;
    bool flag = true;

    while (flag) {
        char* RecvBuf = new char[UDP_LEN]();
        my_udp temp;
        iResult = recvfrom(RecvSocket, RecvBuf, UDP_LEN, 0, (SOCKADDR*)&SenderAddr, &SenderAddrSize);
        if (iResult == SOCKET_ERROR) {
            cout << "Recvfrom failed with error: " << WSAGetLastError() << endl;
        }
        else {
            memcpy(&temp, RecvBuf, UDP_LEN);
            if (temp.udp_header.Flag == START) {
                cout << temp.udp_header.datasize << endl;
                filename = temp.buffer;
                cout << filename << endl;
            }
            else if (temp.udp_header.Flag == OVER) {
                // file_content = file_content + temp.buffer;
                memcpy(file_content + size, temp.buffer, temp.udp_header.datasize);
                size += temp.udp_header.datasize;
                // cout << file_content << endl;
                cout << temp.udp_header.SEQ << endl;
                cout << size << endl;
                ofstream fout(filename, ofstream::binary);
                fout.write(file_content, size); // 这里还是size,如果使用string.data或c_str的话图片不显示，经典深拷贝问题
                fout.close();
                flag = false;
            }
            else {
                // file_content = file_content + temp.buffer;
                memcpy(file_content + size, temp.buffer, temp.udp_header.datasize);
                size += temp.udp_header.datasize;
                cout << temp.udp_header.SEQ << endl;
                // cout << file_content << endl;
            }
        }

        delete[] RecvBuf; // 一定要delete掉啊，否则不给堆区了
    }
}

int main()
{

    int iResult;
    WSADATA wsaData;

    SOCKET RecvSocket;
    sockaddr_in RecvAddr;

    // 可以注释掉了
    char RecvBuf[UDP_LEN] = "";
    int BufLen = UDP_LEN;

    // 确定发送的addr_in
    sockaddr_in SenderAddr;
    int SenderAddrSize = sizeof(SenderAddr);

    //-----------------------------------------------
    // 初始化Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        cout << "WSAStartup failed with error: " << iResult << endl;
        return 1;
    }

    //-----------------------------------------------
    // 创建一个接收socket接收数据
    RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (RecvSocket == INVALID_SOCKET) {
        cout << "Socket failed with error: " << WSAGetLastError() << endl;
        return 1;
    }

    //-----------------------------------------------
    // Bind绑定好接收端的端口号
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(DEFAULT_PORT);
    RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    iResult = bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
    if (iResult != 0) {
        cout << "bind failed with error: " << WSAGetLastError() << endl;
        return 1;
    }

    //-----------------------------------------------
    // recvfrom接收socket上的数据，这里需要改成循环接收，等到发送端断开连接后关闭
    /*
    iResult = recvfrom(RecvSocket, RecvBuf, BufLen, 0, (SOCKADDR*)&SenderAddr, &SenderAddrSize);
    my_udp test;
    memcpy(&test, RecvBuf, UDP_LEN);
    cout << test.buffer << endl;
    */
    recv_file(RecvSocket, SenderAddr, SenderAddrSize);





    //-----------------------------------------------
    // 完成传输后，关闭socket
    iResult = closesocket(RecvSocket);
    if (iResult == SOCKET_ERROR) {
        cout << "Closesocket failed with error: " << WSAGetLastError() << endl;
        return 1;
    }

    //-----------------------------------------------
    // 清理退出
    cout << "Exiting." << endl;
    WSACleanup();
    return 0;
}