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
#define DEFAULT_BUFLEN 4096 // 2^12��С
#define DEFAULT_SEQNUM 65536
#define UDP_LEN sizeof(my_udp)

// ������ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

const uint8_t START = 0x10;  // ����ļ��ĵ�һ�����ݰ���Ҳ�����ļ���
const uint8_t OVER = 0x8;  // ������һ�����ݰ�

struct HEADER {
    uint16_t datasize;
    uint16_t cksum;
    // ������Ҫע����ǣ�˳��ΪSTREAM SYN ACK FIN
    uint8_t Flag;
    uint8_t STREAM_SEQ;
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
    char buffer[DEFAULT_BUFLEN + 1] = ""; // ��Ȼ����+1�ˣ����Ǹ�����Ӱ��4096�Ĵ�С����Ϊ\0����strlen
public:
    my_udp() {};
    my_udp(HEADER& header);
    my_udp(HEADER& header, string data_segment);
    void set_value(HEADER header, char* data_segment, int size); // ����һ��Ҫע��
    uint16_t checksum() {};
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

// ����ֵ���ERROR_CODE���ԣ�Ҫʹ��memcpy�����巢��ȥ
void send_packet(my_udp& Packet, SOCKET& SendSocket, sockaddr_in& RecvAddr) {
    int iResult;
    char* SendBuf = new char[UDP_LEN];
    memcpy(SendBuf, &Packet, UDP_LEN);
    iResult = sendto(SendSocket, SendBuf, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));

    // ֱ�Ӱ���ʾ��ϢҲ��װ��������
    if (iResult == SOCKET_ERROR) {
        cout << "Sendto failed with error: " << WSAGetLastError() << endl;
        closesocket(SendSocket);
        WSACleanup();
    }
}

void send_file(string filename, SOCKET& SendSocket, sockaddr_in& RecvAddr) {
    ifstream fin(filename.c_str(), ifstream::binary);

    // ��ȡ�ļ���С
    fin.seekg(0, std::ifstream::end);
    long size = fin.tellg();
    fin.seekg(0);

    char* binary_file_buf = new char[size];
    cout << size << endl;
    fin.read(&binary_file_buf[0], size);
    fin.close();

    // ��һ�����ݰ�Ҫ�����ļ�����������ֻ���START
    HEADER udp_header(filename.length(), 0, START, 0, 0);
    my_udp udp_packets(udp_header, filename.c_str());
    send_packet(udp_packets, SendSocket, RecvAddr);

    int packet_num = size / DEFAULT_BUFLEN + 1;
    cout << packet_num << endl;

    // ������һ���ļ����Լ�START��־�����һ�����ݰ���OVER��־���ְ�������
    for (int index = 0; index < packet_num; index++) {
        if (index == packet_num - 1) {
            // string tempdata(binary_file_buf, (index * DEFAULT_BUFLEN + 1), size); // tempdata���ǽ�ȡchar�����һ��
            udp_header.set_value(size - index * DEFAULT_BUFLEN, 0, OVER, 0, (index + 1) % DEFAULT_SEQNUM); // index + 1: filename��ȡģ
            udp_packets.set_value(udp_header, binary_file_buf + index * DEFAULT_BUFLEN, size - index * DEFAULT_BUFLEN); // ??
        }
        else {
            // string tempdata(binary_file_buf, (index * DEFAULT_BUFLEN + 1), ((index + 1) * DEFAULT_BUFLEN));
            udp_header.set_value(DEFAULT_BUFLEN, 0, 0, 0, (index + 1) % DEFAULT_SEQNUM);
            udp_packets.set_value(udp_header, binary_file_buf + index * DEFAULT_BUFLEN, DEFAULT_BUFLEN);
        }
        send_packet(udp_packets, SendSocket, RecvAddr);
        Sleep(10);

        // ͼƬ�������������
        // cout << udp_packets.buffer << endl;
    }

    delete[] binary_file_buf;
}

int main()
{

    int iResult;
    WSADATA wsaData;

    // �ȳ�ʼ��Socket��ʱ�򣬳�ʼ��ΪInvalid
    SOCKET SendSocket = INVALID_SOCKET;
    sockaddr_in RecvAddr;

    // ȷ�����ͻ������ĳ���Ϊ1024
    // char SendBuf[DEFAULT_BUFLEN] = "zzekun";

    //----------------------
    // ��ʼ��WinSock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        cout << "WSAStartup failed with error: " << iResult << endl;
        return 1;
    }

    //---------------------------------------------
    // ��������socket����������
    SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (SendSocket == INVALID_SOCKET) {
        cout << "Socket failed with error: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    //---------------------------------------------
    // ����RecvAddr�ṹ�ñ���IP��ָ���Ķ˿ں�
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(DEFAULT_PORT);
    inet_pton(AF_INET, DEFAULT_IP, &RecvAddr.sin_addr.s_addr);

    //---------------------------------------------
    // �������ݰ�����Ҫע�������������ļ���Ҫ��Σ���Ҫ�Ľ��������ַ����Ͽ�����
    // iResult = sendto(SendSocket, SendBuf, DEFAULT_BUFLEN, 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
    /*
    HEADER test_header;
    string log = "zzekun";
    my_udp test(test_header, log);
    send_packet(test, SendSocket, RecvAddr);
    */
    string filename;
    cout << "��������Ҫ���͵��ļ�·����" << endl;
    cin >> filename;
    send_file(filename, SendSocket, RecvAddr);

    //---------------------------------------------
    // ���������صĴ��乤���󣬹ر�socket�����ǿ��Բ���ѡ���ļ�����
    iResult = closesocket(SendSocket);
    if (iResult == SOCKET_ERROR) {
        cout << "Closesocket failed with error: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    //---------------------------------------------
    // �����˳�
    cout << "Exiting." << endl;
    WSACleanup();
    return 0;
}