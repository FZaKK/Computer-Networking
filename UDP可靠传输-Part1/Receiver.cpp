#include <winsock2.h>
#include <Ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <string>
#include <time.h>
using namespace std;

#define WIN32_LEAN_AND_MEAN
#define DEFAULT_PORT 27015
#define DEFAULT_BUFLEN 4096
#define DEFAULT_SEQNUM 65536
#define UDP_LEN sizeof(my_udp)
#define MAX_FILESIZE 1024 * 1024 * 10
#define MAX_TIME 0.2 * CLOCKS_PER_SEC

#pragma comment(lib, "Ws2_32.lib")

const uint16_t SYN = 0x1;
const uint16_t ACK = 0x2;
const uint16_t SYN_ACK = 0x3;
const uint16_t START = 0x10;  
const uint16_t OVER = 0x8;
const uint16_t FIN = 0x4; 
const uint16_t FIN_ACK = 0x6; 
const uint16_t START_OVER = 0x18;

int ready2quit = 0;
uint16_t seq_order = 0;
uint16_t stream_seq_order = 0;

struct HEADER {
    uint16_t datasize;
    uint16_t cksum;
    // 这里需要注意的是，顺序为STREAM SYN ACK FIN
    uint16_t Flag;
    uint16_t STREAM_SEQ;
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

// 检验校验和
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

void print_Recv_information(my_udp& udp2show) {
    cout << "Recv Message " << udp2show.udp_header.datasize << " bytes!";
    cout << " Flag:" << udp2show.udp_header.Flag << " STREAM_SEQ:" << udp2show.udp_header.STREAM_SEQ << " SEQ:" << udp2show.udp_header.SEQ;
    cout << " Check Sum:" << udp2show.udp_header.cksum << endl;
}

void Send_ACK(SOCKET& RecvSocket, sockaddr_in& SenderAddr, int& SenderAddrSize) {
    int iResult = 0;
    my_udp ACK_udp;
    char* SendBuf = new char[UDP_LEN]();

    ACK_udp.udp_header.set_value(0, 0, ACK, stream_seq_order, seq_order);
    uint16_t temp_sum = checksum((uint16_t*)&ACK_udp, UDP_LEN);
    ACK_udp.udp_header.cksum = temp_sum;

    memcpy(SendBuf, &ACK_udp, UDP_LEN);
    iResult = sendto(RecvSocket, SendBuf, UDP_LEN, 0, (sockaddr*)&SenderAddr, SenderAddrSize);
    if (iResult == SOCKET_ERROR) {
        cout << "Sendto failed with error: " << WSAGetLastError() << endl;
    }
    else {
        cout << "Send to Clinet ACK:" << ACK_udp.udp_header.Flag << " STREAM_SEQ:" << ACK_udp.udp_header.STREAM_SEQ << " SEQ:" << ACK_udp.udp_header.SEQ << endl;
    }
    delete[] SendBuf;
}

// 目前都是在工作路径下操作，可以修改成“/测试文件/”
// 待测试
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

            // 仅作为测试
            int drop_probability = rand() % 10;
            cout << drop_probability << endl;
            if (drop_probability < 1) {
                continue;
            }

