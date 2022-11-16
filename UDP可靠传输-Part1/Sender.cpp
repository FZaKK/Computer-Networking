#include <winsock2.h>
#include <Ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <string>
#include <Windows.h>
#include <time.h>
using namespace std;

#define WIN32_LEAN_AND_MEAN
#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT 27015
#define DEFAULT_BUFLEN 4096 // 2^12��С
#define DEFAULT_SEQNUM 65536
#define UDP_LEN sizeof(my_udp)
#define MAX_FILESIZE 1024 * 1024 * 10
#define MAX_TIME 0.5 * CLOCKS_PER_SEC // ��ʱ�ش�

// ������ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

const uint16_t SYN = 0x1; // SYN = 1 ACK = 0 FIN = 0
const uint16_t ACK = 0x2; // SYN = 0, ACK = 1��FIN = 0
const uint16_t SYN_ACK = 0x3; // SYN = 1, ACK = 1
const uint16_t START = 0x10;  // ����ļ��ĵ�һ�����ݰ���Ҳ�����ļ���
const uint16_t OVER = 0x8;  // ������һ�����ݰ�
const uint16_t FIN = 0x4; // FIN = 1 ACK = 0
const uint16_t FIN_ACK = 0x6; // FIN = 1 ACK = 1
const uint16_t START_OVER = 0x18; // START = 1 OVER = 1

uint16_t seq_order = 0;
uint16_t stream_seq_order = 0;

struct HEADER {
    uint16_t datasize;
    uint16_t cksum;
    // ������Ҫע����ǣ�˳��ΪSTREAM SYN ACK FIN
    uint16_t Flag; // ʹ��8λ�Ļ����ͺ��鷳��uint16_tȥָ��0x7+0x0�������0x7
    uint16_t STREAM_SEQ;
    uint16_t SEQ;

    // ��ʼ��������STREAM��ǵ����ļ��Ŀ�ʼ�������Ӧ����Ӧ�ò���ƣ�����ͷŵ�udp���ʶ�ɣ�
    // ��ʼ��ʱ����Ҫ���ļ��������ȥ�����Ը���λ��־λ�ֱ�ΪSTART OVER
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

void my_udp::set_value(HEADER header, char* data_segment , int size) {
    udp_header = header;
    memcpy(buffer, data_segment, size);
}

// ����У��ͣ�������ַ�����������̣����չ����windowsС�˴洢���ص�
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

// ����ȫ�ֱ��������к���û�г������ֵ
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

// ����ֵ���ERROR_CODE���ԣ�Ҫʹ��memcpy�����巢��ȥ
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
    
    // ����һ���ļ��ĸ�������
    cout << "Send Message " << Packet.udp_header.datasize << " bytes!";
    cout << " Flag:" << Packet.udp_header.Flag << " STREAM_SEQ:" << Packet.udp_header.STREAM_SEQ << " SEQ:" << Packet.udp_header.SEQ;
    cout << " Check Sum:" << Packet.udp_header.cksum << endl;

    // ��¼����ʱ�䣬��ʱ�ش�
    // �ȴ�����ACK��Ϣ����֤���к�
    clock_t start = clock(); 

