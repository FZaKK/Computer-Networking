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

// Ŀǰ�����ڹ���·���²����������޸ĳɡ�/�����ļ�/��
// ������
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

            // ����Ϊ����
            int drop_probability = rand() % 10;
            cout << drop_probability << endl;
            if (drop_probability < 1) {
                continue;
            }

            if (temp.udp_header.Flag == START) {
                // ��������
                if (checksum((uint16_t*)&temp, UDP_LEN) != 0 || temp.udp_header.SEQ != seq_order) {
                    cout << "*** Something wrong!! Wait ReSend!! *** " << endl;
                    Send_ACK(RecvSocket, SenderAddr, SenderAddrSize);
                    continue; // �����д���ֱ�Ӷ��������ݰ�
                }
                else {
                    filename = temp.buffer;
                    cout << "*** �ļ�����" << filename << endl;

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
                    continue; // �����д���ֱ�Ӷ��������ݰ�
                }
                else {
                    memcpy(file_content + size, temp.buffer, temp.udp_header.datasize);
                    size += temp.udp_header.datasize;

                    // cout << file_content << endl;
                    // cout << "���ݰ�SEQ��" << temp.udp_header.SEQ << endl;
                    print_Recv_information(temp);

                    ofstream fout(filename, ofstream::binary);
                    fout.write(file_content, size); // ���ﻹ��size,���ʹ��string.data��c_str�Ļ�ͼƬ����ʾ�������������
                    fout.close();
                    flag = false;

                    Send_ACK(RecvSocket, SenderAddr, SenderAddrSize);
                    // seq_order++; // һ���ļ������һ��
                    // check_seq();

                    cout << "*** �ļ���С��" << size << " bytes" << endl;
                    cout << "-----*** �ɹ������ļ� ***-----" << endl << endl;
                }
            }
            // START_OVER�������Ͷ˶���
            else if (temp.udp_header.Flag == START_OVER) {
                flag = false;
                ready2quit = 1; // ͵����ȫ�ֱ�����ʶ׼�������Ĵλ��֣�
                cout << "-----*** Sender���϶Ͽ����ӣ�***-----" << endl;
            }
            else {
                // ������Է�װһ��Send_ACK
                if (checksum((uint16_t*)&temp, UDP_LEN) != 0 || temp.udp_header.SEQ != seq_order) {
                    cout << "*** Something wrong!! Wait ReSend!! *** " << endl;
                    Send_ACK(RecvSocket, SenderAddr, SenderAddrSize);
                    continue; // �����д���ֱ�Ӷ��������ݰ�
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

        delete[] RecvBuf; // һ��Ҫdelete���������򲻸�������
    }

    stream_seq_order++;
    check_stream_seq();
    // ÿһ�λ�ȡ�ļ��󣬽�seq_order����
    seq_order = 0;
    delete[] file_content;
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
            cout << "-----*** ��һ�����ֽ���Error ***-----:(" << endl;
            return 0;
        }
        else {
            cout << "-----*** ���յ���һ��������Ϣ��������֤ ***-----" << endl;
        }
        memcpy(&first_connect, connect_buffer, UDP_LEN);

        // cout << first_connect.udp_header.Flag << " " << first_connect.udp_header.SEQ << " " << first_connect.udp_header.cksum << endl;
        // cout << first_connect.udp_header.datasize << " " << first_connect.udp_header.STREAM_SEQ << endl;
        // cout << first_connect.buffer << endl;
        // cout << checksum((uint16_t*)&first_connect, UDP_LEN) << endl;

        if (first_connect.udp_header.Flag == SYN && first_connect.udp_header.SEQ == 0xFFFF && checksum((uint16_t*)&first_connect, UDP_LEN) == 0) {
            cout << "-----*** ���յ�һ������ ***-----" << endl;
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
        cout << "-----*** �ڶ������ַ���Error ***-----:(" << endl;
        return 0;
    }
    clock_t start = clock(); // ��¼�ڶ������ַ���ʱ��

    // ���յ�����������Ϣ����ʱ�ش�
    while (recvfrom(RecvSocket, connect_buffer, UDP_LEN, 0, (sockaddr*)&SenderAddr, &SenderAddrSize) <= 0) {
        if (clock() - start > MAX_TIME) {
            cout << "-----*** �ڶ������ֳ�ʱ�������ش� ***-----" << endl;
            iResult = sendto(RecvSocket, connect_buffer, UDP_LEN, 0, (sockaddr*)&SenderAddr, SenderAddrSize);
            if (iResult == SOCKET_ERROR) {
                cout << "-----*** �ڶ������ַ���Error ***-----:(" << endl;
                return 0;
            }
            start = clock(); // ����ʱ��
        }
    }

    cout << "-----*** �ڶ������ֳɹ� ***-----" << endl;

    // memset(&first_connect, 0, UDP_LEN);
    memcpy(&first_connect, connect_buffer, UDP_LEN);

    if (first_connect.udp_header.Flag == ACK && checksum((uint16_t*)&first_connect, UDP_LEN) == 0 && first_connect.udp_header.SEQ == 0) {
        cout << "-----*** �ɹ�����ͨ�ţ����Խ����ļ�! ***-----" << endl;
    }
    else {
        cout << "-----*** ���ӷ���������ȴ����� ***-----:(" << endl;
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
    uint16_t last_Recv_Seq = 0x0; // ��Ȼûɶ�ã�������¼��+1������

    while (true) {
        iResult = recvfrom(RecvSocket, disconnect_buffer, UDP_LEN, 0, (sockaddr*)&SenderAddr, &SenderAddrSize);
        if (iResult == SOCKET_ERROR) {
            cout << "-----*** ��һ�λ��ֽ���Error ***-----:(" << endl;
            return 0;
        }
        else {
            cout << "-----*** ���յ���һ�λ�����Ϣ��������֤ ***-----" << endl;
        }
        memcpy(&last_connect, disconnect_buffer, UDP_LEN);
        if (last_connect.udp_header.Flag == FIN && last_connect.udp_header.SEQ == 0xFFFF && checksum((uint16_t*)&last_connect, UDP_LEN) == 0) {
            last_Recv_Seq = last_connect.udp_header.SEQ + 1;
            cout << "-----*** �ɹ����յ�һ�λ��� ***-----" << endl;
            break;
        }
    }

    // ���͵ڶ��λ���ACK��Ϣ
    memset(&last_connect, 0, UDP_LEN);
    memset(disconnect_buffer, 0, UDP_LEN);
    last_connect.udp_header.Flag = ACK;
    last_connect.udp_header.SEQ = last_Recv_Seq; // �ڶ��λ���SEQ��0x0
    last_connect.udp_header.cksum = checksum((uint16_t*)&last_connect, UDP_LEN);
    memcpy(disconnect_buffer, &last_connect, UDP_LEN);

    iResult = sendto(RecvSocket, disconnect_buffer, UDP_LEN, 0, (SOCKADDR*)&SenderAddr, SenderAddrSize);
    if (iResult == SOCKET_ERROR) {
        cout << "-----*** �ڶ��λ��ַ���Error ***-----:(" << endl;
        return 0;
    }

    // ���͵����λ��ֵ���Ϣ
    memset(&last_connect, 0, UDP_LEN);
    memset(disconnect_buffer, 0, UDP_LEN);
    last_connect.udp_header.Flag = FIN_ACK;
    last_connect.udp_header.SEQ = 0xFFFF; // �����λ���SEQ��0xFFFF
    last_connect.udp_header.cksum = checksum((uint16_t*)&last_connect, UDP_LEN);
    memcpy(disconnect_buffer, &last_connect, UDP_LEN);

    iResult = sendto(RecvSocket, disconnect_buffer, UDP_LEN, 0, (SOCKADDR*)&SenderAddr, SenderAddrSize);
    if (iResult == SOCKET_ERROR) {
        cout << "-----*** �����λ��ַ���Error ***-----:(" << endl;
        return 0;
    }

    // ���յ��ĴεĻ�����Ϣ
    while (true) {
        iResult = recvfrom(RecvSocket, disconnect_buffer, UDP_LEN, 0, (sockaddr*)&SenderAddr, &SenderAddrSize);
        if (iResult == SOCKET_ERROR) {
            cout << "-----*** ���Ĵλ��ֽ���Error ***-----:(" << endl;
            return 0;
        }
        else {
            cout << "-----*** ���յ����Ĵλ�����Ϣ��������֤ ***-----" << endl;
        }
        memcpy(&last_connect, disconnect_buffer, UDP_LEN);
        if (last_connect.udp_header.Flag == ACK && last_connect.udp_header.SEQ == 0x0 && checksum((uint16_t*)&last_connect, UDP_LEN) == 0) {
            cout << "-----*** �ɹ����յ��Ĵλ��� ***-----" << endl;
            break;
        }
    }

    cout << "-----*** �ɹ�����Ĵλ��ֹ��̣��Ͽ����ӣ�***-----" << endl;
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
        cout << "Bind failed with error: " << WSAGetLastError() << endl;
        return 1;
    }

    //-----------------------------------------------
    // ��Connect���ӳ���һ��
    cout << "** Waiting Connected..." << endl;
    if (Connect(RecvSocket, RecvAddr)) {
        cout << "zzekun okk" << endl << endl;
    }
    else {
        cout << "kkkkkkkkkk" << endl << endl;
        return 0;
    }

    //-----------------------------------------------
    // recvfrom����socket�ϵ����ݣ�������Ҫ�ĳ�ѭ�����գ��ȵ����Ͷ˶Ͽ����Ӻ�ر�
    // recv_file(RecvSocket, SenderAddr, SenderAddrSize);
    // ������Լ������Ӧ�ò�Э�����ݣ�˫�߳����
    while (true) {
        cout << "**** Log **** " << endl;
        if (ready2quit == 0) {
            cout << endl;
            recv_file(RecvSocket, SenderAddr, SenderAddrSize);
        }
        else {
            break; // �õ�quit��Ϣ���˳�ѭ�������Ĵζ�������
        }
    }

    //-----------------------------------------------
    // �Ĵλ��ֶ���
    if (disConnect(RecvSocket, RecvAddr)) {
        cout << "zzekun okk" << endl << endl;
    }
    else {
        cout << "kkkkkkkkkk" << endl << endl;
        return 0;
    }
    
    //-----------------------------------------------
    // ������ɺ󣬹ر�socket
    iResult = closesocket(RecvSocket);
    if (iResult == SOCKET_ERROR) {
        cout << "Closesocket failed with error: " << WSAGetLastError() << endl;
        return 1;
    }

    //-----------------------------------------------
    // �����˳�
    cout << "Exiting..." << endl;
    WSACleanup();
    return 0;
}