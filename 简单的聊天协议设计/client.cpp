#include<iostream>
#include<string>
#include<WinSock2.h>
#include<WS2tcpip.h>
#include<thread>
#pragma comment(lib,"ws2_32.lib")

#define DEFAULT_BUFLEN 100
#define DEFAULT_PORT 27015
//#define _WINSOCK_DEPRECATED_NO_WARNINGS 1  //VS2015�����þɺ���

using namespace std;
string quit_string = "quit";
int flag = 1;
char user_name[10]; //���ܷ���˷ַ����û���

DWORD WINAPI Recv(LPVOID lparam_socket) {
    int recvResult;
    SOCKET* recvSocket = (SOCKET*)lparam_socket; //һ��Ҫʹ��ָ���ͱ�������ΪҪָ��connect socket��λ��

    while (1) {
        char recvbuf[DEFAULT_BUFLEN] = "";
        recvResult = recv(*recvSocket, recvbuf, DEFAULT_BUFLEN, 0);
        if (recvResult > 0 && flag == 1) {
            SYSTEMTIME systime = { 0 };
            GetLocalTime(&systime);
            cout << endl << endl << systime.wYear << "��" << systime.wMonth << "��" << systime.wDay << "��";
            cout << systime.wHour << "ʱ" << systime.wMinute << "��" << systime.wSecond << "��" << endl;
            cout << "�յ���Ϣ��";
            cout << recvbuf << endl;
            cout << "-----------------------------------------------------" << endl;
            cout << "�����������Ϣ��"; // ��ʾ���Լ���������Ϣ��
        }
        else {
            closesocket(*recvSocket);
            return 1;
        }
    }
}

DWORD WINAPI Send(LPVOID lparam_socket) {

    // ������Ϣֱ��quit�˳�����
    // flagΪ�Ƿ��˳�����ı�־
    int sendResult;
    SOCKET* sendSocket = (SOCKET*)lparam_socket;

    while (1)
    {
        //----------------------
        // ������Ϣ
        char sendBuf[DEFAULT_BUFLEN] = "";
        cout << "�����������Ϣ��";
        cin.getline(sendBuf, DEFAULT_BUFLEN);   // ��֤��������ո�getline�������ú����Ի��з�Ϊ����

        if (string(sendBuf) == quit_string) {
            flag = 0;
            closesocket(*sendSocket);
            cout << endl << "�����˳�����" << endl;
            return 1;
        }
        else {
            sendResult = send(*sendSocket, sendBuf, DEFAULT_BUFLEN, 0);
            if (sendResult == SOCKET_ERROR) {
                cout << "send failed with error: " << WSAGetLastError() << endl;
                closesocket(*sendSocket);
                WSACleanup();
                return 1;
            }
            else {
                SYSTEMTIME systime = { 0 };
                GetLocalTime(&systime);
                cout << endl << endl << systime.wYear << "��" << systime.wMonth << "��" << systime.wDay << "��";
                cout << systime.wHour << "ʱ" << systime.wMinute << "��" << systime.wSecond << "��" << endl;
                cout << "��Ϣ�ѳɹ�����" << endl;
                cout << "-----------------------------------------------------" << endl;
            }
        }
    }
}


int main() {

    //----------------------
    //ʹ��iResult��ֵ���������������Ƿ�����ɹ�
    int iResult;
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;

    int recvbuflen = DEFAULT_BUFLEN;
    int sendbuflen = DEFAULT_BUFLEN;

    //----------------------
    // ��ʼ�� Winsock,�����Ϣ��ϸ����
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        cout << "WSAStartup failed with error: " << iResult << endl;
        return 1;
    }

    //----------------------
    // �ͻ��˴���SOCKET�ڴ������ӵ������
    ConnectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ConnectSocket == INVALID_SOCKET) {
        cout << "socket failed with error: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    //----------------------
    // ����sockaddr_in�ṹ����ת����SOCKADDR�Ľṹ
    // Ҫ���ӵķ���˵�IP��ַ���˿ں�
    struct sockaddr_in clientService;
    clientService.sin_family = AF_INET;
    inet_pton(AF_INET, "10.130.151.45", &clientService.sin_addr.s_addr);
    clientService.sin_port = htons(DEFAULT_PORT);

    //----------------------
    // Connect���ӵ������
    iResult = connect(ConnectSocket, (SOCKADDR*)&clientService, sizeof(clientService));
    if (iResult == SOCKET_ERROR) {
        cout << "connect failed with error: " << WSAGetLastError() << endl;
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    recv(ConnectSocket, user_name, 10, 0);

    // ��ӡ��������ı�־
    cout << "              Welcome    User    " << user_name << endl;
    cout << "*****************************************************" << endl;
    cout << "             Use quit command to quit" << endl;
    cout << "-----------------------------------------------------" << endl;

    //----------------------
    // ���������̣߳�һ�������̣߳�һ�������߳�
    HANDLE hThread[2];
    hThread[0] = CreateThread(NULL, 0, Recv, (LPVOID)&ConnectSocket, 0, NULL);
    hThread[1] = CreateThread(NULL, 0, Send, (LPVOID)&ConnectSocket, 0, NULL);

    WaitForMultipleObjects(2, hThread, TRUE, INFINITE);
    CloseHandle(hThread[0]);
    CloseHandle(hThread[1]);

    // �ر�socket
    iResult = closesocket(ConnectSocket);
    WSACleanup();
    return 0;
}