            if (temp.udp_header.Flag == START) {
                // 出问题了
                if (checksum((uint16_t*)&temp, UDP_LEN) != 0 || temp.udp_header.SEQ != seq_order) {
                    cout << "*** Something wrong!! Wait ReSend!! *** " << endl;
                    Send_ACK(RecvSocket, SenderAddr, SenderAddrSize);
                    continue; // 不进行处理直接丢弃该数据包
                }
                else {
                    filename = temp.buffer;
                    cout << "*** 文件名：" << filename << endl;

                    print_Recv_information(temp);
                    Send_ACK(RecvSocket, SenderAddr, SenderAddrSize);
                    seq_order++;
                    check_seq();
                }
            }
            else if(temp.udp_header.Flag == OVER){
                if (checksum((uint16_t*)&temp, UDP_LEN) != 0 || temp.udp_header.SEQ != seq_order) { 
                    cout << "*** Something wrong!! Wait ReSend!! *** " << endl;
                    Send_ACK(RecvSocket, SenderAddr, SenderAddrSize);
                    continue; // 不进行处理直接丢弃该数据包
                }
                else {
                    memcpy(file_content + size, temp.buffer, temp.udp_header.datasize);
                    size += temp.udp_header.datasize;

                    // cout << file_content << endl;
                    // cout << "数据包SEQ：" << temp.udp_header.SEQ << endl;
                    print_Recv_information(temp);

                    ofstream fout(filename, ofstream::binary);
                    fout.write(file_content, size); // 这里还是size,如果使用string.data或c_str的话图片不显示，经典深拷贝问题
                    fout.close();
                    flag = false;

                    Send_ACK(RecvSocket, SenderAddr, SenderAddrSize);
                    // seq_order++; // 一个文件的最后一次
                    // check_seq();

                    cout << "*** 文件大小：" << size << " bytes" << endl;
                    cout << "-----*** 成功接收文件 ***-----" << endl << endl;
                }
            }
            // START_OVER表征发送端断连
            else if (temp.udp_header.Flag == START_OVER) {
                flag = false;
                ready2quit = 1; // 偷懒（全局变量标识准备进行四次挥手）
                cout << "-----*** Sender马上断开连接！***-----" << endl;
            }
            else {
                // 这里可以封装一个Send_ACK
                if (checksum((uint16_t*)&temp, UDP_LEN) != 0 || temp.udp_header.SEQ != seq_order) {
                    cout << "*** Something wrong!! Wait ReSend!! *** " << endl;
                    Send_ACK(RecvSocket, SenderAddr, SenderAddrSize);
                    continue; // 不进行处理直接丢弃该数据包
                }
                else {
                    memcpy(file_content + size, temp.buffer, temp.udp_header.datasize);
                    size += temp.udp_header.datasize;

                    print_Recv_information(temp);

                    Send_ACK(RecvSocket, SenderAddr, SenderAddrSize);
                    seq_order++;
                    check_seq();
                }
            }
        }

        delete[] RecvBuf; // 一定要delete掉啊，否则不给堆区了
    }

    stream_seq_order++;
    check_stream_seq();
    // 每一次获取文件后，将seq_order清零
    seq_order = 0;
    delete[] file_content;
}

