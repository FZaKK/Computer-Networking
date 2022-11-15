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
#define DEFAULT_SEQNUM 65536
#define UDP_LEN sizeof(my_udp)
#define MAX_FILESIZE 1024 * 1024 * 10
#define MAX_TIME 0.5 * CLOCKS_PER_SEC

#pragma comment(lib, "Ws2_32.lib")

const uint16_t SYN = 0x1;
const uint16_t ACK = 0x2;
const uint16_t SYN_ACK = 0x3;
const uint16_t START = 0x10;
const uint16_t OVER = 0x8;

struct HEADER {
    uint16_t datasize;
    uint16_t cksum;
    // ������Ҫע����ǣ�˳��ΪSTREAM SYN ACK FIN
    uint16_t Flag;
    uint16_t STREAM_SEQ;
    uint16_t SEQ;

    // ��ʼ��������STREAM��ǵ����ļ��Ŀ�ʼ�������Ӧ����Ӧ�ò���ƣ�����ͷŵ�udp���ʶ�ɣ�
    // ��ʼ��ʱ����Ҫ���ļ��������ȥ�����Ը���λ��־λ�ֱ�ΪSTART OVER
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
    char buffer[DEFAULT_BUFLEN] = ""; // �������+1����\0��Ҳ���Բ�+1
public:
    my_udp() {};
    my_udp(HEADER& header);
    my_udp(HEADER& header, string data_segment);
    void set_value(HEADER header, char* data_segment, int size); // ����һ��Ҫע��
};

// ����������ֺ��Ĵλ��ֵĳ�ʼ������
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

// ����У���
uint16_t checksum(uint16_t* udp, int size) {
    int count = (size + 1) / 2;
    uint16_t* buf = (uint16_t*)malloc(size); // ����+1Ҳ���Բ�+1
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

// Ŀǰ�����ڹ���·���²����������޸ĳɡ�/�����ļ�/��
void recv_file(SOCKET& RecvSocket, sockaddr_in& SenderAddr, int& SenderAddrSize) {
    char* file_content = new char[MAX_FILESIZE]; // ͵���ˣ�ֱ�ӵ���
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

            cout << "У��ͣ�" << temp.udp_header.cksum << endl;
            cout << "���飺" << checksum((uint16_t*)&temp, UDP_LEN) << endl;

            if (temp.udp_header.Flag == START) {
                filename = temp.buffer;

                cout << "�ļ�����" << filename << endl;
            }
            else if (temp.udp_header.Flag == OVER) {
                memcpy(file_content + size, temp.buffer, temp.udp_header.datasize);
                size += temp.udp_header.datasize;

                // cout << file_content << endl;
                cout << "���ݰ�SEQ��" << temp.udp_header.SEQ << endl;
                cout << "�ļ���С��" << size << endl;

                ofstream fout(filename, ofstream::binary);
                fout.write(file_content, size); // ���ﻹ��size,���ʹ��string.data��c_str�Ļ�ͼƬ����ʾ�������������
                fout.close();
                flag = false;
            }
            else {
                memcpy(file_content + size, temp.buffer, temp.udp_header.datasize);
                size += temp.udp_header.datasize;

                cout << "���ݰ�SEQ��" << temp.udp_header.SEQ << endl;
                // cout << file_content << endl;
            }
        }

        delete[] RecvBuf; // һ��Ҫdelete���������򲻸�������
    }

    cout << "-------�ɹ������ļ�-------" << endl;
}