    u_long mode = 1;
    ioctlsocket(SendSocket, FIONBIO, &mode);
    while (1 == 1) {
        while (recvfrom(SendSocket, RecvBuf, UDP_LEN, 0, (sockaddr*)&RecvAddr, &RecvAddrSize) <= 0) {
            if (clock() - start > MAX_TIME) {
                cout << " *** TIME OUT! ReSend Message *** " << endl;
                iResult = sendto(SendSocket, SendBuf, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
                start = clock(); // ���迪ʼʱ��
                if (iResult == SOCKET_ERROR) {
                    cout << "Sendto failed with error: " << WSAGetLastError() << endl;
                    closesocket(SendSocket);
                    WSACleanup();
                }
            }
        }

        // ��������Ҫͬʱ����
        memcpy(&Recv_udp, RecvBuf, UDP_LEN);
        if (Recv_udp.udp_header.SEQ == seq_order && Recv_udp.udp_header.Flag == ACK && checksum((uint16_t*)&Recv_udp, UDP_LEN) == 0) {
            cout << "Send has been confirmed! Flag:" << Recv_udp.udp_header.Flag << " STREAM_SEQ:" << Recv_udp.udp_header.STREAM_SEQ << " SEQ:" << Recv_udp.udp_header.SEQ << endl;
            seq_order++; // ȫ�ֱ��������к�
            check_seq();
            break;
        }
        else {
            continue;
        }
    }
    mode = 0;
    ioctlsocket(SendSocket, FIONBIO, &mode);//�Ļ�����ģʽ

    delete[] SendBuf; // ?
    delete[] RecvBuf;
}

// ������
void send_file(string filename, SOCKET& SendSocket, sockaddr_in& RecvAddr) {
    // ÿ�η���һ���ļ���ʱ���Ȱ�seq_order���к�����
    // ÿ�����һ���ļ��ķ��ͺ�stream_seq_order + 1
    seq_order = 0;
    ifstream fin(filename.c_str(), ifstream::binary);

    // ��ȡ�ļ���С
    fin.seekg(0, std::ifstream::end);
    long size = fin.tellg();
    fin.seekg(0);

    char* binary_file_buf = new char[size];

    cout << " ** �ļ���С��" << size << " bytes" << endl;

    fin.read(&binary_file_buf[0], size);
    fin.close();

    // datasize cksum flag stream_seq seq
    // ��һ�����ݰ�Ҫ�����ļ�����������ֻ���START
    HEADER udp_header(filename.length(), 0, START, stream_seq_order, seq_order);
    my_udp udp_packets(udp_header, filename.c_str());
    
    // ����У��� sizeof(HEADER): 8  sizeof(my_udp): 4104 = 4096 + 8
    uint16_t check = checksum((uint16_t*)&udp_packets, UDP_LEN); // ����У���
    udp_packets.udp_header.cksum = check;
    int packet_num = size / DEFAULT_BUFLEN + 1;

    cout << " ** �ļ���У��ͣ�" << check << endl;
    cout << " ** �������ݰ���������" << packet_num << endl;

    send_packet(udp_packets, SendSocket, RecvAddr);

    // ������һ���ļ����Լ�START��־�����һ�����ݰ���OVER��־���ְ�������
    for (int index = 0; index < packet_num; index++) {
        if (index == packet_num - 1) {
            udp_header.set_value(size - index * DEFAULT_BUFLEN, 0, OVER, stream_seq_order, seq_order); // seq���к�
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
        send_packet(udp_packets, SendSocket, RecvAddr);
        Sleep(10);

        // ͼƬ�������������
        // cout << udp_packets.buffer << endl;
    }

    cout << "----- ***�Է��ѳɹ������ļ���***----- " << endl << endl;
    stream_seq_order++;
    check_stream_seq();
    delete[] binary_file_buf;
}

// �������ֽ������ӣ�Ŀǰ���Ȳ���������кţ��������к�����Ϊ-1��16λSEQҲ����FFFF
bool Connect(SOCKET& SendSocket, sockaddr_in& RecvAddr) {
    HEADER udp_header;
    udp_header.set_value(0, 0, SYN, 0, 0xFFFF); // ��ʼ��SEQ��0xFFFF
    my_udp first_connect(udp_header); // ��һ������

    uint16_t temp = checksum((uint16_t*)&first_connect, UDP_LEN);
    first_connect.udp_header.cksum = temp;
    
    // cout << endl;
    // cout << first_connect.udp_header.Flag << " " << first_connect.udp_header.SEQ << " " << first_connect.udp_header.cksum << endl;
    // cout << first_connect.udp_header.datasize << " " << first_connect.udp_header.STREAM_SEQ << endl;
    // cout << first_connect.buffer << endl;
    // cout << checksum((uint16_t*)&first_connect, UDP_LEN) << endl;

    int iResult = 0;
    int RecvAddrSize = sizeof(RecvAddr);
    char* connect_buffer = new char[UDP_LEN];

    memcpy(connect_buffer, &first_connect, UDP_LEN);// ���׼������
    iResult = sendto(SendSocket, connect_buffer, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, RecvAddrSize);
    if (iResult == SOCKET_ERROR) {
        cout << "-----*** ��һ������Error��������Sender ***-----:(" << endl;
        return 0;
    }

    clock_t start = clock(); //��¼���͵�һ�����ַ���ʱ��
    u_long mode = 1;
    ioctlsocket(SendSocket, FIONBIO, &mode); // ���ó�����ģʽ�ȴ�ACK��Ӧ

    // ���յڶ�������ACK��Ӧ�����е�ACKӦ��Ϊ0xFFFF+1 = 0
    while (recvfrom(SendSocket, connect_buffer, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, &RecvAddrSize) <= 0) {
        // rdt3.0: ��ʱ�����´����һ������
        if (clock() - start > MAX_TIME) {
            cout << "-----*** ��һ�����ֳ�ʱ�������ش� ***-----:(" << endl;
            // memcpy(connect_buffer, &first_connect, UDP_LEN); // ��һ�������Բ���
            iResult = sendto(SendSocket, connect_buffer, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, RecvAddrSize);
            if (iResult == SOCKET_ERROR) {
                cout << "-----*** ��һ�������ش�Error��������Sender ***-----:(" << endl;
                return 0;
            }
            start = clock(); // ����ʱ��
        }
    }

    cout << "-----*** ��ɵ�һ������ ***-----" << endl;

    // ��ȡ���˵ڶ������ֵ�ACK��Ϣ�����У��ͣ���ʱ���յ�����connect_buffer֮��
    memcpy(&first_connect, connect_buffer, UDP_LEN);
    // ����SYN_ACK��SEQ��Ϣ�����k+1����֤
    uint16_t Recv_connect_Seq = 0x0;
    if (first_connect.udp_header.Flag == SYN_ACK && checksum((uint16_t*)&first_connect, UDP_LEN) == 0 && first_connect.udp_header.SEQ == 0xFFFF){
        Recv_connect_Seq = 0xFFFF; // first_connect.udp_header.SEQ
        cout << "-----*** ��ɵڶ������� ***-----" << endl;
    }
    else{
        cout << "-----*** �ڶ�������Error��������Sender ***-----:(" << endl;
        return 0;
    }

    // ����������ACK������ջ�����
    memset(&first_connect, 0, UDP_LEN);
    memset(connect_buffer, 0, UDP_LEN);
    first_connect.udp_header.Flag = ACK;
    first_connect.udp_header.SEQ = Recv_connect_Seq + 1; // 0
    first_connect.udp_header.cksum = checksum((uint16_t*)&first_connect, UDP_LEN);
    memcpy(connect_buffer, &first_connect, UDP_LEN);

    iResult = sendto(SendSocket, connect_buffer, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, RecvAddrSize);
    if (iResult == SOCKET_ERROR) {
        cout << "-----*** ����������Error��������Sender ***-----:(" << endl;
        return 0;
    }

    cout << "-----*** ���ն˳ɹ����ӣ����Է����ļ���***-----" << endl;

    delete[] connect_buffer;
    return 1;
}

bool disConnect(SOCKET& SendSocket, sockaddr_in& RecvAddr) {
    int iResult = 0;
    int RecvAddrSize = sizeof(RecvAddr);
    char* disconnect_buffer = new char[UDP_LEN];

    HEADER udp_header;
    udp_header.set_value(0, 0, FIN, 0, 0xFFFF); // ͬ�����Ĵλ��֣��������������кŻ��������0xFFFF
    my_udp last_connect(udp_header); // ��һ��FIN
    uint16_t temp = checksum((uint16_t*)&last_connect, UDP_LEN);
    last_connect.udp_header.cksum = temp;

    memcpy(disconnect_buffer, &last_connect, UDP_LEN);// ���׼������
    iResult = sendto(SendSocket, disconnect_buffer, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, RecvAddrSize);
    if (iResult == SOCKET_ERROR) {
        cout << "-----*** ��һ�λ���Error��������Sender ***-----:(" << endl;
        return 0;
    }

    clock_t start = clock(); 
    u_long mode = 1;
    ioctlsocket(SendSocket, FIONBIO, &mode); // ���ó�����ģʽ�ȴ�ACK��Ӧ

    // ���յڶ��λ���
    while (recvfrom(SendSocket, disconnect_buffer, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, &RecvAddrSize) <= 0) {
        // rdt3.0: ��ʱ�ش�
        if (clock() - start > MAX_TIME) {
            cout << "-----*** ��һ�λ��ֳ�ʱ�������ش� ***-----:(" << endl;
            iResult = sendto(SendSocket, disconnect_buffer, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, RecvAddrSize);
            if (iResult == SOCKET_ERROR) {
                cout << "-----*** ��һ�λ���Error��������Sender ***-----:(" << endl;
                return 0;
            }
            start = clock(); // ����ʱ��
        }
    }

    cout << "-----*** ��ɵ�һ�λ��� ***-----" << endl;
    cout << "-----*** ���յ��ڶ��λ�����Ϣ��������֤ ***-----" << endl;

    memcpy(&last_connect, disconnect_buffer, UDP_LEN);
    if (last_connect.udp_header.Flag == ACK && checksum((uint16_t*)&last_connect, UDP_LEN) == 0 && last_connect.udp_header.SEQ == 0x0) {
        cout << "-----*** ��ɵڶ��λ��� ***-----" << endl;
    }
    else {
        cout << "-----*** �ڶ��λ���Error��������Sender ***-----:(" << endl;
        return 0;
    }

    // �����λ��ֵȴ�������Ϣ
    iResult = recvfrom(SendSocket, disconnect_buffer, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, &RecvAddrSize);
    if (iResult == SOCKET_ERROR) {
        cout << "-----*** �����λ���Error��������Sender ***-----:(" << endl;
        return 0;
    }
    else {
        cout << "-----*** ���յ������λ�����Ϣ��������֤ ***-----" << endl;
        memcpy(&last_connect, disconnect_buffer, UDP_LEN);
        if (last_connect.udp_header.Flag == FIN_ACK && checksum((uint16_t*)&last_connect, UDP_LEN) == 0 && last_connect.udp_header.SEQ == 0xFFFF) {
            cout << "-----*** ��ɵ����λ��� ***-----" << endl;
        }
        else {
            cout << "-----*** �����λ���Error��������Sender ***-----:(" << endl;
            return 0;
        }
    }

    // ���͵��Ĵλ�����Ϣ
    memset(&last_connect, 0, UDP_LEN);
    memset(disconnect_buffer, 0, UDP_LEN);
    last_connect.udp_header.Flag = ACK;
    last_connect.udp_header.SEQ = 0x0;
    last_connect.udp_header.cksum = checksum((uint16_t*)&last_connect, UDP_LEN);

    memcpy(disconnect_buffer, &last_connect, UDP_LEN);
    iResult = sendto(SendSocket, disconnect_buffer, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, RecvAddrSize);
    if (iResult == SOCKET_ERROR) {
        cout << "-----*** �����λ���Error��������Sender ***-----:(" << endl;
        return 0;
    }

    cout << "-----*** ��ɵ��Ĵλ��� ***-----" << endl;
    return 1;
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

    //---------------------------------------------
    //��������
    if (Connect(SendSocket, RecvAddr)) {
        cout << "zzekun okk" << endl;
    }
    else {
        cout << "kkkkkkkkkk" << endl;
        return 0;
    }

    //---------------------------------------------
    // Send���ͽ׶Σ��������Ϊ˫�̣߳�send��recv��Ϊ˫�߳�
    // HANDLE hThread[2];
    // hThread[0] = CreateThread(NULL, 0, Recv, (LPVOID)&ConnectSocket, 0, NULL);
    // hThread[1] = CreateThread(NULL, 0, Send, (LPVOID)&ConnectSocket, 0, NULL);
    cout << "******** You can use quit command to disconnect!!! ********" << endl;
    cout << "***********************************************************" << endl << endl;
    while (true) {
        string command;
        cout << "-----*** ��������Ҫ���͵��ļ��� ***-----" << endl;
        cin >> command;
        cout << endl;
        if (command == "quit") {
            HEADER command_header;
            command_header.set_value(4, 0, START_OVER, 0, 0);
            my_udp command_udp(command_header);
            char* command_buffer = new char[UDP_LEN];
            memcpy(command_buffer, &command_udp, UDP_LEN);

            if (sendto(SendSocket, command_buffer, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr)) == SOCKET_ERROR) {
                cout << "-----*** quit�����ʧ�� ***-----" << endl;
                return 0;
            }
            else {
                cout << "-----*** quit����ͳɹ� ***-----" << endl;
            }

            break;
        }
        else {
            send_file(command, SendSocket, RecvAddr);
            continue;
        }
    }
    
    //---------------------------------------------
    // �Ĵλ��ֶ�����FIN
    if (disConnect(SendSocket, RecvAddr)) {
        cout << "zzekun okk" << endl << endl;
    }
    else {
        cout << "kkkkkkkkkk" << endl << endl;
        return 0;
    }

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
    cout << "Exiting..." << endl;
    WSACleanup();
    return 0;
}