bool Connect(SOCKET& RecvSocket, sockaddr_in& SenderAddr) {
    HEADER udp_header;
    my_udp first_connect;  // 初始化

    int iResult = 0;
    int SenderAddrSize = sizeof(SenderAddr);
    char* connect_buffer = new char[UDP_LEN];

    // 接收第一次握手SYN
    while (true) {
        iResult = recvfrom(RecvSocket, connect_buffer, UDP_LEN, 0, (sockaddr*)&SenderAddr, &SenderAddrSize);
        if (iResult == SOCKET_ERROR) {
            cout << "-----*** 第一次握手接收Error ***-----:(" << endl;
            return 0;
        }
        else {
            cout << "-----*** 接收到第一次握手消息，进行验证 ***-----" << endl;
        }
        memcpy(&first_connect, connect_buffer, UDP_LEN);

        // cout << first_connect.udp_header.Flag << " " << first_connect.udp_header.SEQ << " " << first_connect.udp_header.cksum << endl;
        // cout << first_connect.udp_header.datasize << " " << first_connect.udp_header.STREAM_SEQ << endl;
        // cout << first_connect.buffer << endl;
        // cout << checksum((uint16_t*)&first_connect, UDP_LEN) << endl;

        if (first_connect.udp_header.Flag == SYN && first_connect.udp_header.SEQ == 0xFFFF && checksum((uint16_t*)&first_connect, UDP_LEN) == 0) {
            cout << "-----*** 接收第一次握手 ***-----" << endl;
            break;
        }
    }

    // 发送第二次握手信息
    memset(&first_connect, 0, UDP_LEN);
    memset(connect_buffer, 0, UDP_LEN);
    first_connect.udp_header.Flag = SYN_ACK;
    first_connect.udp_header.SEQ = 0xFFFF; // 第二次握手SEQ：0xFFFF
    first_connect.udp_header.cksum = checksum((uint16_t*)&first_connect, UDP_LEN);
    memcpy(connect_buffer, &first_connect, UDP_LEN);

    iResult = sendto(RecvSocket, connect_buffer, UDP_LEN, 0, (SOCKADDR*)&SenderAddr, SenderAddrSize);
    if (iResult == SOCKET_ERROR) {
        cout << "-----*** 第二次握手发送Error ***-----:(" << endl;
        return 0;
    }
    clock_t start = clock(); // 记录第二次握手发送时间

    // 接收第三次握手消息，超时重传
    while (recvfrom(RecvSocket, connect_buffer, UDP_LEN, 0, (sockaddr*)&SenderAddr, &SenderAddrSize) <= 0) {
        if (clock() - start > MAX_TIME) {
            cout << "-----*** 第二次握手超时，正在重传 ***-----" << endl;
            iResult = sendto(RecvSocket, connect_buffer, UDP_LEN, 0, (sockaddr*)&SenderAddr, SenderAddrSize);
            if (iResult == SOCKET_ERROR) {
                cout << "-----*** 第二次握手发送Error ***-----:(" << endl;
                return 0;
            }
            start = clock(); // 重设时间
        }
    }

    cout << "-----*** 第二次握手成功 ***-----" << endl;

    // memset(&first_connect, 0, UDP_LEN);
    memcpy(&first_connect, connect_buffer, UDP_LEN);

    if (first_connect.udp_header.Flag == ACK && checksum((uint16_t*)&first_connect, UDP_LEN) == 0 && first_connect.udp_header.SEQ == 0) {
        cout << "-----*** 成功建立通信！可以接收文件! ***-----" << endl;
    }
    else {
        cout << "-----*** 连接发生错误！请等待重启 ***-----:(" << endl;
        return 0;
    }

    return 1;
}