bool Connect(SOCKET& RecvSocket, sockaddr_in& SenderAddr) {
    HEADER udp_header;
    my_udp first_connect;  // ��ʼ��

    int iResult = 0;
    int SenderAddrSize = sizeof(SenderAddr);
    char* connect_buffer = new char[UDP_LEN];

    // ���յ�һ������SYN
    while (true) {
        iResult = recvfrom(RecvSocket, connect_buffer, UDP_LEN, 0, (sockaddr*)&SenderAddr, &SenderAddrSize);
        if (iResult == SOCKET_ERROR) {
            cout << "-------��һ�����ֽ���ʧ��-------:(" << endl;
            return 0;
        }
        else {
            cout << "-------���յ���һ��������Ϣ��������֤-------" << endl;
        }
        memcpy(&first_connect, connect_buffer, UDP_LEN);

        // cout << first_connect.udp_header.Flag << " " << first_connect.udp_header.SEQ << " " << first_connect.udp_header.cksum << endl;
        // cout << first_connect.udp_header.datasize << " " << first_connect.udp_header.STREAM_SEQ << endl;
        // cout << first_connect.buffer << endl;
        // cout << checksum((uint16_t*)&first_connect, UDP_LEN) << endl;

        if (first_connect.udp_header.Flag == SYN && first_connect.udp_header.SEQ == 0xFFFF && checksum((uint16_t*)&first_connect, UDP_LEN) == 0) {
            cout << "-------�ɹ����յ�һ������-------" << endl;
            break;
        }
    }

    // ���͵ڶ���������Ϣ
    memset(&first_connect, 0, UDP_LEN);
    memset(connect_buffer, 0, UDP_LEN);
    first_connect.udp_header.Flag = SYN_ACK;
    first_connect.udp_header.SEQ = 0xFFFF; // �ڶ�������SEQ��0xFFFF
    first_connect.udp_header.cksum = checksum((uint16_t*)&first_connect, UDP_LEN);
    memcpy(connect_buffer, &first_connect, UDP_LEN);

    iResult = sendto(RecvSocket, connect_buffer, UDP_LEN, 0, (SOCKADDR*)&SenderAddr, SenderAddrSize);
    if (iResult == SOCKET_ERROR) {
        cout << "-------�ڶ������ַ���ʧ�ܣ����˳�-------:(" << endl;
        return 0;
    }
    clock_t start = clock(); // ��¼�ڶ������ַ���ʱ��

    // ���յ�����������Ϣ����ʱ�ش�
    while (recvfrom(RecvSocket, connect_buffer, UDP_LEN, 0, (sockaddr*)&SenderAddr, &SenderAddrSize) <= 0) {
        if (clock() - start > MAX_TIME) {
            cout << "-------�ڶ������ֳ�ʱ�������ش�-------" << endl;
            iResult = sendto(RecvSocket, connect_buffer, UDP_LEN, 0, (sockaddr*)&SenderAddr, SenderAddrSize);
            if (iResult == SOCKET_ERROR) {
                cout << "-------�ڶ������ַ���ʧ�ܣ����˳�-------:(" << endl;
                return 0;
            }
            start = clock(); // ����ʱ��
        }
    }

    cout << "-------�ڶ������ֳɹ�-------" << endl;

    // memset(&first_connect, 0, UDP_LEN);
    memcpy(&first_connect, connect_buffer, UDP_LEN);

    if (first_connect.udp_header.Flag == ACK && checksum((uint16_t*)&first_connect, UDP_LEN) == 0 && first_connect.udp_header.SEQ == 0) {
        cout << "-------�ɹ�����ͨ�ţ����Խ�������!-------" << endl;
    }
    else {
        cout << "-------���ӷ���������ȴ�����-------:(" << endl;
        return 0;
    }

    return 1;
}

int main()
{

    int iResult;
    WSADATA wsaData;

    SOCKET RecvSocket;
    sockaddr_in RecvAddr;

    // ȷ�����͵�addr_in
    sockaddr_in SenderAddr;
    int SenderAddrSize = sizeof(SenderAddr);

    //-----------------------------------------------
    // ��ʼ��Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        cout << "WSAStartup failed with error: " << iResult << endl;
        return 1;
    }

    //-----------------------------------------------
    // ����һ������socket��������
    RecvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (RecvSocket == INVALID_SOCKET) {
        cout << "Socket failed with error: " << WSAGetLastError() << endl;
        return 1;
    }

    //-----------------------------------------------
    // Bind�󶨺ý��ն˵Ķ˿ں�
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(DEFAULT_PORT);
    RecvAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    iResult = bind(RecvSocket, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
    if (iResult != 0) {
        cout << "bind failed with error: " << WSAGetLastError() << endl;
        return 1;
    }

    //-----------------------------------------------
    // ��Connect���ӳ���һ��
    cout << "Waiting Connected..." << endl;
    if (Connect(RecvSocket, RecvAddr)) {
        cout << "zzekun okk" << endl;
    }
    else {
        cout << "kkkkkkk" << endl;
        return 0;
    }

    //-----------------------------------------------
    // recvfrom����socket�ϵ����ݣ�������Ҫ�ĳ�ѭ�����գ��ȵ����Ͷ˶Ͽ����Ӻ�ر�
    // recv_file(RecvSocket, SenderAddr, SenderAddrSize);
    // ������Լ������Ӧ�ò�Э�����ݣ�˫�߳����
    while (true) {
        /*
        string command;
        cout << "-------��������quit�����˳��ͻ���-------" << endl; // �����Ĵλ���
        if (cin >> command && command == "quit") {
            break;
        }
        */

        cout << endl;
        recv_file(RecvSocket, SenderAddr, SenderAddrSize);
    }

    //-----------------------------------------------
    // �Ĵλ��ֶ���


    //-----------------------------------------------
    // ������ɺ󣬹ر�socket
    iResult = closesocket(RecvSocket);
    if (iResult == SOCKET_ERROR) {
        cout << "Closesocket failed with error: " << WSAGetLastError() << endl;
        return 1;
    }

    //-----------------------------------------------
    // �����˳�
    cout << "Exiting." << endl;
    WSACleanup();
    return 0;
}