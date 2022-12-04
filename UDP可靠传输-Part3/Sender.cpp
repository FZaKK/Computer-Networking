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
#define DEFAULT_BUFLEN 4096 // 2^12��С
#define DEFAULT_SEQNUM 65536
#define UDP_LEN sizeof(my_udp)
#define MAX_FILESIZE 1024 * 1024 * 10
#define MAX_TIME 0.2 * CLOCKS_PER_SEC // ��ʱ�ش�

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
long file_size = 0;

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
    print_Send_information(Packet, "Send");

    // ��¼����ʱ�䣬��ʱ�ش�
    // �ȴ�����ACK��Ϣ����֤���к�
    clock_t start = clock();

    while (true) {
        while (recvfrom(SendSocket, RecvBuf, UDP_LEN, 0, (sockaddr*)&RecvAddr, &RecvAddrSize) <= 0) {
            if (clock() - start > MAX_TIME) {
                cout << "*** TIME OUT! ReSend Message *** " << endl;

                // ����Ϊ����
                Packet.udp_header.SEQ = seq_order;
                memcpy(SendBuf, &Packet, UDP_LEN);
                iResult = sendto(SendSocket, SendBuf, UDP_LEN, 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));

                print_Send_information(Packet, "ReSend");

                start = clock(); // ���迪ʼʱ��
                if (iResult == SOCKET_ERROR) {
                    cout << "Sendto failed with error: " << WSAGetLastError() << endl;
                }
            }
        }

        // ��������Ҫͬʱ���㣬�������ʱ�ж���packet��seq
        memcpy(&Recv_udp, RecvBuf, UDP_LEN);
        if (Recv_udp.udp_header.SEQ == Packet.udp_header.SEQ && Recv_udp.udp_header.Flag == ACK && checksum((uint16_t*)&Recv_udp, UDP_LEN) == 0) {
            cout << "Send has been confirmed! Flag:" << Recv_udp.udp_header.Flag << " STREAM_SEQ:" << Recv_udp.udp_header.STREAM_SEQ << " SEQ:" << Recv_udp.udp_header.SEQ << endl;
            seq_order++; // ȫ�ֱ��������к�
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

// GBN���ֵ�ȫ�ֱ���
int N = 1; // ���ڴ�С
uint16_t base = 1; // ��ʼΪ1
uint16_t next_seqnum = 1;
int ACK_index = 0; // �жϽ���
queue<my_udp> message_queue;

void GBN_init() {
    base = 1;
    next_seqnum = 1;
    ACK_index = 0;
    N = 5;
}

// Reno��һЩȫ�ֱ���
int cwnd = 1;
int recv_window = 2560; // ���ݽ��ն˵Ļ���������ã�ÿ�ν��յ�ACK -1
int ssthresh = 16; // ��ֵ
int dupACKcount = 0; // �ظ�ACK����
int RenoState = 0; // ��־״̬��  0�������� 1��ӵ������ 2�����ٻָ�
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

// ӵ�����Ƶ�Reno�����ļ�����
void send_file_Reno(string filename, SOCKET& SendSocket, sockaddr_in& RecvAddr) {
    // ÿ���ļ�����Ԥ�ȳ�ʼ��
    Reno_init();
    int RecvAddrSize = sizeof(RecvAddr);

    ifstream fin(filename.c_str(), ifstream::binary);
    fin.seekg(0, std::ifstream::end);
    long size = fin.tellg();
    file_size = size;
    fin.seekg(0);

    char* binary_file_buf = new char[size];
    cout << " ** �ļ���С��" << size << " bytes" << endl;
    fin.read(&binary_file_buf[0], size);
    fin.close();

    HEADER udp_header(filename.length(), 0, START, stream_seq_order, 0);
    my_udp udp_packets(udp_header, filename.c_str());
    uint16_t check = checksum((uint16_t*)&udp_packets, UDP_LEN); // ����У���
    udp_packets.udp_header.cksum = check;
    int packet_num = size / DEFAULT_BUFLEN + 1;
    cout << " ** �ļ���У��ͣ�" << check << endl;
    cout << " ** �������ݰ���������" << packet_num << endl;
    cout << " ** Windows���ڴ�С��" << N << endl;

    // �������͵�һ���ļ������ݰ�
    send_packet(udp_packets, SendSocket, RecvAddr);

    clock_t start; // ��ʱ�ش���ʱ��
    char* RecvBuf = new char[UDP_LEN];
    my_udp Recv_udp;
    start = clock();

    // ����SEQ�ػ���mod���㣬�̺�����
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

        // ��ʱ�ش���ʱ����ʱ�ˣ�Ҳ����˵�ܾ�û�յ�ACK���ݰ���
        if (clock() - start > MAX_TIME) {
            cout << "*** TIME OUT! ReSend Message *** " << endl;
            ssthresh = cwnd / 2; // ��ֵ
            cwnd = 1;
            dupACKcount = 0; // ���¼���
            N = min(cwnd, recv_window);
            RenoState = 0; // �������׶�
            cout << "*** �����������׶� ���ڴ�С����Ϊ" << N << " ***" << endl;
            start = clock();

            // �����ǳ�ʱ�ش���ȫ���ط� !!
            for (int i = 0; i < message_queue.size(); i++) {
                send_packet_GBN(message_queue.front(), SendSocket, RecvAddr);
                print_Send_information(message_queue.front(), "ReSend");
                message_queue.push(message_queue.front());
                message_queue.pop();
                // Sleep(10);
            }
        }

        // ѭ��֮��Ҳ����ACK��Ϣ���ɷ�װ
        if (recvfrom(SendSocket, RecvBuf, UDP_LEN, 0, (sockaddr*)&RecvAddr, &RecvAddrSize) > 0) {
            memcpy(&Recv_udp, RecvBuf, UDP_LEN);

            // û��У��͵Ĵ���
            if (Recv_udp.udp_header.Flag == ACK && checksum((uint16_t*)&Recv_udp, UDP_LEN) == 0) {
                switch (RenoState) {
                // ������
                case 0:
                    cout << "***** �������׶� *****" << endl;
                    cout << "Send Window Size: " << N << endl;
                    cout << "base: " << base << endl;
                    cout << "nextseqnum " << next_seqnum << endl;
                    // �����ظ���Ӧ��ACK��ȡģ��ֹ�ػ�����
                    if ((base % DEFAULT_SEQNUM) == Recv_udp.udp_header.SEQ) {
                        if (cwnd < ssthresh) {
                            cwnd++;
                            N = min(cwnd, recv_window); // ��Ҫ���·��ʹ��ڴ�С
                            cout << "*** �������׶ν��յ���ACK��cwnd++ ***" << endl;
                        }
                        else {
                            RenoState = 1;
                            cout << "*** �������׶ν���������ӵ������׶� ***" << endl;
                        }

                        dupACKcount = 0; // �����µ�ACK��dup=0
                        base = base + 1; // ȷ��һ���ƶ�һ��λ��
                        cout << "Send has been confirmed! Flag:" << Recv_udp.udp_header.Flag;
                        cout << " STREAM_SEQ:" << Recv_udp.udp_header.STREAM_SEQ << " SEQ:" << Recv_udp.udp_header.SEQ << endl;
                        recv_window--;
                        N = min(cwnd, recv_window);
                        ACK_index++;
                        message_queue.pop();
                        start = clock();
                    }
                    // �ظ���ACK
                    else {
                        dupACKcount++;
                        cout << "Repetitive ACK! Flag:" << Recv_udp.udp_header.Flag;
                        cout << " STREAM_SEQ:" << Recv_udp.udp_header.STREAM_SEQ << " SEQ:" << Recv_udp.udp_header.SEQ << endl;
                        cout << "dupACKcount: " << dupACKcount << endl;
                    }
                    // �ظ�����ACK
                    if (dupACKcount == 3) {
                        // ��⵽��������ʼ��ǰ�ش�
                        cout << "*** ACK�ظ����Σ���ʼ�ش� ***" << endl;
                        start = clock();
                        for (int i = 0; i < message_queue.size(); i++) {
                            send_packet_GBN(message_queue.front(), SendSocket, RecvAddr);
                            print_Send_information(message_queue.front(), "ReSend");
                            message_queue.push(message_queue.front());
                            message_queue.pop();
                            // Sleep(10);
                        }
                        // ���ٻָ��׶�
                        ssthresh = cwnd / 2;
                        cwnd = ssthresh + 3;
                        RenoState = 2; 
                        recover = message_queue.back().udp_header.SEQ;
                        dupACKcount = 0;
                        N = min(cwnd, recv_window); // ��Ҫ���·��ʹ��ڴ�С
                    }
                    break;
                
                // ӵ������׶�
                case 1:
                    cout << "***** ӵ������׶� *****" << endl;
                    cout << "Send Window Size: " << N << endl;
                    // cout << "RTT_ACK: " << RTT_ACK << endl;
                    cout << "base: " << base << endl;
                    cout << "nextseqnum " << next_seqnum << endl;
                    // �����ظ���Ӧ��ACK��ȡģ��ֹ�ػ�����
                    if ((base % DEFAULT_SEQNUM) == Recv_udp.udp_header.SEQ) {
                        RTT_ACK++;
                        // ��������cwnd���߸���ֵӦ�ö����ԣ�����cwnd��
                        if (RTT_ACK == cwnd) {
                            cwnd++;
                            N = min(cwnd, recv_window); // ��Ҫ���·��ʹ��ڴ�С
                            RTT_ACK = 0;
                            cout << "*** ӵ������׶Σ����Ե���+1 ***" << endl;
                        }

                        dupACKcount = 0; // �����µ�ACK��dup=0
                        base = base + 1; // ȷ��һ���ƶ�һ��λ��
                        cout << "Send has been confirmed! Flag:" << Recv_udp.udp_header.Flag;
                        cout << " STREAM_SEQ:" << Recv_udp.udp_header.STREAM_SEQ << " SEQ:" << Recv_udp.udp_header.SEQ << endl;
                        recv_window--;
                        N = min(cwnd, recv_window);
                        ACK_index++;
                        message_queue.pop();
                        start = clock();
                    }
                    // �ظ���ACK
                    else {
                        dupACKcount++;
                        RTT_ACK = 0; // ��֤RTT_ACKֵ����ȷ��
                        cout << "Repetitive ACK! Flag:" << Recv_udp.udp_header.Flag;
                        cout << " STREAM_SEQ:" << Recv_udp.udp_header.STREAM_SEQ << " SEQ:" << Recv_udp.udp_header.SEQ << endl;
                        cout << "dupACKcount: " << dupACKcount << endl;
                    }
                    if (dupACKcount == 3) {
                        // ��⵽��������ʼ��ǰ�ش�
                        cout << "*** ACK�ظ����Σ���ʼ�ش� ***" << endl;
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
                        RenoState = 2; // ���ٻָ��׶�
                        recover = message_queue.back().udp_header.SEQ; // ����New Reno�ĸ���
                        RTT_ACK = 0;
                        dupACKcount = 0;
                        N = min(cwnd, recv_window); // ��Ҫ���·��ʹ��ڴ�С
                    }
                    break;
                
                // ���ٻָ��׶�
                case 2: 
                    cout << "***** ���ٻָ��׶� *****" << endl;
                    cout << "Send Window Size: " << N << endl;
                    cout << "base: " << base << endl;
                    cout << "nextseqnum " << next_seqnum << endl;
                    // ����Ҫ����֮ǰ�����������к�
                    // uint16_t recover = next_seqnum - 1;
                    if ((base % DEFAULT_SEQNUM) == Recv_udp.udp_header.SEQ) {
                        // �µ�ACK��New Reno��ȥ�ж��Ƿ��͵���Ϣ���Ѿ���ȷ��
                        if (Recv_udp.udp_header.SEQ < recover) {
                            RenoState = 2; // ά�ֿ��ٻָ�
                        }
                        else {
                            RenoState = 1; // ����ӵ������׶�
                            cwnd = ssthresh;
                            N = min(cwnd, recv_window); // ��Ҫ���·��ʹ��ڴ�С
                            // dupACKcount = 0;
                        }

                        dupACKcount = 0;
                        base = base + 1; // ȷ��һ���ƶ�һ��λ��
                        cwnd++; // �ɹ����վ�������״̬����
                        cout << "Send has been confirmed! Flag:" << Recv_udp.udp_header.Flag;
                        cout << " STREAM_SEQ:" << Recv_udp.udp_header.STREAM_SEQ << " SEQ:" << Recv_udp.udp_header.SEQ << endl;
                        recv_window--;
                        N = min(cwnd, recv_window);
                        ACK_index++;
                        message_queue.pop();
                        start = clock();
                    }
                    else {
                        // Ҫ��������ס�ˣ�Ҫ��������ס�˾ͷ�����������(���У�
                        // Ҫ�ǲ������������Ļ����ᵼ�¿��ٻָ��׶�ʱ��������������ڹ���
                        dupACKcount++;
                        /*
                        // �������д�˼��
                        if (dupACKcount <= 3)
                            cwnd++;
                        else;
                        */
                        N = min(cwnd, recv_window); // ��Ҫ���·��ʹ��ڴ�С
                        cout << "*** ���ٻָ��׶Σ�cwnd: " << cwnd << endl;
                        cout << "Repetitive ACK! Flag:" << Recv_udp.udp_header.Flag;
                        cout << " STREAM_SEQ:" << Recv_udp.udp_header.STREAM_SEQ << " SEQ:" << Recv_udp.udp_header.SEQ << endl;
                    }
                    // �ظ�����ACK������һ��
                    if (dupACKcount == 6) {
                        // ��⵽���ٻָ��׶λ��ڶ�������ôֱ�ӽ���������
                        cout << "*** ACK�ظ����Σ����������� ***" << endl;
                        ssthresh = cwnd / 2; // ��ֵ
                        cwnd = 1;
                        dupACKcount = 0; // ���¼���
                        N = min(cwnd, recv_window);
                        RenoState = 0; // �������׶�
                        cout << "*** ���½����������׶� ���ڴ�С����Ϊ" << N << " ***" << endl;
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

    cout << "----- ***�Է��ѳɹ������ļ���***----- " << endl << endl;
    stream_seq_order++;
    check_stream_seq();
    delete[] binary_file_buf;
}

// GBN�ķ����ļ�����
void send_file_GBN(string filename, SOCKET& SendSocket, sockaddr_in& RecvAddr) {
    // ÿ���ļ�����Ԥ�ȳ�ʼ��
    GBN_init();
    int RecvAddrSize = sizeof(RecvAddr);

    ifstream fin(filename.c_str(), ifstream::binary);
    fin.seekg(0, std::ifstream::end);
    long size = fin.tellg();
    file_size = size;
    fin.seekg(0);

    char* binary_file_buf = new char[size];
    cout << " ** �ļ���С��" << size << " bytes" << endl;
    fin.read(&binary_file_buf[0], size);
    fin.close();

    HEADER udp_header(filename.length(), 0, START, stream_seq_order, 0);
    my_udp udp_packets(udp_header, filename.c_str());
    uint16_t check = checksum((uint16_t*)&udp_packets, UDP_LEN); // ����У���
    udp_packets.udp_header.cksum = check;

    int packet_num = size / DEFAULT_BUFLEN + 1;
    cout << " ** �ļ���У��ͣ�" << check << endl;
    cout << " ** �������ݰ���������" << packet_num << endl;
    cout << " ** Windows���ڴ�С��" << N << endl;
    
    // �������͵�һ���ļ������ݰ�
    send_packet(udp_packets, SendSocket, RecvAddr);

    clock_t start;
    char* RecvBuf = new char[UDP_LEN];
    my_udp Recv_udp;
    start = clock();

    // ����SEQ�ػ���mod���㣬�̺�����
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

        // ѭ��֮��Ҳ����ACK��Ϣ���ɷ�װ
        if (recvfrom(SendSocket, RecvBuf, UDP_LEN, 0, (sockaddr*)&RecvAddr, &RecvAddrSize) > 0) {
            memcpy(&Recv_udp, RecvBuf, UDP_LEN);
            if (Recv_udp.udp_header.Flag == ACK && checksum((uint16_t*)&Recv_udp, UDP_LEN) == 0) {
                cout << "base: " << base << endl;
                cout << "nextseqnum " << next_seqnum << endl;
                // �����ظ���Ӧ��ACK��ȡģ��ֹ�ػ�����
                if ((base % DEFAULT_SEQNUM) == Recv_udp.udp_header.SEQ) {
                    base = base + 1; // ȷ��һ���ƶ�һ��λ��
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

    cout << "----- ***�Է��ѳɹ������ļ���***----- " << endl << endl;
    stream_seq_order++;
    check_stream_seq();
    delete[] binary_file_buf;
}

void send_file(string filename, SOCKET& SendSocket, sockaddr_in& RecvAddr) {
    // ÿ�η���һ���ļ���ʱ���Ȱ�seq_order���к�����
    // ÿ�����һ���ļ��ķ��ͺ�stream_seq_order + 1
    seq_order = 0;
    ifstream fin(filename.c_str(), ifstream::binary);

    // ��ȡ�ļ���С
    fin.seekg(0, std::ifstream::end);
    long size = fin.tellg();
    file_size = size;
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

        // ���Ӳ��Բ��֣�80%�ĸ��ʰ�����ȷ�Ľ��з��ͣ�20%�ĸ��ʰ��մ���Ľ��з���
        int error_probability = rand() % 10;
        if (error_probability < 1) {
            cout << error_probability << endl;
            udp_packets.udp_header.SEQ++; // �ֶ���Ӵ���
            send_packet(udp_packets, SendSocket, RecvAddr);
        }
        else {
            cout << error_probability << endl;
            send_packet(udp_packets, SendSocket, RecvAddr);
        }
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
            clock_t start = clock();
            send_file_Reno(command, SendSocket, RecvAddr);
            clock_t end = clock();
            cout << "**�����ļ�ʱ��Ϊ��" << (end - start) / CLOCKS_PER_SEC << "s" << endl;
            cout << "**������Ϊ:" << ((float)file_size) / ((end - start) / CLOCKS_PER_SEC) << " bytes/s " << endl << endl;
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
    system("pause");
    return 0;
}