bool disConnect(SOCKET& RecvSocket, sockaddr_in& SenderAddr) {
    HEADER udp_header;
    my_udp last_connect; 

    int iResult = 0;
    int SenderAddrSize = sizeof(SenderAddr);
    char* disconnect_buffer = new char[UDP_LEN];
    uint16_t last_Recv_Seq = 0x0; // 虽然没啥用，用来记录下+1的流程

    while (true) {
        iResult = recvfrom(RecvSocket, disconnect_buffer, UDP_LEN, 0, (sockaddr*)&SenderAddr, &SenderAddrSize);
        if (iResult == SOCKET_ERROR) {
            cout << "-----*** 第一次挥手接收Error ***-----:(" << endl;
            return 0;
        }
        else {
            cout << "-----*** 接收到第一次挥手消息，进行验证 ***-----" << endl;
        }
        memcpy(&last_connect, disconnect_buffer, UDP_LEN);
        if (last_connect.udp_header.Flag == FIN && last_connect.udp_header.SEQ == 0xFFFF && checksum((uint16_t*)&last_connect, UDP_LEN) == 0) {
            last_Recv_Seq = last_connect.udp_header.SEQ + 1;
            cout << "-----*** 成功接收第一次挥手 ***-----" << endl;
            break;
        }
    }

    // 发送第二次挥手ACK消息
    memset(&last_connect, 0, UDP_LEN);
    memset(disconnect_buffer, 0, UDP_LEN);
    last_connect.udp_header.Flag = ACK;
    last_connect.udp_header.SEQ = last_Recv_Seq; // 第二次挥手SEQ：0x0
    last_connect.udp_header.cksum = checksum((uint16_t*)&last_connect, UDP_LEN);
    memcpy(disconnect_buffer, &last_connect, UDP_LEN);

    iResult = sendto(RecvSocket, disconnect_buffer, UDP_LEN, 0, (SOCKADDR*)&SenderAddr, SenderAddrSize);
    if (iResult == SOCKET_ERROR) {
        cout << "-----*** 第二次挥手发送Error ***-----:(" << endl;
        return 0;
    }

    // 发送第三次挥手的消息
    memset(&last_connect, 0, UDP_LEN);
    memset(disconnect_buffer, 0, UDP_LEN);
    last_connect.udp_header.Flag = FIN_ACK;
    last_connect.udp_header.SEQ = 0xFFFF; // 第三次挥手SEQ：0xFFFF
    last_connect.udp_header.cksum = checksum((uint16_t*)&last_connect, UDP_LEN);
    memcpy(disconnect_buffer, &last_connect, UDP_LEN);

    iResult = sendto(RecvSocket, disconnect_buffer, UDP_LEN, 0, (SOCKADDR*)&SenderAddr, SenderAddrSize);
    if (iResult == SOCKET_ERROR) {
        cout << "-----*** 第三次挥手发送Error ***-----:(" << endl;
        return 0;
    }

    // 接收第四次的挥手消息
    while (true) {
        iResult = recvfrom(RecvSocket, disconnect_buffer, UDP_LEN, 0, (sockaddr*)&SenderAddr, &SenderAddrSize);
        if (iResult == SOCKET_ERROR) {
            cout << "-----*** 第四次挥手接收Error ***-----:(" << endl;
            return 0;
        }
        else {
            cout << "-----*** 接收到第四次挥手消息，进行验证 ***-----" << endl;
        }
        memcpy(&last_connect, disconnect_buffer, UDP_LEN);
        if (last_connect.udp_header.Flag == ACK && last_connect.udp_header.SEQ == 0x0 && checksum((uint16_t*)&last_connect, UDP_LEN) == 0) {
            cout << "-----*** 成功接收第四次挥手 ***-----" << endl;
            break;
        }
    }

    cout << "-----*** 成功完成四次挥手过程，断开连接！***-----" << endl;
    return 1;
}

int main()
{

    int iResult;
    WSADATA wsaData;

    SOCKET RecvSocket;
    sockaddr_in RecvAddr;

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
        cout << "Bind failed with error: " << WSAGetLastError() << endl;
        return 1;
    }

    //-----------------------------------------------
    // 先Connect连接尝试一下
    cout << "** Waiting Connected..." << endl;
    if (Connect(RecvSocket, RecvAddr)) {
        cout << "zzekun okk" << endl << endl;
    }
    else {
        cout << "kkkkkkkkkk" << endl << endl;
        return 0;
    }

    //-----------------------------------------------
    // recvfrom接收socket上的数据，这里需要改成循环接收，等到发送端断开连接后关闭
    // recv_file(RecvSocket, SenderAddr, SenderAddrSize);
    // 这里可以继续添加应用层协议内容，双线程设计
    while (true) {
        cout << "**** Log **** " << endl;
        if (ready2quit == 0) {
            cout << endl;
            recv_file(RecvSocket, SenderAddr, SenderAddrSize);
        }
        else {
            break; // 得到quit消息，退出循环进行四次断连过程
        }
    }

    //-----------------------------------------------
    // 四次挥手断连
    if (disConnect(RecvSocket, RecvAddr)) {
        cout << "zzekun okk" << endl << endl;
    }
    else {
        cout << "kkkkkkkkkk" << endl << endl;
        return 0;
    }
    
    //-----------------------------------------------
    // 断连完成后，关闭socket
    iResult = closesocket(RecvSocket);
    if (iResult == SOCKET_ERROR) {
        cout << "Closesocket failed with error: " << WSAGetLastError() << endl;
        return 1;
    }

    //-----------------------------------------------
    // 清理退出
    cout << "Exiting..." << endl;
    WSACleanup();
    return 0;
}